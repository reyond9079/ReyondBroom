#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bake adult face shape differences for the canonical MakeHuman topology."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


REVISION = "a8bc2d54ff0ac92e78ff71431b1023eda42bf482"
TARGET_URL = f"https://raw.githubusercontent.com/makehumancommunity/makehuman/{REVISION}/makehuman/data/targets/macrodetails/"
GROUPS = ("caucasian", "asian", "african")


def load(path):
    data = path.read_bytes()
    assert b"explicitly released as CC0" in data
    rows = {}
    for line in data.decode("utf-8").splitlines():
        if line and not line.startswith("#"):
            index, *delta = line.split()
            rows[int(index)] = tuple(map(float, delta))
    return data, rows


def bake(folder, rig_file, output):
    rig = rig_file.read_bytes()
    magic, _, joint_count, vertex_count = struct.unpack_from("<4s3I", rig)
    assert magic == b"RBH1"
    start = 52 + 48 * joint_count
    vertices = {v[0]: v[1:4] for v in struct.iter_unpack("<I3f2f4B4f", rig[start:start+44*vertex_count])}
    targets, sources = [], {}
    for group in GROUPS:
        pair = []
        for shape in ("female", "male"):
            name = f"{group}-{shape}-young.target"
            data, rows = load(folder/name)
            pair.append(rows)
            sources[TARGET_URL+name] = hashlib.sha256(data).hexdigest()
        targets.append(pair)

    def value(pair, index):
        return pair.get(index, (0., 0., 0.))

    # Keep the canonical skeleton fixed. Remove the displacement at the
    # head joint, so the adult patterns change shape without sliding the head.
    head = next(struct.unpack_from("<3f",rig,pos+36)
                for pos in range(52,start,48) if rig[pos:pos+32].split(b"\0",1)[0]==b"head")
    anchor = sorted(vertices,key=lambda i:sum((a-b)**2 for a,b in zip(vertices[i],head)))[:20]
    templates = [side for group in targets for side in group]
    translation = [tuple(sum(value(side,i)[axis] for i in anchor)/len(anchor)
                         for axis in range(3)) for side in templates]
    rows = []
    for index, (_, y, _) in sorted(vertices.items()):
        if y <= 13.75:
            continue
        weight = min(1., (y-13.75)/.85)
        weight = weight*weight*(3-2*weight)
        local = [tuple(a-b for a,b in zip(value(side,index),offset))
                 for side,offset in zip(templates,translation)]
        average = tuple(sum(v[axis] for v in local)/len(local) for axis in range(3))
        deltas = [(v[axis]-average[axis])*weight for v in local for axis in range(3)]
        rows.append(struct.pack("<I18f", index, *deltas))

    output.mkdir(parents=True, exist_ok=True)
    file = output/"adult-face-shapes.rbf"
    file.write_bytes(struct.pack("<4sII32s", b"RBF1", 1, len(rows), rig[20:52])+b"".join(rows))
    (output/"face-shapes-provenance.json").write_text(json.dumps({
        "license": "CC0-1.0", "sources": sources,
        "files": {file.name: hashlib.sha256(file.read_bytes()).hexdigest()},
    }, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target_inputs", type=Path)
    parser.add_argument("rig", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    bake(args.target_inputs, args.rig, args.output)
