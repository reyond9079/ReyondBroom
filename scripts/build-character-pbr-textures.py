#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bake the generated character PBR sources into runtime input textures."""
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/characters"
RUNTIME = ROOT / "runtime"
MATERIALS = RUNTIME / "characters/materials"
DETAIL = RUNTIME / "textures/reyond_character_detail_v3.png"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def periodic_axis(image, axis, overlap=48):
    image = np.moveaxis(image, axis, 0)
    weight = (np.arange(overlap) + .5) / overlap
    weight = (weight * weight * (3 - 2 * weight))[:, None]
    transition = image[-overlap:] * (1 - weight) + image[:overlap] * weight
    return np.moveaxis(np.concatenate((transition, image[overlap:-overlap])), 0, axis)


def bake_skin(path):
    source = Image.open(path).convert("L").resize((560, 560), Image.Resampling.LANCZOS)
    height = np.asarray(source, dtype=np.float32) - np.asarray(source.filter(ImageFilter.GaussianBlur(6)))
    height = periodic_axis(periodic_axis(height, 0), 1)
    dx = .025 * (np.roll(height, 1, axis=1) - np.roll(height, -1, axis=1))
    dy = .025 * (np.roll(height, 1, axis=0) - np.roll(height, -1, axis=0))
    length = np.sqrt(1 + dx * dx + dy * dy)
    rgba = np.stack((.5 + .5 * dx / length, .5 + .5 * dy / length, .5 + .5 / length,
                     np.clip(.5 + .017 * height, .1, .9)), axis=-1)
    DETAIL.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.rint(255 * rgba).astype(np.uint8), "RGBA").save(DETAIL, optimize=True)


def bake_eyes(path):
    size = 2048
    eye = np.asarray(Image.open(path).convert("RGB").resize((size, size), Image.Resampling.LANCZOS),
                     dtype=np.float32) / 255
    y, x = np.mgrid[:size, :size] / (size-1)
    distance = np.sqrt((x - .511) ** 2 + (y - .566) ** 2)
    mask = np.clip((.225 - distance) / .031, 0, 1)[..., None]
    value = np.dot(eye, [.30, .51, .19])[..., None]
    # Pigment changes only the iris, preserving the sclera and black pupil.
    for color, tint in {"brown": None, "blue": [.89, 1.45, 1.83], "green": [1.02, 1.52, 1.00]}.items():
        result = eye if tint is None else eye * (1 - mask) + value * tint * mask
        output = MATERIALS / f"eyes-pbr-{color}.jpg"
        Image.fromarray(np.rint(255 * np.clip(result, 0, 1)).astype(np.uint8), "RGB").save(
            output, quality=96, subsampling=0, optimize=True)
        yield output


def main():
    pores = SOURCE / "skin-pore-source-v3.png"
    iris = SOURCE / "iris-albedo-source-v3.png"
    bake_skin(pores)
    registry = MATERIALS / "provenance.json"
    manifest = json.loads(registry.read_text(encoding="utf-8"))
    manifest.setdefault("pbrInputs",{}).update({p.relative_to(ROOT).as_posix(): digest(p) for p in (pores, iris)})
    manifest["pbrDetail"] = {"file": DETAIL.relative_to(RUNTIME).as_posix(), "sha256": digest(DETAIL)}
    for path in bake_eyes(iris):
        manifest["textures"][path.stem] = {"file": path.name, "sha256": digest(path),
                                            "source": "ImageGen diffuse iris, derived with per-color pigment",
                                            "size": 2048}
    registry.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
