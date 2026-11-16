#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Convert CC0 system wardrobe meshes and their barycentric anchors to RBW1."""
import argparse
import hashlib
import json
import math
import struct
from collections import defaultdict
from pathlib import Path

PACK = ("https://files2.makehumancommunity.org/asset_packs/"
        "makehuman_system_assets/makehuman_system_assets_cc0.zip")
SLOTS = {
    "clothes": [
        "female_casualsuit01", "female_casualsuit02", "female_elegantsuit01", "female_sportsuit01",
        "male_casualsuit01", "male_casualsuit02", "male_casualsuit03", "male_casualsuit04",
        "male_casualsuit05", "male_casualsuit06", "male_elegantsuit01", "male_worksuit01",
        "shoes02", "shoes03", "shoes04", "fedora01",
    ],
    "hair": ["afro01", "bob01", "braid01", "long01", "ponytail01", "short01",
             "short02", "short03", "short04", "bob02"],
    "eyebrows": ["eyebrow001", "eyebrow010", "eyebrow012"],
    "eyelashes": ["eyelashes01"],
}

HAIR_DIFFUSE = {"afro01": "afro_diffuse.png"}


def visible_hair_faces(folder, identifier, uv, faces, pinned, sources, upstream):
    from PIL import Image
    atlas = folder / HAIR_DIFFUSE.get(identifier, identifier + "_diffuse.png")
    key = atlas.relative_to(upstream).as_posix()
    actual = hashlib.sha256(atlas.read_bytes()).hexdigest()
    if actual != pinned[key]:
        raise ValueError("Hair atlas differs from pinned input: " + key)
    sources[key] = actual
    with Image.open(atlas) as image:
        alpha = image.convert("RGBA").resize((1024, 1024), Image.Resampling.LANCZOS).getchannel("A")
    size = alpha.width

    def visible(face):
        u, v = zip(*(uv[tex] for _, tex in face))
        # Test the entire UV rectangle with a filtering margin so triangles
        # at the edge of each alpha island survive.
        lo_x, hi_x = math.floor(min(u)*size)-8, math.ceil(max(u)*size)+8
        lo_y, hi_y = math.floor((1-max(v))*size)-8, math.ceil((1-min(v))*size)+8
        box = (max(0, lo_x), max(0, lo_y), min(size, hi_x), min(size, hi_y))
        if box[0]>=box[2] or box[1]>=box[3]:
            return True
        return alpha.crop(box).getextrema()[1]>=12

    return [face for face in faces if visible(face)]


def read_obj(text):
    points, uv, faces = [], [], []
    for line in text.splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "v":
            points.append(tuple(map(float, fields[1:4])))
        elif fields[0] == "vt":
            uv.append(tuple(map(float, fields[1:3])))
        elif fields[0] == "f":
            corners = [tuple(int(x) - 1 for x in token.split("/")[:2]) for token in fields[1:]]
            faces.extend((corners[0], corners[i-1], corners[i]) for i in range(2, len(corners)))
    return points, uv, faces


def mapping(text, count):
    if "explicitly released as CC0" not in text:
        raise ValueError("Missing CC0 asset notice")
    section = None
    anchors, hidden, scales = [], [], {}
    for line in text.splitlines():
        fields = line.split()
        if not fields or fields[0].startswith("#"):
            continue
        if fields[0] == "verts":
            section = "anchors"
        elif fields[0] in ("x_scale", "y_scale", "z_scale"):
            scales[fields[0][0]] = (int(fields[1]),int(fields[2]),float(fields[3]))
        elif fields[0] == "delete_verts":
            section = "hidden"
        elif fields[0].isdigit():
            if section == "anchors":
                if len(fields) == 1:
                    anchors.append(((int(fields[0]),)*3, (1.,0.,0.), (0.,0.,0.)))
                elif len(fields) == 9:
                    anchors.append((tuple(map(int,fields[:3])), tuple(map(float,fields[3:6])),
                                    tuple(map(float,fields[6:9]))))
                else:
                    raise ValueError("Invalid wardrobe binding")
            elif section == "hidden":
                cursor = 0
                while cursor < len(fields):
                    first = int(fields[cursor])
                    cursor += 1
                    if cursor < len(fields) and fields[cursor] == "-":
                        hidden.extend(range(first,int(fields[cursor+1])+1))
                        cursor += 2
                    else:
                        hidden.append(first)
    if len(anchors) != count:
        raise ValueError("Wardrobe anchors do not match source OBJ")
    return anchors, sorted(set(hidden)), scales


