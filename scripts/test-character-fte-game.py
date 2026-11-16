#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run three native IQM exports together in a standalone FTE game under X11."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
from tempfile import TemporaryDirectory
import time

from PIL import ImageChops, ImageGrab


REPO = Path(__file__).resolve().parents[1]
ACTORS = ("casey", "drew", "jamie")


def stage_actors(game, bundles):
    hashes = {}
    for name,bundle in zip(ACTORS,bundles):
        source = bundle / "models"
        iqm = source / (name + ".iqm")
        if not (source/"scripts"/("character-"+name+".shader")).is_file():
            raise RuntimeError(f"{name}: the native export is missing its shader definitions.")
        hashes[name] = hashlib.sha256(iqm.read_bytes()).hexdigest()
        path = game / "models" / iqm.name
        path.parent.mkdir(exist_ok=True)
        shutil.copy2(iqm,path)
        for folder in ("scripts","textures","glsl"):
            for file in (source/folder).rglob("*"):
                if not file.is_file():
                    continue
                target = game / file.relative_to(source)
                target.parent.mkdir(exist_ok=True,parents=True)
                if target.exists() and target.read_bytes() != file.read_bytes():
                    raise RuntimeError(f"Conflicting IQM dependency: {target.relative_to(game)}")
                if not target.exists():
                    shutil.copy2(file,target)
    if len(set(hashes.values())) != len(ACTORS):
        raise RuntimeError("The three fixtures must be different models.")
    return hashes


