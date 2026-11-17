#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Play three named Studio MDL sequences in a locally installed Steam Half-Life."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

from PIL import Image, ImageChops


CLIPS = {"walk_forward": 47, "run_forward": 0, "death_back": -47}
MODEL = "models/rb21_qa_character.mdl"
ROOM = "rb21_studio_camera"


def sequences(model):
    if model[:8] != b"IDST\x0a\x00\x00\x00" or len(model) < 244:
        raise ValueError("Expected a GoldSrc Studio MDL v10 model.")
    count, start = struct.unpack_from("<ii", model, 164)
    if count < 3 or start < 244 or start + 176 * count > len(model):
        raise ValueError("Invalid Studio MDL sequence table.")
    by_name = {model[index:index + 32].split(b"\0", 1)[0].decode("ascii"): number
               for number, index in enumerate(range(start, start + 176 * count, 176))}
    return {name: by_name[name] for name in CLIPS}


def preview_map(source, clips):
    bsp = bytearray(source)
    if len(bsp) < 124 or struct.unpack_from("<i", bsp)[0] != 30:
        raise ValueError("Expected the compiled GoldSrc BSP v30 test room.")
    offset, size = struct.unpack_from("<ii", bsp, 4)
    if offset < 124 or size <= 0 or offset + size > len(bsp):
        raise ValueError("Invalid GoldSrc BSP entity lump.")
    entities = bsp[offset:offset + size].rstrip(b"\0").decode("ascii")
    entities, removed = re.subn(r'\{[^{}]*"classname"\s+"cycler"[^{}]*\}', "", entities)
    if removed != 1 or '"info_player_start"' not in entities:
        raise ValueError("Expected the test room with one cycler and a player start.")

    def entity(**keys):
        return "\n{\n" + "".join(f'"{key}" "{val}"\n' for key, val in keys.items()) + "}\n"

    for name, x in CLIPS.items():
        entities += entity(classname="cycler", origin=f"{x} -13 0", angle=90, model=MODEL,
                           targetname="rb21_play", sequence=clips[name])
    entities += entity(classname="trigger_auto", target="rb21_play", delay="2")
    lump = entities.encode("ascii") + b"\0"
    bsp.extend(bytes(-len(bsp) % 4))
    struct.pack_into("<ii", bsp, 4, len(bsp), len(lump))
    bsp.extend(lump)
    return bsp


def observed_motion(screenshots):
    frames = []
    for file in screenshots:
        with Image.open(file) as screenshot:
            frames.append(screenshot.convert("RGB"))
    if len(frames) != 12 or len({frame.size for frame in frames}) != 1:
        raise ValueError("Half-Life did not capture 12 comparable frames.")
    width, height = frames[0].size

    def changed(frame, bounds):
        x0, y0, x1, y1 = bounds
        region = (int(x0*width), int(y0*height), int(x1*width), int(y1*height))
        diff = ImageChops.difference(frames[0].crop(region), frame.crop(region))
        return sum(max(pixel) > 12 for pixel in diff.getdata())

    regions = {"walk_forward": (.625, .46, .79, .86), "run_forward": (.41, .46, .59, .86),
               "death_back": (.23, .46, .401, .86), "background": (.02, .15, .20, .61)}
    report = {key: [changed(frame, bounds) for frame in frames[1:]] for key, bounds in regions.items()}
    if max(report["background"]) > 20 or any(max(report[name]) < 500 for name in CLIPS):
        raise ValueError(f"Missing isolated motion for a named Half-Life sequence: {report}")
    return report


def fixture_bsp(fixture, work, game, compilers):
    if fixture.suffix.lower() == ".bsp":
        return fixture.read_bytes()
    if fixture.suffix.lower() != ".map" or not compilers:
        raise ValueError("Supply a GoldSrc BSP, or a source MAP with --compilers.")
    wad = game / "valve" / "halflife.wad"
    if not wad.is_file():
        raise ValueError("The Half-Life installation is missing valve/halflife.wad.")
    generated = work / "fixture.map"
    source = fixture.read_text(encoding="ascii").replace('"wad" "halflife.wad"', f'"wad" "{wad}"')
    generated.write_text(source, encoding="ascii")
    for name in ("sdHLCSG", "sdHLBSP", "sdHLVIS", "sdHLRAD"):
        subprocess.run([str(compilers / f"{name}.exe"), str(generated)], cwd=work, check=True, timeout=90)
    return generated.with_suffix(".bsp").read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("steam_game", type=Path, help="Directory containing hl.exe and valve/")
    parser.add_argument("fixture", type=Path, help="Source or compiled GoldSrc QA room with one cycler")
    parser.add_argument("model", type=Path, help="Studio MDL exported from Character Creator")
    parser.add_argument("output", type=Path, help="Directory for local test evidence")
    parser.add_argument("--compilers", type=Path, help="Directory with the four Windows SDHLT tools")
    args = parser.parse_args()
    game = args.steam_game.resolve()
    model = args.model.read_bytes()
    clips = sequences(model)
    args.output.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="rb21_qa_", dir=game) as temp:
        mod = Path(temp)
        (mod / "models").mkdir()
        (mod / MODEL).write_bytes(model)
        (mod / "maps").mkdir()
        compilers = args.compilers.resolve() if args.compilers is not None else None
        bsp = preview_map(fixture_bsp(args.fixture, mod, game, compilers), clips)
        (mod / "maps" / f"{ROOM}.bsp").write_bytes(bsp)
        (mod / "liblist.gam").write_text('game "RB21 Local QA"\nfallback_dir "valve"\n'
                                        'gamedll "dlls\\hl.dll"\ntype "Singleplayer_only"\n', encoding="ascii")
        for folder in ("dlls", "cl_dlls"):
            shutil.copytree(game / "valve" / folder, mod / folder)
        commands = ["sv_lan 1", "deathmatch 0", "fps_max 60", "hud_draw 0", "r_drawviewmodel 0",
                    f"map {ROOM}", "hideconsole"] + ["wait"]*160
        commands += (["snapshot"] + ["wait"]*11)*12 + ["wait"]*12 + ["quit"]
        (mod / "capture.cfg").write_text("\n".join(commands) + "\n", encoding="ascii")
        subprocess.run([str(game / "hl.exe"), "-game", mod.name, "-window", "-w", "1280", "-h", "800",
                        "-nojoy", "+exec", "capture.cfg"], cwd=game, check=True, timeout=90)
        screenshots = sorted(mod.glob(f"{ROOM}*.bmp"))
        for screenshot in screenshots:
            with Image.open(screenshot) as frame:
                frame.save(args.output / f"{screenshot.stem}.png")
        report = {"sha256": hashlib.sha256(model).hexdigest(), "mapSHA256": hashlib.sha256(bsp).hexdigest(),
                  "namedSequences": clips, "changedPixels": observed_motion(screenshots)}
        (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Half-Life loaded the Studio MDL and rendered motion in walk_forward, run_forward and death_back.")


if __name__ == "__main__":
    main()
