#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Convert the pinned CC0 MakeHuman base, rig and weights into RB_Humanoid_v1."""
import argparse
import hashlib
import json
import math
import shutil
import struct
from collections import defaultdict
from pathlib import Path

REVISION = "a8bc2d54ff0ac92e78ff71431b1023eda42bf482"
SOURCE = "https://raw.githubusercontent.com/makehumancommunity/makehuman/" + REVISION + "/"
INPUTS = {
    "makehuman/data/3dobjs/base.obj": "8e761e6624b8f54536409135d1636da63b32486a90d4897f84e121d144f6fb4c",
    "makehuman/data/rigs/default.mhskel": "99f179bce0aa850b45d4191a1d0d234c5851f881c057439470ded3bddf729a24",
    "makehuman/data/rigs/default_weights.mhw": "0f3641d651ae3d00ad6b4ccee43142edb109d3bd909d27d9e4139ef1beed8625",
    "LICENSE.ASSETS.md": "f6089cba01cb570a24712b41ab8a586ccd3cc5ef53dc266ca50b95c288956d2c",
}
CENTRAL = ["root", *(f"spine{i:02}" for i in range(1, 6)),
           "neck01", "neck02", "neck03", "head", "jaw"]
LIMBS = ["clavicle", "shoulder01", "upperarm01", "upperarm02", "lowerarm01",
         "lowerarm02", "wrist", "pelvis", "upperleg01", "upperleg02", "lowerleg01",
         "lowerleg02", "foot", "toe1-1", "eye", *(f"metacarpal{i}" for i in range(1, 5)),
         *(f"finger{i}-{j}" for i in range(1, 6) for j in range(1, 4))]
KEEP = set(CENTRAL) | {f"{name}.{side}" for name in LIMBS for side in "LR"}


def mesh(source):
    points, texcoords, triangles = [], [], []
    material = None
    for line in source:
        kind, *values = line.split()
        if kind == "v":
            points.append(tuple(map(float, values)))
        elif kind == "vt":
            texcoords.append(tuple(map(float, values[:2])))
        elif kind == "g":
            material = {"body": 0, "helper-l-eye": 1, "helper-r-eye": 1,
                        "helper-upper-teeth": 2, "helper-lower-teeth": 2}.get(values[0])
        elif kind == "f" and material is not None:
            face = [tuple(int(index)-1 for index in corner.split("/")[:2]) for corner in values]
            for end in range(2, len(face)):
                triangles.append((material, face[0], face[end-1], face[end]))
    return points, texcoords, triangles


def build(source, output):
    for path, expected in INPUTS.items():
        local = source / Path(path).name
        if hashlib.sha256(local.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Unexpected source revision: {local.name}")
    obj = (source / "base.obj").read_text(encoding="utf-8")
    points, texcoords, triangles = mesh(line for line in obj.splitlines()
                                       if line.strip() and not line.startswith("#"))
    rig = json.loads((source / "default.mhskel").read_bytes())
    weights = json.loads((source / "default_weights.mhw").read_bytes())
    if rig["license"] != "CC0" or weights["license"] != "CC0":
        raise ValueError("Source assets must be CC0")

    floor = min(points[point][1] for _, *face in triangles for point, _ in face)
    points = [(x, y-floor, z) for x, y, z in points]
    bones = rig["bones"]
    names = []

    def retained(name):
        while name is not None and name not in KEEP:
            name = bones[name]["parent"]
        return name

    def add(name):
        if name is None or name in names:
            return
        add(retained(bones[name]["parent"]))
        names.append(name)

    for name in sorted(KEEP):
        add(name)
    bone_ids = {name: index for index, name in enumerate(names)}
    influences = defaultdict(lambda: defaultdict(float))
    for name, entries in weights["weights"].items():
        joint = bone_ids[retained(name)]
        for index, weight in entries:
            influences[index][joint] += weight

    data = bytearray()
    for name in names:
        bone = bones[name]
        head = rig["joints"][bone["head"]]
        position = [math.fsum(points[index][axis] for index in head)/len(head)
                    for axis in range(3)]
        parent = bone_ids.get(retained(bone["parent"]), -1)
        data += struct.pack("<32si3f", name.encode("ascii"), parent, *position)
    rig_digest = hashlib.sha256(data).digest()

    vertices, indices = {}, []
    for material, *face in triangles:
        tri = []
        for point, texcoord in face:
            key = point, texcoord
            if key not in vertices:
                vertices[key] = len(vertices)
                strongest = sorted(influences[point].items(), key=lambda x: (-x[1], x[0]))[:4]
                if not strongest:
                    raise ValueError(f"Unweighted surface vertex {point}")
                total = sum(weight for _, weight in strongest)
                indices4 = [index for index, _ in strongest] + [0]*(4-len(strongest))
                weights4 = [weight/total for _, weight in strongest] + [0]*(4-len(strongest))
                data += struct.pack("<I3f2f4B4f", point, *points[point],
                                    *texcoords[texcoord], *indices4, *weights4)
            tri.append(vertices[key])
        indices.append(struct.pack("<4I", *tri, material))
    output.mkdir(parents=True, exist_ok=True)
    asset = (struct.pack("<4s4I32s", b"RBH1", 1, len(names), len(vertices), len(indices), rig_digest)
             + data + b"".join(indices))
    (output / "humanoid-v1.rbh").write_bytes(asset)
    shutil.copyfile(source / "LICENSE.ASSETS.md", output / "LICENSE-CC0.md")
    manifest = {"license": "CC0-1.0", "revision": REVISION, "rig": "RB_Humanoid_v1",
                "rigSha256": rig_digest.hex(),
                "sources": [{"url": SOURCE+name, "sha256": digest} for name, digest in INPUTS.items()],
                "files": {name: hashlib.sha256((output / name).read_bytes()).hexdigest()
                          for name in ("humanoid-v1.rbh", "LICENSE-CC0.md")}}
    (output / "provenance.json").write_text(json.dumps(manifest, indent=2)+"\n", encoding="utf-8")
    print(f"{len(names)} joints, {len(vertices)} skinned vertices, {len(indices)} triangles")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build(args.source, args.output)
