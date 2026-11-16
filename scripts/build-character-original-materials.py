#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bake original Character Creator textures from generated swatches and mesh UVs."""

import hashlib
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageOps

from character_workwear import build_boots
from importlib import import_module

pbr = import_module("build-character-pbr-textures")
utility = import_module("build-character-utility-uniform")
ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/characters"
RUNTIME = ROOT / "runtime"
WARDROBE = RUNTIME / "characters/wardrobe"
OUTPUT = RUNTIME / "characters/materials"
SOURCE = ASSETS / "reyond-surface-source-v5.png"
SIZE = 2048
PROMPT = (
    "Create a seamless-ready orthographic PBR ALBEDO texture source contact sheet for an adult character creator in a 3D game. "
    "A precise 3 x 2 layout with six equal square photographic texture samples, landscape with aspect ratio 3:2, no dividers, no words, no full objects, no perspective. "
    "Upper left: flat peach-neutral adult cheek skin showing natural tiny pores, some faint redness; upper middle: blackish-gray fine cotton poplin from a plain overshirt; upper right: indigo denim with fine diagonal twill. "
    "Bottom left: dark gray wool suiting with subtle micro-weave, bottom middle: very dark brown pebbled boot leather with fine irregular grain, bottom right: dense dark brown strands of hair arranged mostly vertically with natural subtle tonal variation. "
    "Neutral exposure and cross polarized flat diffuse light in all six samples. Entire tiles filled evenly, no harsh highlights, cast shadows, vignettes, fake mesh patterns, extreme local gradients, borders, labels, branding or watermark. High detail fine microstructure. Real material photography."
)
SKINS = {
    "skin-deep": ((103, 66, 50), .58, 5),
    "skin-medium": ((168, 116, 89), .82, 14),
    "skin-warm": ((190, 135, 109), .95, 21),
    "skin-soft": ((205, 154, 132), .72, 28),
    "skin-mature": ((178, 124, 102), 1.28, 36),
    "skin-fair": ((221, 170, 150), .78, 48),
    "skin-freckled": ((209, 147, 115), 1.05, 163),
}
# Dye colors above and below the waist, tailored to each silhouette.
WARDROBE_COLORS = {
    "male_casualsuit01": ((112, 88, 62), (68, 62, 54)),
    "male_casualsuit02": ((88, 94, 104), (47, 65, 83)),
    "male_casualsuit03": ((56, 82, 92), (59, 67, 76)),
    "male_casualsuit04": ((160, 143, 109), (87, 86, 67)),
    "male_casualsuit05": ((108, 104, 72), (77, 72, 58)),
    "male_casualsuit06": ((74, 76, 109), (58, 60, 72)),
    "male_worksuit01": ((72, 87, 98), (66, 80, 89)),
    "male_elegantsuit01": ((58, 62, 72), (48, 51, 57)),
    "female_casualsuit01": ((84, 112, 117), (83, 89, 87)),
    "female_casualsuit02": ((103, 67, 104), (63, 68, 79)),
    "female_elegantsuit01": ((93, 53, 62), (78, 43, 52)),
    "female_sportsuit01": ((71, 116, 99), (53, 70, 65)),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def smooth(low, high, value):
    t = np.clip((value-low)/(high-low), 0, 1)
    return t*t*(3-2*t)


def channels(size=SIZE):
    y, x = np.ogrid[:size, :size]
    return x.astype(np.float32)/size, y.astype(np.float32)/size


def swatches():
    image = Image.open(SOURCE).convert("RGB")
    width, height = image.width//3, image.height//2
    return [
        image.crop((x*width+12, y*height+12, (x+1)*width-12, (y+1)*height-12))
        for y in range(2) for x in range(3)
    ]


def grain(sample, period, size=SIZE):
    sample = ImageOps.grayscale(sample).resize((period, period), Image.Resampling.LANCZOS)
    pixels = np.asarray(sample, dtype=np.float32)
    low = np.asarray(sample.filter(ImageFilter.GaussianBlur(max(4, period/36))), dtype=np.float32)
    high = pixels-low
    high = np.clip(high/max(float(np.percentile(np.abs(high), 96)), 1.), -2., 2.)
    indexes = np.arange(size) % (period*2)
    indexes = np.minimum(indexes, period*2-1-indexes)
    return high[np.ix_(indexes, indexes)]


def skin_map(name, source):
    color, blush, freckles = SKINS[name]
    u, v = channels()
    face = np.abs(v-.517)
    patch = lambda at, width, across, spread: np.exp(-((u-at)/width)**2-((face-across)/spread)**2)
    red = (.78*patch(.857,.027,.047,.023)+.52*patch(.854,.024,0,.019))*blush
    lips = patch(.893,.009,0,.022)
    varied = 1.8*np.sin(41*u+32*v)+1.2*np.cos(98*v-17*u)
    marks = Image.new("L", (SIZE, SIZE))
    brush = ImageDraw.Draw(marks)
    random = np.random.default_rng(53+freckles)
    for ax, ay, radius in zip(random.normal(.849,.012,freckles), random.normal(.517,.037,freckles),
                              random.uniform(.65, 2.2, freckles)):
        if .818 < ax < .881 and abs(ay-.517) < .09:
            x, y = ax*SIZE, ay*SIZE
            brush.ellipse((x-radius,y-radius,x+radius,y+radius),fill=int(random.integers(30,105)))
    marks = np.asarray(marks.filter(ImageFilter.GaussianBlur(.55)), dtype=np.float32)*.13
    detail = varied + 3.2*grain(source, 256) - marks
    return rgb(np.asarray(color)+detail[..., None]+red[...,None]*np.array([7,-10,-9])+
               lips[...,None]*np.array([4,-20,-15]))


def mesh_uv(path):
    data = path.read_bytes()
    tag, version, count, face_count, removed = struct.unpack_from("<4s4I", data)
    start = 52+92*count
    if tag != b"RBW1" or version != 1 or len(data) != start+face_count*12+removed*4:
        raise ValueError(f"Invalid wardrobe geometry {path}")
    vertices = np.ndarray((count,5), dtype="<f4", buffer=data, offset=56,
                          strides=(92,4)).copy()
    vertices[:,3] *= SIZE-1
    vertices[:,4] = (1-vertices[:,4])*(SIZE-1)
    faces = np.frombuffer(data,dtype="<u4",count=3*face_count,offset=start).reshape((-1,3))
    return vertices, faces


def atlas_positions(path):
    vertices, faces = mesh_uv(path)
    positions = np.zeros((SIZE,SIZE,3), dtype=np.float32)
    mask = np.zeros((SIZE,SIZE), dtype=bool)
    for triangle in vertices[faces]:
        points, uv = triangle[:,:3], triangle[:,3:]
        low = np.maximum(0, np.floor(uv.min(axis=0)).astype(int))
        high = np.minimum(SIZE-1, np.ceil(uv.max(axis=0)).astype(int))
        if np.any(high < low):
            continue
        a, b = uv[1]-uv[0], uv[2]-uv[0]
        determinant = a[0]*b[1]-a[1]*b[0]
        if abs(determinant) < 1e-7:
            continue
        y, x = np.ogrid[low[1]:high[1]+1, low[0]:high[0]+1]
        u = ((x-uv[0,0])*b[1]-(y-uv[0,1])*b[0])/determinant
        v = (a[0]*(y-uv[0,1])-a[1]*(x-uv[0,0]))/determinant
        inside = (u>=-.002)&(v>=-.002)&(u+v<=1.002)
        iy, ix = np.nonzero(inside)
        positions[iy+low[1],ix+low[0]] = (
            points[0]+u[inside,None]*(points[1]-points[0])+v[inside,None]*(points[2]-points[0]))
        mask[iy+low[1],ix+low[0]] = True
    return positions, mask


def rgb(pixels):
    return Image.fromarray(np.rint(pixels).clip(0,255).astype(np.uint8))


def garment_map(identifier, geometry, fabrics):
    (x, y, z) = np.moveaxis(geometry, 2, 0)
    if identifier.startswith("shoes"):
        base = np.array([46,42,38])+18*smooth(.08,.82,y)[...,None]
        if identifier=="shoes02":
            base += 28*smooth(.39,.74,y)[...,None]
        if identifier=="shoes03":
            base += np.array([8,3,0])
        return rgb(base+(4.5*grain(fabrics[4],462)+2*np.sin(z*24))[...,None])
    if identifier=="fedora01":
        ribbon = smooth(15.97,16.02,y)*(1-smooth(16.13,16.18,y))
        return rgb(np.array([69,60,52])+(3.2*grain(fabrics[3],412)-21*ribbon)[...,None])
    upper, lower = WARDROBE_COLORS[identifier]
    belt = smooth(8.61,8.76,y)*(1-smooth(8.89,8.99,y))
    phase = smooth(8.65,8.99,y)
    dye = np.asarray(lower)+(np.asarray(upper)-lower)*phase[...,None]
    top = 3 if "elegant" in identifier or identifier.endswith("06") else 1
    bottom = top if "elegant" in identifier or "sportsuit" in identifier else 2
    weave = 4.8*(phase*grain(fabrics[top],316)+(1-phase)*grain(fabrics[bottom],391))
    belly = smooth(8.65,9.30,y)*(1-smooth(10.8,12,y))
    knee = smooth(3.2,3.7,y)*(1-smooth(4.9,5.55,y))
    folds = 2.9*belly*np.sin(y*20+x*8+z*11)+2.7*knee*np.sin(y*27-x*7+z*5)
    sleeve = smooth(1.75,2.32,np.abs(x))*smooth(9,10,y)
    folds += 2.6*sleeve*np.sin(np.abs(x)*22+y*11+z*5)
    front = smooth(.35,.62,z)*(1-smooth(1.3,1.55,np.abs(x)))
    line = np.exp(-(x/.024)**2)*front*smooth(8.8,9.3,y)*(1-smooth(13.2,13.9,y))
    stitched = .5+.5*np.cos((y+.036*np.sin(z*9))*104)
    shade = weave+folds-13*belt
    if "elegant" in identifier or "work" in identifier or identifier.endswith(("03","06")):
        placket = np.exp(-(x/.065)**2)
        buttons = np.exp(-((np.abs(x)-.017)/.045)**2-((np.remainder(y,.39)-.17)/.035)**2)
        collar = np.exp(-((y-13.62+.72*np.abs(x))/.066)**2)*(1-smooth(.65,.74,np.abs(x)))
        pocket_x = np.abs(x-.53)
        pocket_y = np.abs(y-11.78)
        pocket = (1-smooth(.26,.295,pocket_x))*(1-smooth(.22,.25,pocket_y))
        edging = np.exp(-((pocket_x-.258)/.016)**2)*(1-smooth(.22,.25,pocket_y))
        edging += np.exp(-((pocket_y-.218)/.017)**2)*(1-smooth(.258,.29,pocket_x))
        shade += front*(4*placket*phase-11*buttons*phase-7*collar-4*pocket+(4+2*stitched)*edging)-5*line
    return rgb(dye+shade[...,None])


def dilation_edge(image, original, mask):
    # Keep continuous dye at material-atlas island edges through linear filtering.
    pixels = np.asarray(image).copy()
    filled = mask.copy()
    for _ in range(4):
        for axis, offset in ((0,1),(0,-1),(1,1),(1,-1)):
            neighbor = np.roll(filled, offset, axis=axis)
            take = neighbor & ~filled
            pixels[take] = np.roll(pixels, offset, axis=axis)[take]
            filled |= neighbor
    pixels[~filled] = np.asarray(original)
    return Image.fromarray(pixels)


def hair_map(identifier, geometry, mask, source):
    u,v = channels()
    x,y,z = np.moveaxis(geometry,2,0)
    shift = .044*np.sin(v*15+u*4)
    strand = np.sin((u+shift)*1120)+.5*np.sin((u+shift*.44)*2047)
    variation = 3.2*grain(source,544)+1.2*strand+2.1*np.sin(u*46+v*6)
    bias = 7 if identifier=="afro01" else 2 if "bob" in identifier else 0
    image = rgb(np.array([43.,33.,27.])+bias+variation[...,None]*np.array([1.25,1.,.76]))
    if identifier.startswith("eyebrow"):
        strand = .65+.35*np.sin((u-.44*v)*1260+v*110)
        coverage = (1-smooth(.52,.66,np.abs(x)))*(.84+.16*strand)
    elif identifier.startswith("eyelashes"):
        strand = .45+.55*np.abs(np.sin((u-.28*v)*985))
        coverage = strand*(.60+.26*np.sin(v*145)**2)
    else:
        face = smooth(1.03,1.23,z)*(1-smooth(.51,.67,np.abs(x)))
        front = smooth(15.48,15.96,y+(.055*np.sin(x*31)+.031*np.sin(x*71)))
        coverage = 1-face*(1-front) if identifier.startswith("short") else np.ones_like(x)
    alpha = rgb(np.where(mask,255*np.clip(coverage,0,1),0)).filter(ImageFilter.MaxFilter(5))
    image.putalpha(alpha)
    return image


def record(manifest, path, source, inputs):
    manifest["textures"][path.stem] = {
        "file":path.name, "sha256":digest(path), "size":Image.open(path).width,
        "source":source, "authoringInputs":{p.relative_to(ROOT).as_posix():digest(p) for p in inputs},
    }


def save(manifest, name, image, source, inputs, size=SIZE):
    if image.size != (size,size):
        image = image.resize((size,size), Image.Resampling.LANCZOS)
    path = OUTPUT/(name+(".png" if image.mode=="RGBA" else ".jpg"))
    settings = {"compress_level":5} if image.mode=="RGBA" else {"quality":93,"subsampling":0}
    image.save(path, **settings)
    record(manifest,path,source,inputs)


def main():
    registry = OUTPUT / "provenance.json"
    previous = json.loads(registry.read_bytes())
    manifest = {
        "version":2, "detailSource":{
            "file":SOURCE.relative_to(ROOT).as_posix(), "tool":"OpenAI ImageGen built-in",
            "prompt":PROMPT, "sha256":digest(SOURCE),
        }, "textures":{}, "pbrInputs":previous["pbrInputs"],
        "pbrFabric":previous["pbrFabric"], "pbrDetail":previous["pbrDetail"],
    }
    fabrics = swatches()
    for name in SKINS:
        save(manifest,name,skin_map(name,fabrics[0]),"Original skin grain and painted UV pigment",[SOURCE])

    iris = ASSETS / "iris-albedo-source-v3.png"
    for path in pbr.bake_eyes(iris):
        record(manifest,path,"Original generated iris and color variants",[iris])
        with Image.open(path) as image:
            save(manifest,path.stem.replace("-pbr",""),image,"Original generated iris and color variants",[iris],512)
    for path in sorted(WARDROBE.glob("*.rbw")):
        name = path.stem
        if name in ("male_utility_field","short02-tousled"):
            continue
        geometry, mask = atlas_positions(path)
        cutout = name.startswith(("eyebrow","eyelashes")) or (
            name not in WARDROBE_COLORS and not name.startswith(("shoes","fedora")))
        image = hair_map(name,geometry,mask,fabrics[5]) if cutout else garment_map(name,geometry,fabrics)
        if not cutout:
            image = dilation_edge(image,(70,65,60),mask)
        label = "Original fibers and geometry-derived UV opacity" if cutout else "Original fabric and UV dyes"
        save(manifest,"hd-"+name,image,label,[SOURCE,path])
        save(manifest,name,image,label,[SOURCE,path],512 if name.startswith(("eyebrow","eyelashes")) else 1024)
    manifest["textures"]["charcoal-utility"] = previous["textures"]["charcoal-utility"]
    build_boots(OUTPUT, manifest)
    utility.fringe_atlas(OUTPUT, manifest)
    registry.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")


if __name__=="__main__":
    main()
