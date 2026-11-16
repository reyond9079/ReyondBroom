#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the Field Technician uniform and its UV-masked fringe."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

from character_workwear import build_uniform

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT/"runtime/characters"
SIZE = 2048


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def smooth(low, high, x):
    t = np.clip((x-low)/(high-low),0.,1.)
    return t*t*(3-2*t)


def fringe_coverage(x, y, z):
    front = smooth(1.03,1.24,z)*(1-smooth(.49,.61,np.abs(x)))
    # Expose uneven channels at the root of the fringe. Each lock is specified
    # by its horizontal root, bend and tip, tip height and root half-width.
    edge = (15.974+.145*smooth(-.14,.48,x)+.145*np.exp(-((x-.23)/.096)**2)+
            .031*np.sin(19*x+.9)+.021*np.sin(46*x-.3))
    mask = smooth(-.018,.016,y-edge)
    locks = [
        (-.44,-.52,-.56,15.803,.047),
        (-.39,-.49,-.503,15.703,.036),
        (-.33,-.48,-.433,15.576,.047),
        (-.28,-.40,-.372,15.674,.025),
        (-.20,-.36,-.272,15.542,.058),
        (-.15,-.29,-.31,15.731,.022),
        (-.09,-.22,-.19,15.504,.045),
        (-.035,-.12,-.135,15.668,.026),
        ( .051,-.067,-.072,15.656,.043),
        ( .110, .015, .033,15.794,.035),
        ( .16, .12, .106,15.835,.018),
        ( .32, .391,.436,15.849,.039),
        ( .38, .469,.489,15.744,.042),
        ( .47, .54, .51,15.841,.026),
        ( .53, .58, .55,15.901,.033),
    ]
    for index, (root, bend, tip, end, width) in enumerate(locks):
        t = np.clip((16.26-y)/(16.26-end),0.,1.)
        # A curl in the bottom third makes the tips hook away from their roots.
        mid = (1-t)**3*root+3*t*(1-t)**2*bend+3*t*t*(1-t)*bend+t**3*tip
        outline = width*(1-.90*t**(1.15+.26*(index%3)))
        distance = outline-np.abs(x-mid)
        lock = smooth(-.005,.007,distance)*smooth(end-.007,end+.025,y)
        mask = np.maximum(mask,lock)
    return 1-front*(1-mask)


def shaped_fringe():
    wardrobe = RUNTIME/"wardrobe"
    base = wardrobe/"short02.rbw"
    mesh = bytearray(base.read_bytes())
    magic,version,vertices,faces,_ = struct.unpack_from("<4s4I",mesh)
    if magic!=b"RBW1" or version!=1:
        raise ValueError("Expected the canonical RBW1 hair")
    points, uv = [], []
    for index in range(vertices):
        offset = 56+92*index
        x,y,z,u,v = struct.unpack_from("<5f",mesh,offset)
        extension = (smooth(1.04,1.38,z)*(1-smooth(.43,.62,abs(x)))*
                     (1-smooth(15.79,16.38,y)))
        y -= .44*extension
        z -= .12*extension
        x -= .04*extension*(1-smooth(.02,.35,x))
        struct.pack_into("<3f",mesh,offset,x,y,z)
        points.append((x,y,z))
        uv.append((u*(SIZE-1),(1-v)*(SIZE-1)))
    path = wardrobe/"short02-tousled.rbw"
    path.write_bytes(mesh)
    registry = wardrobe/"provenance.json"
    manifest = json.loads(registry.read_bytes())
    parent = next(asset for asset in manifest["assets"] if asset["id"]=="short02")
    entry = {
        "id":path.stem,"file":path.name,"sha256":digest(path),
        "sources":parent["sources"],"license":parent["license"],
        "material":"tousled-fringe","derivedFrom":base.name,
    }
    manifest["assets"] = [asset for asset in manifest["assets"] if asset["id"]!=entry["id"]]+[entry]
    registry.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    start = 52+92*vertices
    triangles = np.frombuffer(mesh,dtype="<u4",count=3*faces,offset=start).reshape((-1,3))
    return np.array(points), np.array(uv), triangles


def trim_fringe_alpha(alpha, vertices, texels, faces):
    cut = np.ones(alpha.shape,dtype=np.float32)
    for ids in faces:
        points, uv = vertices[ids], texels[ids]
        if (min(points[:,1])>16.33 or max(points[:,2])<1.03 or
                min(points[:,0])>.61 or max(points[:,0])<-.61):
            continue
        low = np.maximum(0,np.floor(uv.min(axis=0)).astype(int))
        high = np.minimum(SIZE-1,np.ceil(uv.max(axis=0)).astype(int))
        if np.any(high<low):
            continue
        a,b = uv[1]-uv[0],uv[2]-uv[0]
        determinant = a[0]*b[1]-a[1]*b[0]
        if abs(determinant)<1e-7:
            continue
        yy,xx = np.ogrid[low[1]:high[1]+1,low[0]:high[0]+1]
        u = ((xx-uv[0,0])*b[1]-(yy-uv[0,1])*b[0])/determinant
        v = (a[0]*(yy-uv[0,1])-a[1]*(xx-uv[0,0]))/determinant
        xyz = [points[0,i]+(points[1,i]-points[0,i])*u+(points[2,i]-points[0,i])*v
               for i in range(3)]
        # Overlap one fraction of a texel at shared UV edges to avoid a
        # strand of unmasked pixels due to rounding during rasterization.
        mask = np.where((u>=-.0008)&(v>=-.0008)&(u+v<=1.0008),fringe_coverage(*xyz),1.)
        cut[yy,xx] = np.minimum(cut[yy,xx],mask)
    # At a UV seam the renderer filters slightly beyond the last triangle.
    # Carry the cut into its padding so discarded texels cannot leave a line.
    padding = Image.fromarray(np.rint(cut*255).astype(np.uint8)).filter(ImageFilter.MinFilter(5))
    return alpha*np.asarray(padding,dtype=np.float32)/255.


def fringe_atlas(materials, manifest):
    original = materials/"hd-short02.png"
    source = np.asarray(Image.open(original).convert("RGBA"),dtype=np.float32)
    luma = source[:,:,:3] @ np.array([.2126,.7152,.0722],dtype=np.float32)
    # Use one dye for the crown, nape and wisps. Preserve the original alpha
    # and strand detail while compressing the bright bands across the shells.
    value = np.clip(39+(luma-62)*.55,16,82)
    alpha = trim_fringe_alpha(source[:,:,3],*shaped_fringe())
    atlas = np.dstack((value*1.035,value,value*.945,alpha))
    asset = materials/"tousled-fringe.png"
    Image.fromarray(np.rint(atlas).astype(np.uint8),"RGBA").save(asset,optimize=True)
    manifest["textures"][asset.stem] = {
        "file":asset.name,"sha256":digest(asset),"size":SIZE,
        "source":"Original ReyondBroom short02 strands with a hand-shaped fringe opacity mask",
        "authoringInputs":{original.relative_to(ROOT).as_posix():digest(original)},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fringe-only",action="store_true")
    args = parser.parse_args()
    materials = RUNTIME/"materials"
    registry = materials/"provenance.json"
    manifest = json.loads(registry.read_bytes())
    if args.fringe_only:
        fringe_atlas(materials,manifest)
        registry.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
        return
    build_uniform(materials,manifest)
    fringe_atlas(materials,manifest)
    registry.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")


if __name__=="__main__":
    main()