def build_room(game, tools, output):
    # Use a generated grayscale palette and one mipmapped texture so the
    # fixture never needs a retail game installation or a downloaded WAD.
    name = b"roomtile"
    mipmaps = [bytes(108 + 12*((x//max(1,size//8) + y//max(1,size//8)) % 2)
                    for y in range(size) for x in range(size)) for size in (32,16,8,4)]
    offsets = [40 + sum(map(len,mipmaps[:i])) for i in range(4)]
    texture = struct.pack("<16s6I",name,32,32,*offsets) + b"".join(mipmaps)
    (game/"room.wad").write_bytes(struct.pack("<4sII",b"WAD2",1,12+len(texture)) + texture +
                                 struct.pack("<IIIBBH16s",12,len(texture),len(texture),68,0,0,name))
    (game/"gfx").mkdir()
    (game/"gfx"/"palette.lmp").write_bytes(bytes(v for v in range(256) for _ in range(3)))
    maps = game/"maps"
    maps.mkdir()
    bounds = [
        ((-272,-272,-16),(272,272,0)), ((-272,-272,192),(272,272,208)),
        ((-272,-272,0),(-256,272,192)), ((256,-272,0),(272,272,192)),
        ((-256,-272,0),(256,-256,192)), ((-256,256,0),(256,272,192)),
    ]
    def box(lo,hi):
        x,y,z = lo
        X,Y,Z = hi
        planes = (((x,Y,Z),(x,y,Z),(x,y,z)),((X,y,z),(X,y,Z),(X,Y,Z)),
                  ((X,y,Z),(X,y,z),(x,y,z)),((x,Y,z),(X,Y,z),(X,Y,Z)),
                  ((X,Y,z),(x,Y,z),(x,y,z)),((x,y,Z),(x,Y,Z),(X,Y,Z)))
        return "{\n" + "".join(
            " ".join(f"( {a} {b} {c} )" for a,b,c in plane) + " roomtile 0 0 0 1 1\n"
            for plane in planes) + "}\n"

    room = maps/"character_room.map"
    room.write_text('{\n"classname" "worldspawn"\n"wad" "room.wad"\n' +
                    "".join(box(lo,hi) for lo,hi in bounds) +
                    '}\n{\n"classname" "info_player_start"\n"origin" "-154 0 68"\n}\n' +
                    '{\n"classname" "light"\n"origin" "-72 0 134"\n"light" "340"\n}\n',
                    encoding="ascii")
    environment = {**os.environ, "LD_LIBRARY_PATH": str(tools)}
    with (output/"build-room.log").open("wb") as log:
        for tool,arg in (("qbsp",room),("light",room.with_suffix(".bsp"))):
            subprocess.run([str(tools/tool),str(arg)],cwd=game,env=environment,stdout=log,
                           stderr=subprocess.STDOUT,check=True,timeout=45)
    if not room.with_suffix(".prt").is_file() or room.with_suffix(".pts").is_file():
        raise RuntimeError("The standalone test room must be sealed.")


def build_quakec(game, compiler, output):
    sources = REPO/"tests"/"fixtures"/"characters"
    with (output/"build-quakec.log").open("wb") as log:
        for kind,result in (("server","progs.dat"),("client","csprogs.dat")):
            scratch = game.parent/kind
            scratch.mkdir()
            shutil.copy2(REPO/"qc"/"csplat.qc",scratch/"csplat.qc")
            shutil.copy2(sources/f"standalone-{kind}.qc",scratch/"main.qc")
            (scratch/"progs.src").write_text(f"{result}\nmain.qc\n",encoding="ascii")
            subprocess.run([str(compiler),"-Tfte"],cwd=scratch,stdout=log,
                           stderr=subprocess.STDOUT,check=True,timeout=45)
            shutil.copy2(scratch/result,game/result)


def exercise(engine, game, output):
    runner = game.parent/engine.name
    shutil.copy2(engine,runner)
    (game/"rb21.cfg").write_text(
        "vid_vsync 0\ncl_maxfps 60\nscr_conspeed 10000\n"
        "set cl_download_csprogs 1\nmap character_room\nin 12 disconnect\nin 13 quit\n",encoding="ascii")
    environment = {**os.environ,"LIBGL_ALWAYS_SOFTWARE":"1","SDL_VIDEODRIVER":"x11"}
    environment.pop("WAYLAND_DISPLAY",None)
    log_path = output/"standalone.log"
    with log_path.open("wb") as stream:
        process = subprocess.Popen([str(runner),"-quake","-basedir",str(game.parent),
                                    "-nohome","-noupdates","-nosound","-nomouse","-condebug",
                                    "-window","-width","1280","-height","800","+exec","rb21.cfg"],
                                   cwd=game.parent,env=environment,stdout=stream,stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic()+18
            while "RB21_RIGS_READY" not in log_path.read_text(errors="replace"):
                if process.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError(f"Standalone rigs did not load. See {log_path}")
                time.sleep(.1)
            # The ready message is emitted before the first render presents.
            time.sleep(.45)
            frames = []
            for index in range(5):
                image = ImageGrab.grab(bbox=(0,0,1280,800)).convert("RGB")
                image.save(output/f"standalone-{index}.png")
                frames.append(image)
                time.sleep(.22)
            if process.wait(timeout=19) != 0:
                raise RuntimeError(f"Standalone FTE exited with code {process.returncode}. See {log_path}")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
    log = log_path.read_text(errors="replace")
    samples = [(int(i),int(actor),float(distance)) for i,actor,distance in
               re.findall(r"RB21_WRIST (\d+) (\d+) ([\d.]+)",log)]
    def changed(a,b,box):
        return ImageChops.difference(a.crop(box),b.crop(box)).convert("L").point(
            lambda v:255 if v>=14 else 0).histogram()[255]

    pairs = list(zip(frames,frames[1:]))
    if any(changed(a,b,(0,655,1280,800))>80 for a,b in pairs):
        raise RuntimeError("The fixed-camera floor changed between captures.")
    pixels = {}
    for i,name in enumerate(ACTORS):
        if max((value for phase,actor,value in samples if phase>1 and actor==i),default=0)<.5:
            raise RuntimeError(f"{name}: FTE did not move the sampled wrist.")
        if f"RB21_SKELETON_RELEASED {i}" not in log:
            raise RuntimeError(f"{name}: FTE did not log skeleton disposal.")
        x = (365,640,915)[i]
        box = (x-100,200,x+100,625)
        pixels[name] = [changed(a,b,box) for a,b in pairs]
        if sum(count>=160 for count in pixels[name])<2:
            raise RuntimeError(f"{name}: the corresponding model's pixels did not move.")
    return {"wristMotion":samples,"changedPixels":pixels}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output",type=Path)
    parser.add_argument("--engine",type=Path,required=True)
    parser.add_argument("--compiler",type=Path,required=True)
    parser.add_argument("--map-tools",type=Path,required=True)
    parser.add_argument("--bundles",type=Path,nargs=3,required=True,metavar=("CASEY","DREW","JAMIE"))
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True,exist_ok=True)
    summary = out/"report.json"
    summary.unlink(missing_ok=True)
    with TemporaryDirectory(prefix="rb21-standalone-") as scratch:
        game = Path(scratch)/"id1"
        game.mkdir()
        hashes = stage_actors(game,args.bundles)
        build_room(game,args.map_tools.resolve(),out)
        build_quakec(game,args.compiler.resolve(),out)
        report = exercise(args.engine.resolve(),game,out)
        report["models"] = hashes
        report["engine"] = hashlib.sha256(args.engine.read_bytes()).hexdigest()
        summary.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print("PASS three native IQMs play walk_forward in standalone FTE with matching bone names and parents.")


if __name__ == "__main__":
    main()
