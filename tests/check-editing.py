"""Compare complete MAP face records and texture transforms after editor operations."""
from pathlib import Path
import math
import re
import sys

root = Path(sys.argv[1])

def faces(name):
    return [line for line in (root / f"edit_{name}.map").read_text().splitlines()
            if line.startswith("( ")]

def same(a, b):
    assert sorted(faces(a)) == sorted(faces(b)), f"{a} differs from {b}"

start = faces("start")
material = faces("material")
uv = faces("uv")
assert len(start) == len(material) == len(uv) == 12
assert sum(a != b for a, b in zip(start, material)) == 1, "Material edit escaped selected face"
assert sum(a != b for a, b in zip(material, uv)) == 1, "UV edit escaped selected face"
changed = [line for line in uv if "test/custom" in line]
assert len(changed) == 1
axes = re.findall(r"\[ ([^]]+) \]", changed[0])
u, v = [list(map(float, axis.split()))[:3] for axis in axes]
assert math.isclose(math.sqrt(sum(x*x for x in u)), .5, abs_tol=1e-5), u
assert math.isclose(math.sqrt(sum(x*x for x in v)), .25, abs_tol=1e-5), v
assert abs(sum(a*b for a,b in zip(u,v))) < 1e-5, "Rotated axes not perpendicular"
same("uv", "redo")
same("start", "undo")
same("uv", "reload")
assert sum("test/custom" in x for x in faces("all")) == 6, "Whole brush material assignment failed"
same("all", "clip_undo")
same("clip", "clip_redo")
for case in ("tangent", "miss", "collinear"):
    same("all", case)
cut = faces("clip")
assert len(cut) == 18, f"Expected floor and two six-face halves, got {len(cut)} faces"
# Every source surface remains present; clipping adds two opposite X=0 cap faces.
caps = [line for line in cut if all(abs(float(x)) < 1e-6 for x in re.findall(r"\( ([\d.eE+-]+) ", line))]
assert len(caps) == 2, "Missing opposite cut caps"
same("all", "side_undo")
def bounds(lines):
    # Valve 220 stores points on each infinite plane. In this axis-aligned
    # fixture the constant coordinate gives the actual brush boundary.
    coordinates = [[], [], []]
    for line in lines:
        points = [tuple(map(float, point)) for point in
                  re.findall(r"\( ([\d.eE+-]+) ([\d.eE+-]+) ([\d.eE+-]+) \)", line)]
        assert len(points) == 3, line
        axes = [axis for axis in range(3) if points[0][axis] == points[1][axis] == points[2][axis]]
        assert len(axes) == 1, line
        axis, = axes
        coordinates[axis].append(points[0][axis])
    assert all(len(values) == 2 and values[0] != values[1] for values in coordinates), coordinates
    return tuple(map(min, coordinates)), tuple(map(max, coordinates))
assert bounds(cut[6:12]) == ((-64, -64, 0), (0, 64, 128))
assert bounds(cut[12:18]) == ((0, -64, 0), (64, 64, 128))
assert len(faces("front")) == len(faces("back")) == 12
assert bounds(faces("front")[6:]) == ((-64, -64, 0), (0, 64, 128))
assert bounds(faces("back")[6:]) == ((0, -64, 0), (64, 64, 128))
print("PASS: face material/UV, whole brush, complete undo/redo/reload, split and rejected cuts")
