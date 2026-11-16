#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bake the adult MakeHuman body shape pair and matching joint offsets."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


REVISION = "a8bc2d54ff0ac92e78ff71431b1023eda42bf482"
PREFIX = f"https://raw.githubusercontent.com/makehumancommunity/makehuman/{REVISION}/"
BASE_FILES = {
    "base.obj": ("makehuman/data/3dobjs/base.obj", "8e761e6624b8f54536409135d1636da63b32486a90d4897f84e121d144f6fb4c"),
    "default.mhskel": ("makehuman/data/rigs/default.mhskel", "99f179bce0aa850b45d4191a1d0d234c5851f881c057439470ded3bddf729a24"),
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def smooth(t):
    t = min(1., max(0., t))
    return t*t*(3-2*t)


def bake(base, targets, rig_path, face_manifest, output):
    sources = {}
    data = {}
    for name, (path, expected) in BASE_FILES.items():
        raw = (base/name).read_bytes()
        if sha(raw) != expected:
            raise ValueError(f"Incompatible MakeHuman input: {name}")
        data[name] = raw
        sources[PREFIX+path] = expected
    points = [tuple(map(float, line.split()[1:4])) for line in data["base.obj"].decode("utf-8").splitlines()
              if line.startswith("v ")]
    rig = rig_path.read_bytes()
    magic, _, joint_count, vertex_count = struct.unpack_from("<4s3I", rig)
    if magic != b"RBH1":
        raise ValueError("Expected a canonical RBH1 rig")
    joint_end = 52 + 48*joint_count
    source_id, _, body_y, _ = struct.unpack_from("<I3f", rig, joint_end)
    floor = points[source_id][1]-body_y
    points = [(x, y-floor, z) for x,y,z in points]

    offsets = [[0.,0.,0.] for _ in points]
    manifest = json.loads(face_manifest.read_bytes())
    for url, digest in manifest["sources"].items():
        name = url.rsplit("/",1)[1]
        raw = (targets/name).read_bytes()
        if sha(raw) != digest or b"explicitly released as CC0" not in raw:
            raise ValueError(f"Incompatible MakeHuman target: {name}")
        sources[url] = digest
        sign = 1/6 if "-male-" in name else -1/6
        for line in raw.decode().splitlines():
            if line and not line.startswith("#"):
                idx, *values = line.split()
                for axis, value in enumerate(values):
                    offsets[int(idx)][axis] += float(value)*sign

    def body_delta(point, offset):
        y = point[1]
        neck = 1-smooth((y-13.68)/.92)
        # Preserve the canonical ground and head height. Joint and volume
        # differences remain within those anchors, including pelvis and rib cage.
        vertical = (offset[1]-.040*y)*smooth(y/.65)
        return neck*offset[0], neck*vertical, neck*offset[2]

    rows = [struct.pack("<6f", *point, *body_delta(point, offset))
            for point, offset in zip(points, offsets)]
    skeleton = json.loads(data["default.mhskel"])
    for start in range(52, joint_end, 48):
        name = rig[start:start+32].split(b"\0",1)[0].decode("ascii")
        ids = skeleton["joints"][skeleton["bones"][name]["head"]]
        point = struct.unpack_from("<3f", rig, start+36)
        raw = [sum(offsets[idx][axis] for idx in ids)/len(ids) for axis in range(3)]
        rows.append(struct.pack("<3f", *body_delta(point, raw)))

    asset = output/"adult-body-shapes.rbs"
    asset.write_bytes(struct.pack("<4sIII32s", b"RBS1", 1, len(points), joint_count, rig[20:52])
                      +b"".join(rows))
    (output/"body-shapes-provenance.json").write_text(json.dumps({
        "license": "CC0-1.0", "sources": sources, "files": {asset.name: sha(asset.read_bytes())},
    }, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for item in ("base", "targets", "rig", "face_manifest", "output"):
        parser.add_argument(item, type=Path)
    args = parser.parse_args()
    bake(args.base, args.targets, args.rig, args.face_manifest, args.output)
