#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract straight locomotion cycles from the credited 100STYLE BVH takes."""
import hashlib
from pathlib import Path
import re
import sys
import numpy as np

def rotations(values,axes):
    matrices=np.broadcast_to(np.eye(3),(len(values),3,3)).copy()
    for column,axis in enumerate(axes):
        c,s=np.cos(values[:,column]),np.sin(values[:,column])
        a="XYZ".index(axis)
        i,j=(a+1)%3,(a+2)%3
        rotation=np.broadcast_to(np.eye(3),matrices.shape).copy()
        rotation[:,i,i]=rotation[:,j,j]=c
        rotation[:,i,j]=-s
        rotation[:,j,i]=s
        matrices=matrices@rotation
    return matrices

def read(path):
    hierarchy,motion=Path(path).read_text().split("MOTION")
    tokens=iter(re.findall(r"[{}]|[^\s{}]+",hierarchy))
    assert next(tokens)=="HIERARCHY"
    names,offsets,parents,channels=[],[],[],[]
    def joint(kind,parent):
        name=next(tokens)
        if kind=="End": name=names[parent]+"_end"
        assert next(tokens)=="{"
        i=len(names)
        names.append(name)
        parents.append(parent)
        channels.append([])
        assert next(tokens)=="OFFSET"
        offsets.append([float(next(tokens)) for _ in range(3)])
        token=next(tokens)
        if token=="CHANNELS":
            channels[i]=[next(tokens) for _ in range(int(next(tokens)))]
            token=next(tokens)
        while token in ("JOINT","End"):
            joint(token,i)
            token=next(tokens)
        assert token=="}"
    joint(next(tokens),-1)
    lines=motion.strip().splitlines()
    dt=float(lines[1].split()[-1])
    data=np.loadtxt(lines[2:])
    pos=np.empty((len(data),len(names),3))
    rot=np.empty((len(data),len(names),3,3))
    start=0
    for i,spec in enumerate(channels):
        parent=parents[i]
        idx=[start+j for j,c in enumerate(spec) if c.endswith("rotation")]
        matrix=rotations(np.deg2rad(data[:,idx]),[spec[j-start][0] for j in idx])
        if parent<0:
            pos[:,i]=data[:,:3]
            rot[:,i]=matrix
        else:
            pos[:,i]=pos[:,parent]+rot[:,parent]@offsets[i]
            rot[:,i]=rot[:,parent]@matrix
        start+=len(spec)
    return names,pos,rot,dt

def unit(vectors):
    return vectors / np.maximum(np.linalg.norm(vectors, axis=-1, keepdims=True), 1e-8)


TAKES = (
    ("walk", "Neutral_FW.bvh", 540, 619, True,
     "2f28800baa299547c42c257d1c926172542a1e69b46a3d3e6aa0aded0e9b40f6"),
    ("run", "Neutral_FR.bvh", 488, 536, True,
     "f46ca526027a3654c8939a0123413f6008906fb32125d5e885eafcaa830831cb"),
    ("backward", "Neutral_BW.bvh", 6120, 6190, False,
     "cfa225ff3a29a38d11ecd1b72fa0d917ca6c3dc3b24b3d09a4cb5c28ee9dc5b9"),
    ("strafeRight", "Neutral_SW.bvh", 1481, 1554, False,
     "07225ded6df11c4eb97a7b42e6121a9e985ae7e8d0f56f9ca971429f9c507ad9"),
    ("crouchWalk", "Crouched_FW.bvh", 5878, 5951, True,
     "e4242640c94810a6c5f1df0d0d0bb78c8ffaed4bb79a7569ba3391e57a34ac8a"),
)