def build(source, upstream, humanoid, output):
    base, _, _ = read_obj((source / "base.obj").read_text(encoding="utf-8"))
    rig = humanoid.read_bytes()
    joint_count, vertex_count, _ = struct.unpack_from("<III", rig, 8)
    vertex_offset = 52 + 48 * joint_count
    order = [rig[52+i*48:52+i*48+32].split(b"\0",1)[0].decode() for i in range(joint_count)]
    floor = next(base[s][1]-y for s,_,y,_,*rest in struct.iter_unpack(
        "<I3f2f4B4f",rig[vertex_offset:vertex_offset+vertex_count*44]))
    base = [(x,y-floor,z) for x,y,z in base]
    original = json.loads((source / "default.mhskel").read_bytes())["bones"]
    weights = json.loads((source / "default_weights.mhw").read_bytes())["weights"]
    source_influences = defaultdict(lambda: defaultdict(float))
    for name, entries in weights.items():
        while name not in order:
            name = original[name]["parent"]
        joint = order.index(name)
        for point, weight in entries:
            source_influences[point][joint] += weight

    pinned = json.loads((upstream / "sha256.json").read_bytes())
    provenance = []
    output.mkdir(parents=True,exist_ok=True)
    for category, assets in SLOTS.items():
        for identifier in assets:
            folder = upstream / category / identifier
            binding = folder / (identifier + ".mhclo")
            text = binding.read_text(encoding="utf-8")
            obj_name = next(line.split()[1] for line in text.splitlines() if line.startswith("obj_file "))
            geometry = folder / obj_name
            sources = {}
            for path in (binding,geometry):
                key = path.relative_to(upstream).as_posix()
                actual = hashlib.sha256(path.read_bytes()).hexdigest()
                if actual != pinned[key]:
                    raise ValueError("Wardrobe source differs from pinned input: " + key)
                sources[key] = actual
            points,uv,faces = read_obj(geometry.read_text(encoding="utf-8"))
            if category == "hair":
                faces = visible_hair_faces(folder,identifier,uv,faces,pinned,sources,upstream)
            anchors,hidden,scales = mapping(text,len(points))
            ratio = [1.,1.,1.]
            for axis, key in enumerate("xyz"):
                if key in scales:
                    first,last,reference = scales[key]
                    ratio[axis] = abs(base[first][axis]-base[last][axis])/reference
            vertices, triangles = {}, []
            for face in faces:
                indices = []
                for point, tex in face:
                    key = (point, tex)
                    if key not in vertices:
                        ids, coefficients, offsets = anchors[point]
                        total = math.fsum(coefficients)
                        coefficients = [value / total for value in coefficients]
                        influences = defaultdict(float)
                        for i, coeff in zip(ids,coefficients):
                            if coeff > 0:
                                for bone, weight in source_influences[i].items():
                                    influences[bone] += weight * coeff
                        strongest = sorted(influences.items(),key=lambda a:(-a[1],a[0]))[:4]
                        weight_sum = math.fsum(weight for _,weight in strongest)
                        skin = [weight/weight_sum for _,weight in strongest]
                        bones = [bone for bone,_ in strongest]
                        origin = [math.fsum(base[i][axis]*coeff for i,coeff in zip(ids,coefficients))
                                  + offsets[axis]*ratio[axis] for axis in range(3)]
                        coords = [coordinate for i in ids for coordinate in base[i]]
                        vertices[key] = (len(vertices), struct.pack(
                            "<I3f2f4B4f3f9f",point,*origin,*uv[tex],
                            *(bones+[0]*(4-len(bones))),*(skin+[0.]*(4-len(skin))),
                            *coefficients,*coords))
                    indices.append(vertices[key][0])
                if len(set(indices)) == 3:
                    triangles.append(struct.pack("<3I",*indices))
            filename = identifier + ".rbw"
            result = (struct.pack("<4s4I32s",b"RBW1",1,len(vertices),len(triangles),len(hidden),rig[20:52])
                      + b"".join(item for _,item in vertices.values()) + b"".join(triangles)
                      + struct.pack("<" + str(len(hidden)) + "I",*hidden))
            (output/filename).write_bytes(result)
            provenance.append({"id":identifier,"file":filename,"sha256":hashlib.sha256(result).hexdigest(),
                               "sources":sources,"license":"CC0-1.0"})
    (output/"provenance.json").write_text(json.dumps({"source":PACK,"assets":provenance},indent=2)+"\n",
                                          encoding="utf-8")
    print(f"Converted {len(provenance)} CC0 wardrobe items")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_inputs", type=Path)
    parser.add_argument("wardrobe_inputs", type=Path)
    parser.add_argument("humanoid", type=Path)
    parser.add_argument("output", type=Path)
    options = parser.parse_args()
    build(options.base_inputs, options.wardrobe_inputs, options.humanoid, options.output)
