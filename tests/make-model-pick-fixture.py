"""Generate the concave static mesh used by ftebroom_test_pick mesh."""
import argparse
from pathlib import Path


def grid(out, first, x0, x1, y0, y1, columns, rows):
    for y in range(rows + 1):
        for x in range(columns + 1):
            out.write(f"v {x0 + (x1 - x0) * x / columns:.6f} "
                      f"{y0 + (y1 - y0) * y / rows:.6f} 0\n")
    rows_per_group = max(1, 10_000 // columns)
    for y in range(rows):
        if y % rows_per_group == 0:
            out.write(f"g bar_{first}_{y}\n")
        for x in range(columns):
            a = first + y * (columns + 1) + x
            b = a + columns + 1
            out.write(f"f {a}/0/1 {a + 1}/0/1 {b + 1}/0/1\n"
                      f"f {a}/0/1 {b + 1}/0/1 {b}/0/1\n")
    return first + (columns + 1) * (rows + 1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gamedir", type=Path)
    args = parser.parse_args()
    model = args.gamedir / "models/qa/mesh-pick.obj"
    model.parent.mkdir(parents=True, exist_ok=True)
    with model.open("w", encoding="ascii") as out:
        out.write("vn 0 0 1\n")
        next_vertex = grid(out, 1, 0, 160, 0, 16, 320, 96)
        grid(out, next_vertex, 0, 16, 16, 160, 96, 320)
    print(f"{model} (122880 triangles)")