def fit_take(path, a, b, faces_travel):
    names, positions, _, dt = read(path)
    joint = {name: index for index, name in enumerate(names)}
    clip = positions[a:b+1]
    pelvis = clip[:, 0]
    direction = pelvis[-1] - pelvis[0]
    direction[1] = 0
    if faces_travel:
        forward = unit(direction)
    else:
        shoulders = clip[:, joint["LeftShoulder"]] - clip[:, joint["RightShoulder"]]
        left = np.mean(shoulders, axis=0)
        left[1] = 0
        forward = np.cross(unit(left), [0, 1, 0])
    basis = np.stack(([forward[2], 0, -forward[0]], [0, 1, 0], forward), axis=1)
    local = (clip - pelvis[0]) @ basis
    local -= np.linspace(0, 1, len(clip))[:, None, None] * (direction @ basis)
    centers = local[:, 0]
    root = (centers - np.mean(centers, axis=0)) * .094
    feet, pitch, upper, lower = [], [], [], []
    all_ankles = local[:, [joint["LeftAnkle"], joint["RightAnkle"]]]
    floor = np.percentile(all_ankles[:, :, 1], 11)
    fore = np.mean(all_ankles[:, :, 2])
    for side, sign in (("Left", 1), ("Right", -1)):
        ankle = local[:, joint[side + "Ankle"]].copy()
        toes = local[:, joint[side + "Toe"]]
        foot = toes - ankle
        angle = np.rad2deg(np.arctan2(-foot[:, 1], np.linalg.norm(foot[:, [0, 2]], axis=1)))
        pitch.append(np.clip(angle - np.percentile(angle, 35), -23, 42))
        ankle[:, 1] -= floor
        ankle[:, 2] -= fore
        ankle *= .094
        ankle[:, 0] += .32 * sign
        ankle[:, 1] = np.maximum(-.025, ankle[:, 1])
        feet.append(ankle)
        shoulder = local[:, joint[side + "Shoulder"]]
        elbow = local[:, joint[side + "Elbow"]]
        wrist = local[:, joint[side + "Wrist"]]
        upper.append(unit(elbow - shoulder))
        lower.append(unit(wrist - elbow))
    inputs = np.column_stack([root, *feet, *pitch, *upper, *lower])
    # Remove the residual difference across the selected cycle so the seam is exact.
    inputs -= np.linspace(0, 1, len(inputs))[:, None] * (inputs[-1] - inputs[0])
    times = np.arange(len(inputs))
    output = np.stack([np.interp(np.linspace(0, times[-1], 41), times, values)
                       for values in inputs.T], axis=1)
    return (b - a) * dt, output


def write_header(source_dir, output):
    rows = ["// 100STYLE locomotion, Ian Mason, Sebastian Starke and Taku Komura, CC BY 4.0.",
            "// Derived by scripts/build-character-motion.py. Do not hand-edit.",
            "#pragma once", "", "namespace ftebroom::character {",
            "struct CapturedGaitFrame {",
            "  Vec3 root, feet[2];", "  double pitch[2];",
            "  Vec3 upperArm[2], forearm[2];", "};", ""]
    vec = lambda values: "{" + ",".join(format(v, ".5f") for v in values) + "}"
    for name, source, start, end, faces_travel, expected in TAKES:
        path = source_dir / source
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Unexpected motion input: {source}")
        duration, values = fit_take(path, start, end, faces_travel)
        rows.append(f"inline constexpr double {name}CaptureSeconds = {duration:.5f};")
        rows.append(f"inline constexpr CapturedGaitFrame {name}Capture[]{{")
        for v in values:
            rows.append("  {" + ",".join((vec(v[0:3]),
                "{" + vec(v[3:6]) + "," + vec(v[6:9]) + "}", vec(v[9:11]),
                "{" + vec(v[11:14]) + "," + vec(v[14:17]) + "}",
                "{" + vec(v[17:20]) + "," + vec(v[20:23]) + "}")) + "},")
        rows.extend(["};", ""])
    rows.append("} // namespace ftebroom::character")
    output.write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    write_header(Path(sys.argv[1]), Path(sys.argv[2]))
