#!/usr/bin/env python3
"""Test map and model texture Apply in a disposable copy of the native editor."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time
import zipfile

from editor_os_input import NativeInput, read_ui_json


def faces(path):
    lines = path.read_text(encoding="ascii").splitlines()
    return sorted(match.groups() for line in lines if (match := re.fullmatch(
        r"(\s*\([^)]*\)\s*\([^)]*\)\s*\([^)]*\)\s+)(\S+)(\s+.+)", line
    )))


def wad_contents(path):
    data = path.read_bytes()
    count, offset = struct.unpack_from("<II", data, 4)
    result = {}
    for index in range(count):
        address, size, _, _, _, name = struct.unpack_from("<IIIBB2x16s", data, offset + 32 * index)
        result[name.rstrip(b"\0").decode("ascii")] = data[address:address + size]
    return result


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("runtime", type=Path)
    cli.add_argument("--output", type=Path, required=True)
    cli.add_argument("--profile", default="fte", choices=("fte", "quake1", "quake2", "quake3", "halflife"))
    cli.add_argument("--three-faces", action="store_true")
    cli.add_argument("--layout-only", action="store_true")
    cli.add_argument("--preset-library", action="store_true")
    cli.add_argument("--project-presets", action="store_true")
    cli.add_argument("--legacy-project", type=Path, help="Copy an existing pre-numbered WAD preset library for migration testing.")
    cli.add_argument("--move-project", action="store_true")
    cli.add_argument("--renderer-restart", action="store_true")
    cli.add_argument("--graph-keyboard", action="store_true")
    cli.add_argument("--recover-source", action="store_true")
    cli.add_argument("--interrupt-apply", choices=("before-asset", "before-register"))
    cli.add_argument("--input-stall-helper", type=Path)
    cli.add_argument("--theme", choices=("light", "dark"), default="dark")
    cli.add_argument("--scale", choices=(100, 150, 200), type=int, default=100)
    cli.add_argument("--size", choices=("1280x720", "1440x900", "1920x1080"), default="1440x900")
    args = cli.parse_args()
    if args.interrupt_apply and not sys.platform.startswith("linux"):
        cli.error("The Apply interruption fixture requires Linux.")
    if args.move_project and args.profile != "quake1":
        cli.error("The project relocation fixture exercises the Quake 1 WAD profile.")
    if args.legacy_project and (not args.project_presets or args.profile not in ("quake1", "halflife")):
        cli.error("Legacy library migration requires the Quake 1 or Half-Life project preset test.")
    if args.renderer_restart and not sys.platform.startswith("linux"):
        cli.error("The renderer restart fixture needs the Linux software renderer.")
    output = args.output.resolve()
    output.mkdir(parents=True)
    runtime = output / "editor"
    shutil.copytree(args.runtime.resolve(), runtime)
    profile_ids = {"quake3": 0, "fte": 1, "quake1": 2, "quake2": 3, "halflife": 4}
    profile = profile_ids[args.profile]
    wad = profile in (2, 4)
    profile_id = ("q3", "fte", "q1", "q2", "hl")[profile]
    data = runtime / "ftebroom"
    project = runtime / {2: "id1", 3: "baseq2", 4: "valve"}.get(profile, "baseq3")
    project.mkdir(exist_ok=True)
    if profile == 3:
        # The editor reads this bounded PCX for the game palette, even when the
        # original room only has an unresolved texture.
        header = bytearray(128)
        header[:4] = bytes((10, 5, 1, 8))
        header[65:68] = bytes((1, 1, 0))
        palette = bytes(c for i in range(256) for c in (i, i, i))
        (project / "pics").mkdir(exist_ok=True)
        (project / "pics/colormap.pcx").write_bytes(header + b"\0\x0c" + palette)
    map_file = data / "maps/texture-studio-qa.map"
    fixture = Path(__file__).resolve().parents[1] / "tests/make-build-fixture.py"
    subprocess.run([sys.executable, str(fixture), str(map_file)], check=True)
    original_texture = "fb_grey" if wad else "ftebroom/grid"
    legacy_material = legacy_wad = None
    legacy_lumps = {}
    if args.legacy_project:
        legacy_material = "rbs_" + hashlib.sha256(f"{profile_id}/builtin-0".encode()).hexdigest()[:11]
        legacy_wad = f"texture-wads/reyond-studio-presets-{profile_id}.wad"
        for relative in ("texture-studio-presets", "texture-wads"):
            shutil.copytree(args.legacy_project / relative, project / relative, dirs_exist_ok=True)
        legacy_lumps = wad_contents(project / legacy_wad)
        assert legacy_material in legacy_lumps and len(legacy_lumps) == 134
        original_texture = legacy_material
    key = "when-the-sky-went-dark" if args.profile == "fte" else args.profile
    style = "quake-standard" if profile == 2 else "valve220" if profile in (3, 4) else "quake3"
    source = map_file.read_text(encoding="ascii").replace("ftebroom/grid", original_texture)
    source = source.replace(" 0 0 0 1 1 0 0 0", " 7 -3 0 2 1" if wad else " 7 -3 0 2 1 0 0 0")
    if wad:
        magic = b"WAD2" if profile == 2 else b"WAD3"
        (project / "prior.wad").write_bytes(magic + struct.pack("<II", 0, 12))
        wad_refs = f"prior.wad;{legacy_wad}" if legacy_wad else "prior.wad"
        source = source.replace('"classname" "worldspawn"\n', f'"classname" "worldspawn"\n"wad" "{wad_refs}"\n', 1)
    selected_texture = "qa_selected" if wad else "ftebroom/qa_selected"
    if args.three_faces:
        substitutions = iter((7, 19, 37))
        source = re.sub(re.escape(original_texture) + r" 7 -3",
                        lambda _: f"{selected_texture} {next(substitutions)} -3", source, count=3)
    map_file.write_text(f"// ReyondBroom profile={key} format={style}\n" + source, encoding="ascii")
    if args.three_faces:
        assert sum(material == selected_texture for _, material, _ in faces(map_file)) == 3
    for directory in (runtime, data):
        (directory / "ftebroom-project.txt").write_text(project.as_posix(), encoding="ascii")
        settings = f'{profile} 0 "{project.as_posix()}" "" "" "" 1 5 "" 1 1\n'
        (directory / "ftebroom-game-profile.txt").write_text(settings, encoding="ascii")
        quake = f'"" "{project.as_posix()}" "" "" 0 0 "" 0\n'
        (directory / "ftebroom-quake1.txt").write_text(quake, encoding="ascii")
        legacy = f'"{project.as_posix()}" "{project.as_posix()}" "" "" "" 0\n'
        (directory / "reyondbroom-legacy-games.txt").write_text(legacy, encoding="ascii")
    state_path = data / "compilation-ui-live.json"
    state_path.unlink(missing_ok=True)
    config = data / "texture-studio-qa.cfg"
    mount = f'set ftebroom_assetpath "{project.as_posix()}"\nfs_restart\n' if args.project_presets else ""
    config.write_text("exec ftebroom.cfg\nset ftebroom_skip_setup 1\n"
                      f"{mount}"
                      f'set ftebroom_test_import_root "{output.as_posix()}"\n'
                      "in 7 mapedit maps/texture-studio-qa.map\n"
                      f"in 9 ftebroom_test_desktop reyon8qa {args.theme}\n",
                      encoding="ascii")
    engine = runtime / ("bin/fteqw64.exe" if os.name == "nt" else "fteqw-gl64")
    width, height = args.size.split("x")
    flags = ["-nohome", "-noupdates", "-nosound", "-window", "-width", width, "-height", height]
    if renderer := os.environ.get("RB_QA_RENDERER"):
        flags += ["+set", "vid_renderer", renderer]
    runtime_suffix = ".wad" if wad else ".wal" if profile == 3 else ".tga"
    folder = project / ("texture-wads" if wad else "textures/reyond-studio")
    shared_wad = folder / f"reyond-studio-presets-{profile_id}.wad"
    def runtime_bytes(path, material):
        return wad_contents(path)[material.removeprefix("textures/")] if wad else path.read_bytes()

    env = os.environ.copy()
    decoder = runtime / ("tools/texture-studio.exe" if os.name == "nt" else "tools/texture-studio")
    if args.input_stall_helper:
        actual_decoder = decoder.with_name("real-" + decoder.name)
        decoder.rename(actual_decoder)
        shutil.copy2(args.input_stall_helper.resolve(), decoder)
    if args.interrupt_apply:
        hook = output / "interrupt-asset-rename.so"
        fixture = Path(__file__).resolve().parents[1] / "tests/fixtures/texture-studio/interrupt-asset-rename.c"
        subprocess.run(["cc", "-shared", "-fPIC", str(fixture), "-ldl", "-o", str(hook)], check=True)
        env["LD_PRELOAD"] = str(hook)
        env["RB_TEXTURE_TEST_INTERRUPT_FOLDER"] = str(folder)
        env["RB_TEXTURE_TEST_INTERRUPT_EXTENSION"] = (
            ".reyond.json" if args.interrupt_apply == "before-asset" else runtime_suffix)

    def launch(log, environment):
        return subprocess.Popen([str(engine), *flags, "+exec", config.name], cwd=runtime,
                                stdout=log, stderr=subprocess.STDOUT, env=environment)

    with (output / "engine.log").open("wb") as log:
        game = launch(log, env)
        try:
            def live():
                return read_ui_json(state_path)

            def map_open():
                path = Path(live()["document"])
                return (runtime / path).resolve() == map_file

            def wait(predicate, timeout=25):
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    if game.poll() is not None:
                        raise RuntimeError(f"The isolated editor exited with {game.returncode}")
                    try:
                        value = predicate()
                        if value:
                            return value
                    except (FileNotFoundError, PermissionError, KeyError):
                        pass
                    time.sleep(.1)
                raise TimeoutError("The editor did not report the expected state")

            ui = NativeInput(game, state_path, live, wait, output)

            def locate(name):
                rect = wait(lambda: live()["globalWidgets"].get(name))
                studio = live()["globalWidgets"].get("texture-studio/controls")
                main_controls = not name.startswith("texture-studio/project/") or name == "texture-studio/project/offer"
                if (studio and name.startswith("texture-studio/") and main_controls
                        and studio["x"] <= rect["x"] + rect["width"] * .5 <= studio["x"] + studio["width"]):
                    middle = rect["y"] + rect["height"] * .5
                    if not studio["y"] <= middle <= studio["y"] + studio["height"]:
                        ui.move(studio["x"] + studio["width"] - 20, studio["y"] + 10)
                        for _ in range(30):
                            ui.scroll(-1 if middle > studio["y"] else 1)
                            rect = wait(lambda: live()["globalWidgets"].get(name))
                            middle = rect["y"] + rect["height"] * .5
                            if studio["y"] <= middle <= studio["y"] + studio["height"]:
                                break
                return rect

            def click(name):
                if (not args.project_presets and not name.startswith("texture-studio/project/")
                        and (offer := live()["globalWidgets"].get("texture-studio/project/later"))):
                    ui.click(offer["x"] + offer["width"] * .5, offer["y"] + offer["height"] * .5)
                if name == "texture-studio/close" and (tab := live()["globalWidgets"].get("texture-studio/tab")):
                    ui.click(tab["x"] + tab["width"] * .5, tab["y"] + tab["height"] * .5, button=2)
                    return
                rect = locate(name)
                ui.click(rect["x"] + rect["width"] * .5, rect["y"] + rect["height"] * .5)

            def edit(name, selection=False):
                click("menu/Edit")
                if selection:
                    click("Menu/Edit/Selection")
                click("Menu/Edit/" + name)

            def save_when(predicate):
                click("menu/File")
                click("Menu/File/Save")
                return wait(lambda: (rows if predicate(rows := faces(map_file)) else None))

            def choose(path):
                click("choose-file/file-path")
                ui.key("a", ("ctrl",))
                ui.text(path.as_posix())
                wait(lambda: live()["globalWidgets"].get("choose-file/path-value", {}).get("value") == path.as_posix())
                click("choose-file/file-choose")

            wait(lambda: live()["globalWidgets"].get("menu/Assets"))
            wait(map_open)
            edit("Select All", selection=True)
            if args.project_presets:
                save_when(lambda rows: rows and "// Format:" in map_file.read_text(encoding="ascii"))
            if args.scale != 100:
                ui.console(f"ftebroom_test_run {60 + (args.scale - 100) // 25}")
            click("menu/Assets")
            click("Menu/Assets/Texture Studio...")

            if args.project_presets:
                studio = lambda: live()["globalWidgets"]["texture-studio/render-state"]
                assert wait(lambda: studio()["docked"])
                assert studio()["projectAvailable"], studio()["error"]
                assert studio()["projectImported"] == bool(legacy_wad)
                original = [(geometry, uv) for geometry, _, uv in faces(map_file)]
                click("texture-studio/project/later")
                wait(lambda: "texture-studio/project/later" not in live()["globalWidgets"])
                offer = locate("texture-studio/project/offer")
                ui.move(offer["x"] + offer["width"] * .5, offer["y"] + offer["height"] * .5)
                if os.name == "nt":
                    scan = ui.user.MapVirtualKeyW(ord("W"), 0)
                    assert scan and ui.windows_focused()
                    ui.user.SendMessageW(ui.hwnd, 0x100, ord("W"), 1 | (scan << 16))
                else:
                    keycode = ui.d.keysym_to_keycode(ui.XK.string_to_keysym("w"))
                    ui.xtest.fake_input(ui.d, ui.X.KeyPress, keycode)
                    ui.d.sync()
                try:
                    wait(lambda: studio()["cameraForward"] == 1)
                finally:
                    if os.name == "nt":
                        ui.user.SendMessageW(ui.hwnd, 0x101, ord("W"), 1 | (scan << 16) | (3 << 30))
                    else:
                        ui.xtest.fake_input(ui.d, ui.X.KeyRelease, keycode)
                        ui.d.sync()
                wait(lambda: studio()["cameraForward"] == 0)
                click("texture-studio/project/offer")
                click("texture-studio/project/import")
                metadata = project / "texture-studio-presets" / profile_id
                manifest_file = metadata / "manifest.json"
                wait(lambda: (studio()["projectImported"] and manifest_file.is_file() and
                              read_ui_json(manifest_file).get("catalogNames")) or studio()["error"], timeout=360)
                assert studio()["projectImported"] and not studio()["error"], studio()["error"]
                manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
                assert manifest["profile"] == profile_id
                if not legacy_wad:
                    assert len(manifest["files"]) == (1 if wad else 537 if profile == 1 else 135 if profile == 0 else 134)
                for name, expected in manifest["files"].items():
                    assert hashlib.sha256((project / name).read_bytes()).hexdigest() == expected, name
                key = "builtin-0"
                assert len(set(manifest["catalogNames"].values())) == 134
                assert all(re.fullmatch(r"[a-z]+_\d+", name) for name in manifest["catalogNames"].values())
                assert manifest["catalogNames"][key] == "brick_1"
                material = "brick_1" if wad else "reyond-studio/brick_1"
                relative = f"texture-wads/reyond-studio-presets-{profile_id}.wad" if wad else f"textures/{material}{runtime_suffix}"
                resource = project / relative
                if wad:
                    assert resource.read_bytes()[:4] == (b"WAD2" if profile == 2 else b"WAD3")
                    lumps = wad_contents(resource)
                    assert material in lumps and len(lumps) == 134 + len(legacy_lumps)
                    if legacy_material:
                        assert all(lumps[name] == data for name, data in legacy_lumps.items())
                        assert lumps[material][16:] == lumps[legacy_material][16:]
                if profile == 3:
                    assert resource.read_bytes()[:32].rstrip(b"\0").decode() == material
                if profile <= 1:
                    script = f"reyond-studio-presets-{profile_id}"
                    assert script in (project / "scripts/shaderlist.txt").read_text()
                    definition = (project / f"scripts/{script}.shader").read_text()
                    assert f"textures/{material}.tga" in definition
                    if profile == 1:
                        assert f"normalmap textures/{material}_norm.tga" in definition
                        assert f"specularmap $nosrgb:textures/{material}_gloss.tga" in definition
                        assert manifest["fteWallGloss"]
                if studio()["projectPreset"] != key:
                    click(f"texture-studio/project/preset/{key}")
                wait(lambda: studio()["projectPreset"] == key and studio()["previewReady"] and not studio()["busy"]
                     and studio().get("projectMaterialReady", True))
                click("texture-studio/project/apply")
                applied = save_when(lambda rows: bool(rows) and
                                    all(row[1].removeprefix("textures/") == material for row in rows))
                assert [(geometry, uv) for geometry, _, uv in applied] == original
                if wad:
                    assert relative in map_file.read_text() and "prior.wad" in map_file.read_text()
                previous = studio()["revision"]
                seed = locate("texture-studio/seed")
                ui.click(seed["x"] + 30, seed["y"] + seed["height"] * .5)
                ui.key("a", ("ctrl",))
                ui.text("21991" if legacy_wad else "8128")
                ui.key("enter")
                wait(lambda: studio()["revision"] != previous and studio()["previewReady"] and not studio()["busy"])
                if profile == 0 and sys.platform.startswith("linux"):
                    import fcntl
                    with (metadata / ".lock").open("rb") as project_lock:
                        fcntl.flock(project_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                        click("texture-studio/project/update")
                        wait(lambda: "preset library lock" in studio()["error"], timeout=90)
                        assert manifest == json.loads(manifest_file.read_text(encoding="utf-8"))
                        assert hashlib.sha256(resource.read_bytes()).hexdigest() == manifest["files"][relative]
                click("texture-studio/project/update")
                custom_source = metadata / f"{key}.rbtex.json"
                def updated():
                    if not custom_source.is_file():
                        return None
                    files = json.loads(manifest_file.read_text(encoding="utf-8"))["files"]
                    return files if files[relative] != manifest["files"][relative] else None
                changed = wait(lambda: updated() or studio()["error"], timeout=90)
                assert not studio()["error"], studio()["error"]
                assert hashlib.sha256(resource.read_bytes()).hexdigest() == changed[relative]
                if wad:
                    new_lumps = wad_contents(resource)
                    assert new_lumps.pop(material) != lumps.pop(material)
                    if legacy_material:
                        assert new_lumps.pop(legacy_material) != lumps.pop(legacy_material)
                    assert new_lumps == lumps
                assert faces(map_file) == applied
                click("texture-studio/project/offer")
                click("texture-studio/project/import")
                wait(lambda: not studio()["busy"])
                assert not studio()["error"], studio()["error"]
                assert hashlib.sha256(resource.read_bytes()).hexdigest() == changed[relative]
                if profile == 1:
                    # Material updates reload engine resources while the map remains open.
                    snapshot = data / "data/export-map.map"
                    snapshot.unlink(missing_ok=True)
                    ui.console("ftebroom_export_map")
                    wait(snapshot.is_file)
                    exported = faces(snapshot)
                    assert len(exported) == len(applied)
                    assert {row[1].strip('"').removeprefix("textures/") for row in exported} == {material}
                from PIL import ImageGrab
                x, y = ui.position()
                scale_x, scale_y = getattr(ui, "scale", (1, 1))
                ImageGrab.grab(bbox=(x, y, x + round(int(width) * scale_x),
                                     y + round(int(height) * scale_y))).save(output / "project-presets.png")
                print(f"PASS {args.profile}: imported 134 native presets, applied one to selected brushes, and updated it in place")
                if legacy_wad:
                    print("PASS legacy WAD migration preserves each original lump and updates its alias with the named material")
                return

            if args.input_stall_helper:
                def current():
                    return live()["globalWidgets"]["texture-studio/render-state"]

                wait(lambda: current()["previewReady"] and not current()["busy"])
                saved = output / "before-opening.rbtex.json"
                click("texture-studio/save")
                choose(saved)
                wait(saved.is_file)
                original = saved.read_bytes()
                before = current()["revision"]

                def open_source(source=saved):
                    click("texture-studio/open")
                    click("texture-studio/open-choose")
                    choose(source)

                open_source()
                wait(lambda: current()["readingSource"])
                started = time.monotonic()
                # The application still processes input while its child is stalled.
                click("texture-studio/cancel-input")
                wait(lambda: not current()["readingSource"], timeout=5)
                cancelled = time.monotonic() - started
                assert current()["revision"] == before and current()["previewReady"]

                open_source()
                wait(lambda: current()["readingSource"])
                started = time.monotonic()
                wait(lambda: "input decoder timed out" in current()["error"], timeout=36)
                elapsed = time.monotonic() - started
                assert 28 <= elapsed < 36, elapsed
                assert current()["revision"] == before and current()["previewReady"]
                assert saved.read_bytes() == original

                actual_decoder.replace(decoder)
                open_source()
                def reopened():
                    state = current()
                    if state["error"]:
                        raise RuntimeError(state["error"])
                    return state["revision"] > before and state["previewReady"]
                wait(reopened)
                assert not current()["error"] and saved.read_bytes() == original
                future = output / "future-format.rbtex.json"
                future.write_bytes(b'{ "schemaVersion" : 999, "futureSetting": true }\n')
                open_source(future)
                wait(lambda: current()["readOnly"] and not current()["readingSource"])
                click("texture-studio/save")
                copied = output / "future-format-copy.rbtex.json"
                choose(copied)
                wait(copied.is_file)
                assert copied.read_bytes() == future.read_bytes() and not current()["error"]
                print(json.dumps({"status": "PASS", "cancelledSeconds": cancelled, "timeoutSeconds": elapsed,
                                  "draftUnchanged": True, "reopenedWithRealDecoder": True,
                                  "futureSourcePreserved": True}))
                return

            if args.interrupt_apply:
                original_map = map_file.read_bytes()
                recovery = output / "saved-before-interruption.rbtex.json"
                click("texture-studio/save")
                choose(recovery)
                wait(recovery.is_file)
                wait(lambda: live()["globalWidgets"]["texture-studio/render-state"]["faceCount"] > 0)
                action = wait(lambda: live()["globalWidgets"].get("texture-studio/apply"))
                try:
                    ui.click(action["x"] + action["width"] / 2, action["y"] + action["height"] / 2)
                except (RuntimeError, TimeoutError, FileNotFoundError):
                    if game.poll() != 91:
                        raise
                assert game.wait(timeout=25) == 91, "The fault hook did not interrupt Apply"
                assert map_file.read_bytes() == original_map
                manifest_file, = folder.glob("*.reyond.json")
                asset_stem = manifest_file.name.removesuffix(".reyond.json")
                manifest = json.loads(manifest_file.read_bytes())
                runtime_file = shared_wad if wad else folder / (asset_stem + runtime_suffix)
                source_file = folder / (asset_stem + ".rbtex.json")
                assert hashlib.sha256(source_file.read_bytes()).hexdigest() == manifest["sourceHash"]
                if args.interrupt_apply == "before-asset":
                    assert not runtime_file.exists()
                else:
                    assert hashlib.sha256(runtime_bytes(runtime_file, manifest["material"])).hexdigest() == manifest["runtimeHash"]

                log.write(b"\n--- Restart after interrupted Apply ---\n")
                log.flush()
                state_path.unlink(missing_ok=True)
                game = launch(log, os.environ.copy())
                ui = NativeInput(game, state_path, live, wait, output)
                wait(map_open)
                edit("Select All", selection=True)
                click("menu/Assets")
                click("Menu/Assets/Texture Studio...")
                click("texture-studio/open")
                click("texture-studio/open-choose")
                previous = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
                choose(recovery)
                wait(lambda: (state := live()["globalWidgets"]["texture-studio/render-state"])["previewReady"]
                     and state["revision"] > previous)
                click("texture-studio/apply")
                wait(runtime_file.is_file)
                generated = manifest["material"].removeprefix("textures/")
                wait(lambda: not live()["globalWidgets"]["texture-studio/render-state"]["busy"])
                save_when(lambda rows: bool(rows) and all(row[1].endswith(generated) for row in rows))
                assert list(folder.glob("*.reyond.json")) == [manifest_file]
                assert hashlib.sha256(runtime_bytes(runtime_file, manifest["material"])).hexdigest() == manifest["runtimeHash"]
                print(f"PASS restart after {args.interrupt_apply} interruption preserves map and retries the same asset")
                return

            if args.renderer_restart:
                def current():
                    return live()["globalWidgets"]["texture-studio/render-state"]

                def preview_frame():
                    from PIL import ImageGrab
                    box = wait(lambda: live()["globalWidgets"].get("texture-studio/preview-image"))
                    ox, oy = ui.position()
                    x, y = int(ox + box["x"]), int(oy + box["y"])
                    width, height = int(box["width"]), int(box["height"])
                    return ImageGrab.grab(bbox=(x + 6, y + 6, x + width - 6, y + height - 6))

                wait(lambda: current()["previewReady"])
                click("texture-studio/generate")
                wait(lambda: current()["fullSize"])
                click("texture-studio/new")
                wait(lambda: current()["catalogThumbnails"] > 0)
                click("texture-studio/catalog-cancel")
                revision = current()["revision"]
                before = preview_frame()
                old_log = (output / "engine.log").stat().st_size
                ui.console("vid_restart")
                wait(lambda: b"restored ImGui atlas after renderer restart" in
                     (output / "engine.log").read_bytes()[old_log:])
                wait(lambda: current()["previewReady"] and current()["fullSize"] and
                     current()["catalogThumbnails"] == 0)
                assert current()["revision"] == revision
                after = preview_frame()
                before.save(output / "before-renderer-restart.png")
                after.save(output / "after-renderer-restart.png")
                assert before.tobytes() == after.tobytes(), "Restarted renderer changed the baked preview"
                click("texture-studio/new")
                wait(lambda: current()["catalogThumbnails"] > 0)
                print("PASS the restarted renderer restores the same full bake and rebuilds catalogue thumbnails")
                return

            if args.recover_source:
                recovery = data / "data/texture-studio/recovery"
                existing = set(recovery.glob("*.rbtex.json"))
                def ready_revision():
                    current = live()["globalWidgets"]["texture-studio/render-state"]
                    return current["revision"] if current["previewReady"] and not current["busy"] else None

                previous = wait(ready_revision)
                click("texture-studio/randomize-parameters")
                wait(lambda: (revision := ready_revision()) and revision > previous)
                click("texture-studio/close")
                backup, = wait(lambda: set(recovery.glob("*.rbtex.json")) - existing)
                original = json.loads(backup.read_text(encoding="utf-8"))
                game.kill()
                game.wait(timeout=15)
                state_path.unlink(missing_ok=True)
                game = launch(log, env)
                ui = NativeInput(game, state_path, live, wait, output)
                wait(map_open)
                click("menu/Assets")
                click("Menu/Assets/Texture Studio...")
                click("texture-studio/recover")
                click("texture-studio/recover/" + backup.name.removesuffix(".rbtex.json"))
                wait(ready_revision)
                click("texture-studio/save")
                restored = output / "recovered.rbtex.json"
                choose(restored)
                wait(restored.is_file)
                recovered = json.loads(restored.read_text(encoding="utf-8"))
                assert original.pop("documentId") != recovered.pop("documentId")
                assert recovered == original
                print("PASS closing an unsaved Texture Studio panel preserves the exact source for recovery after restart")
                return

            if args.preset_library:
                original_source = output / "saved-before-library-edit.rbtex.json"
                click("texture-studio/save")
                choose(original_source)
                wait(original_source.is_file)
                original_hash = hashlib.sha256(original_source.read_bytes()).hexdigest()
                click("texture-studio/save-preset")
                click("texture-studio/save-preset-name")
                ui.text("User Bricks")
                click("texture-studio/save-preset-confirm")
                library = data / "data/texture-studio/library.json"
                folder = library.parent / "user-presets"

                def presets(count):
                    saved = json.loads(library.read_text(encoding="utf-8")).get("userPresets", [])
                    return saved if len(saved) == count else None

                (first, _), = wait(lambda: presets(1))
                expected = json.loads((folder / (first + ".rbtex.json")).read_text(encoding="utf-8"))
                click("texture-studio/new")
                click("texture-studio/preset-search")
                ui.text("User Bricks")
                rect = wait(lambda: live()["globalWidgets"].get("texture-studio/user-preset/" + first))
                ui.click(rect["x"] + 20, rect["y"] + rect["height"] * .5, button=3)
                click("texture-studio/user-preset/duplicate")
                click("texture-studio/user-preset/duplicate-name")
                ui.text("User Bricks Copy")
                click("texture-studio/user-preset/duplicate-confirm")
                (first_again, _), (second, name) = wait(lambda: presets(2))
                assert first == first_again and first != second and name == "User Bricks Copy"
                duplicate = folder / (second + ".rbtex.json")
                copy = json.loads(duplicate.read_text(encoding="utf-8"))
                assert copy == {**expected, "name": name, "documentId": second}
                rect = wait(lambda: live()["globalWidgets"].get("texture-studio/user-preset/" + first))
                ui.click(rect["x"] + 20, rect["y"] + rect["height"] * .5, button=3)
                click("texture-studio/user-preset/remove")
                click("texture-studio/user-preset/remove-confirm")
                assert wait(lambda: presets(1)) == [[second, name]]
                assert not (folder / (first + ".rbtex.json")).exists()
                assert json.loads(duplicate.read_text(encoding="utf-8")) == copy
                assert hashlib.sha256(original_source.read_bytes()).hexdigest() == original_hash
                print("PASS duplicate and remove user presets preserve other entries and the saved source")
                return

            if args.layout_only:
                widgets = wait(lambda: (items if (items := live()["globalWidgets"]).get(
                    "texture-studio/render-state", {}).get("previewReady") else None))
                state = widgets["texture-studio/render-state"]
                preview = locate("texture-studio/preview-image")
                if os.name != "nt":
                    from PIL import ImageGrab
                    ImageGrab.grab().save(output / "quick.png")
                assert state["scale"] == args.scale / 100 and state["theme"] == (3 if args.theme == "light" else 0)
                assert live()["display"] == [int(width), int(height)]
                assert min(preview["width"], preview["height"]) >= 64, preview
                controls = ("new", "import", "save", "save-package", "export", "apply",
                            "preview-mode", "quick-tab", "layers-tab", "graph-tab")
                for name in controls:
                    rect = locate(f"texture-studio/{name}")
                    assert 0 <= rect["x"] < rect["x"] + rect["width"] <= int(width), rect
                    current = live()["globalWidgets"]
                    clip = current["texture-studio/controls"]
                    edge = clip["y"] + clip["height"]
                    assert clip["y"] <= rect["y"] + rect["height"] * .5 <= edge, rect
                    for other in controls:
                        box = current.get(f"texture-studio/{other}")
                        if other == name or not box or not clip["y"] <= box["y"] + box["height"] * .5 <= edge:
                            continue
                        assert (rect["x"] + rect["width"] <= box["x"] or box["x"] + box["width"] <= rect["x"] or
                                rect["y"] + rect["height"] <= box["y"] or box["y"] + box["height"] <= rect["y"]), (rect, box)
                click("texture-studio/layers-tab")
                wait(lambda: live()["globalWidgets"].get("texture-studio/layer-add"))
                click("texture-studio/graph-tab")
                wait(lambda: live()["globalWidgets"].get("texture-studio/graph-add"))
                click("texture-studio/quick-tab")
                (output / "layout.json").write_text(json.dumps(widgets, indent=2), encoding="utf-8")
                print(f"PASS layout {args.theme} {args.size} at {args.scale}%, all three modes reachable")
                return

            if args.profile == "fte":
                click("texture-studio/preview-mode")
                click("texture-studio/preview-mode/5")
                def surface_ready():
                    state = live()["globalWidgets"]["texture-studio/render-state"]
                    if state["error"]:
                        raise RuntimeError(state["error"])
                    return state["previewMode"] == 5 and state["hasSurface"] and state["previewReady"]
                wait(surface_ready)
                if os.name != "nt":
                    from PIL import ImageGrab
                    ImageGrab.grab().save(output / "selected-surface.png")
                click("texture-studio/preview-mode")
                click("texture-studio/preview-mode/4")
                wait(lambda: (state := live()["globalWidgets"]["texture-studio/render-state"])[
                    "previewMode"] == 4 and state["previewReady"])
                if os.name != "nt":
                    ImageGrab.grab().save(output / "target-preview.png")
                click("texture-studio/preview-mode")
                click("texture-studio/preview-mode/0")
                print("PASS selected brush surface preview used the actual map geometry and UV projection")

            def installed():
                state = live()["globalWidgets"]["texture-studio/render-state"]
                if state["error"]:
                    raise RuntimeError(state["error"])
                return next((manifest for manifest in folder.glob("*.reyond.json") if
                             (shared_wad if wad else folder / (manifest.name.removesuffix(".reyond.json") + runtime_suffix)).is_file()), None)

            edit("Deselect All", selection=True)
            wait(lambda: live()["globalWidgets"]["texture-studio/render-state"]["faceCount"] == 0)
            click("texture-studio/apply")
            manifest_file = wait(installed)
            manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
            asset_name = manifest["material"]
            stem = folder / Path(asset_name).name
            runtime_texture = shared_wad if wad else stem.with_suffix(runtime_suffix)
            source = stem.with_suffix(".rbtex.json")
            assert hashlib.sha256(runtime_bytes(runtime_texture, asset_name)).hexdigest() == manifest["runtimeHash"]
            assert re.fullmatch(r"[a-z]+_\d+", stem.name)
            if profile == 1:
                assert (folder / (stem.name + "_norm.tga")).is_file()
                assert (folder / (stem.name + "_gloss.tga")).is_file()
            assert hashlib.sha256(source.read_bytes()).hexdigest() == manifest["sourceHash"]
            if wad:
                content = runtime_texture.read_bytes()
                assert content[:4] == (b"WAD2" if profile == 2 else b"WAD3")
                assert struct.unpack_from("<II", content, 28) == (256, 256)
            elif profile == 3:
                assert struct.unpack_from("<II", runtime_texture.read_bytes(), 32) == (256, 256)
            elif profile == 0:
                assert runtime_texture.read_bytes()[17] == 8

            def expected_textures(rows, applied):
                if len(rows) < 3:
                    return False
                generated = asset_name.removeprefix("textures/")
                def matches(material):
                    if not applied:
                        return material.endswith(selected_texture)
                    return material.endswith(generated) if profile == 1 else material == generated
                if not args.three_faces:
                    return all(matches(row[1]) if applied else row[1].endswith(original_texture) for row in rows)
                return sum(matches(row[1]) for row in rows) == 3 and all(
                    matches(row[1]) or row[1].endswith(original_texture) for row in rows)

            unchanged = save_when(lambda rows: expected_textures(rows, False))
            shutil.copy2(map_file, output / "before-apply.map")
            if wad:
                assert re.search(r'"wad"\s+"prior\.wad"', map_file.read_text(encoding="ascii"))
            print("PASS Generate Asset creates the game texture without changing the map")
            if args.three_faces:
                ui.console(f"set ftebroom_material_query {selected_texture}")
                ui.console("ftebroom_material_select faces")
            else:
                edit("Select All", selection=True)
            wait(lambda: (count := live()["globalWidgets"]["texture-studio/render-state"]["faceCount"]) > 0 and
                 (not args.three_faces or count == 3))
            click("texture-studio/apply")
            applied = save_when(lambda rows: expected_textures(rows, True))
            if args.three_faces:
                def untouched(rows):
                    return sorted((prefix.strip(), uv.strip()) for prefix, material, uv in rows if material.endswith(original_texture))
                assert len(unchanged) == len(applied)
                assert untouched(unchanged) == untouched(applied), "Unselected faces changed"
            if wad:
                text = map_file.read_text(encoding="ascii")
                assert re.search(r'"wad"\s+"prior\.wad;[^"]*' + re.escape(runtime_texture.name), text), text[:300]
            shutil.copy2(map_file, output / "applied.map")
            edit("Undo")
            restored = save_when(lambda rows: expected_textures(rows, False))
            if wad:
                text = map_file.read_text(encoding="ascii")
                assert re.search(r'"wad"\s+"prior\.wad"', text), text[:300]
            shutil.copy2(map_file, output / "restored.map")
            assert sorted((row[0], row[2]) for row in applied) == sorted((row[0], row[2]) for row in restored), (
                "Apply must preserve geometry and UVs. Compare applied.map and restored.map."
            )
            assert runtime_texture.is_file(), "Undo must retain a potentially shared asset"
            edit("Redo")
            assert save_when(lambda rows: expected_textures(rows, True)) == applied
            if wad:
                assert map_file.read_bytes() == (output / "applied.map").read_bytes()
            scope = "three selected faces" if args.three_faces else "the entire selection"
            print(f"PASS {args.profile} Apply, save, Undo and Redo update {scope} and preserve all {len(applied)} face projections")
            click("texture-studio/close")
            click("menu/Assets")
            click("Menu/Assets/Edit Texture Studio source...")
            click("texture-studio/save")
            saved_copy = output / "generated-graph-copy.rbtex.json"
            choose(saved_copy)
            wait(saved_copy.is_file)
            assert json.loads(saved_copy.read_text(encoding="utf-8")) == json.loads(source.read_text(encoding="utf-8"))
            assert hashlib.sha256(source.read_bytes()).hexdigest() == manifest["sourceHash"]
            print("PASS Edit Texture Studio source opens the map material's graph and preserves the original")
            if args.move_project:
                game.kill()
                game.wait(timeout=15)
                moved = output / "Moved Jam QA"
                old_runtime = runtime
                runtime.rename(moved)
                assert not old_runtime.exists()
                runtime = moved
                project = moved / project.relative_to(old_runtime)
                data = moved / data.relative_to(old_runtime)
                map_file = moved / map_file.relative_to(old_runtime)
                state_path = moved / state_path.relative_to(old_runtime)
                engine = moved / engine.relative_to(old_runtime)
                for directory in (runtime, data):
                    (directory / "ftebroom-project.txt").write_text(project.as_posix(), encoding="utf-8")
                    (directory / "ftebroom-game-profile.txt").write_text(
                        f'{profile} 0 "{project.as_posix()}" "" "" "" 1 5 "" 1 1\n', encoding="utf-8")
                    (directory / "ftebroom-quake1.txt").write_text(
                        f'"" "{project.as_posix()}" "" "" 0 0 "" 0\n', encoding="utf-8")
                state_path.unlink(missing_ok=True)
                game = launch(log, env)
                ui = NativeInput(game, state_path, live, wait, output)
                wait(map_open)
                # Assets opens the source of the active material. Reopening the
                # map does not carry over the material selected before restart.
                ui.console("set ca_newbrushtexture " + asset_name.removeprefix("textures/"))
                click("menu/Assets")
                click("Menu/Assets/Edit Texture Studio source...")
                wait(lambda: live()["globalWidgets"]["texture-studio/render-state"]["previewReady"])
                click("texture-studio/save")
                restored_source = output / "relocated-graph-copy.rbtex.json"
                choose(restored_source)
                wait(restored_source.is_file)
                assert restored_source.read_bytes() == saved_copy.read_bytes()
                assert map_file.read_bytes() == (output / "applied.map").read_bytes()
                moved_texture = moved / runtime_texture.relative_to(old_runtime)
                assert hashlib.sha256(runtime_bytes(moved_texture, asset_name)).hexdigest() == manifest["runtimeHash"]
                print("PASS reopening the map after moving its project preserves the relative WAD and editable graph")
                return
            if args.profile != "fte" or args.three_faces:
                return

            click("texture-studio/close")
            ui.console("ftebroom_test_desktop bb121open")
            ui.console("ftebroom_test_desktop bb121save")
            model_file = output / "monster.bbmodel"
            original = wait(lambda: json.loads(model_file.read_text(encoding="utf-8")))
            click("texture-studio/model/source")
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            click("texture-studio/apply-model")

            def model_ready():
                state = live()["globalWidgets"]["texture-studio/render-state"]
                if state["error"]:
                    raise RuntimeError(state["error"])
                return state["revision"] > before and not state["busy"] and state["previewReady"]

            wait(model_ready)
            click("texture-studio/close")

            def save_model_when(predicate):
                click("texture-studio/model/file-menu")
                click("texture-studio/model/save")
                return wait(lambda: (model if predicate(model := json.loads(
                    model_file.read_text(encoding="utf-8"))) else None))

            def generated(model):
                return "reyond_texture_studio" in model["textures"][0]

            after = save_model_when(generated)
            slot = after["textures"][0]
            old = original["textures"][0]
            assert slot["source"] != old["source"] and slot["uuid"] == old["uuid"]
            assert (slot["uv_width"], slot["uv_height"]) == (old["uv_width"], old["uv_height"])
            assert after["elements"] == original["elements"]
            click("texture-studio/model/edit-menu")
            click("texture-studio/model/Undo")
            assert save_model_when(lambda model: not generated(model))["textures"][0]["source"] == old["source"]
            click("texture-studio/model/edit-menu")
            click("texture-studio/model/Redo")
            assert save_model_when(generated)["textures"][0] == slot
            print("PASS Model Editor Apply, save, Undo and Redo preserve the selected slot, UVs and graph")

            click("texture-studio/model/source")
            click("texture-studio/import")
            click("texture-studio/import-license")
            ui.text("CC0-1.0")
            click("texture-studio/import-choose")
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            choose(runtime_texture)
            wait(model_ready)
            click("texture-studio/save-package")
            package = output / "imported-image.rbtexpkg"
            choose(package)
            wait(package.is_file)
            with zipfile.ZipFile(package) as archive:
                imported = json.loads(archive.read("source.rbtex.json"))
                reference, = imported["assetReferences"]
                assert reference["license"] == "CC0-1.0"
                assert archive.read(reference["path"]) == runtime_texture.read_bytes()
                assert {"baseColor", "height", "normal", "ao"} == set(imported["outputs"])
            print("PASS native Image Import and Save Package preserve the original image and license")
            click("texture-studio/generate")
            wait(lambda: live()["globalWidgets"]["texture-studio/render-state"]["fullSize"])
            click("texture-studio/export")
            click("texture-studio/export-dry-run")
            dry_run = wait(lambda: live()["globalWidgets"].get("texture-studio/export-manifest"))
            click("texture-studio/export-destination")
            png = output / "exported-source.png"
            choose(png)
            assert wait(lambda: png.is_file() and png.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"))
            assert png.stat().st_size == dry_run["bytes"]
            assert hashlib.sha256(png.read_bytes()).hexdigest() == dry_run["sha256"]
            print("PASS native Texture Studio PNG matches the exact bytes and SHA-256 shown by Dry run")

            prior_export = b"Existing texture contents\n"
            png.write_bytes(prior_export)
            click("texture-studio/export")
            click("texture-studio/export-destination")
            choose(png)
            wait(lambda: live()["globalWidgets"].get("choose-file/replace-confirm"))
            assert png.read_bytes() == prior_export
            click("choose-file/replace-cancel")
            wait(lambda: not live()["globalWidgets"].get("choose-file/replace-confirm"))
            assert png.read_bytes() == prior_export
            click("choose-file/file-choose")
            click("choose-file/replace-confirm")
            wait(lambda: hashlib.sha256(png.read_bytes()).hexdigest() == dry_run["sha256"])
            print("PASS exporting over an existing file requires confirmation and preserves it on cancel")

            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            click("texture-studio/apply-model")
            wait(model_ready)
            click("texture-studio/close")
            asset_slot = save_model_when(lambda model: bool(model["textures"][0].get("reyond_texture_studio_originals")))["textures"][0]
            digest = imported["assetReferences"][0]["sha256"]
            assert base64.b64decode(asset_slot["reyond_texture_studio_originals"][digest], validate=True) == runtime_texture.read_bytes()
            assert asset_slot["reyond_texture_studio"]["assetReferences"] == imported["assetReferences"]
            click("texture-studio/model/edit-menu")
            click("texture-studio/model/Undo")
            assert save_model_when(lambda model: not model["textures"][0].get("reyond_texture_studio_originals"))["textures"][0] == slot
            click("texture-studio/model/edit-menu")
            click("texture-studio/model/Redo")
            assert save_model_when(lambda model: bool(model["textures"][0].get("reyond_texture_studio_originals")))["textures"][0] == asset_slot
            click("texture-studio/model/source")
            click("texture-studio/save-package")
            restored_package = output / "model-restored.rbtexpkg"
            choose(restored_package)
            wait(restored_package.is_file)
            with zipfile.ZipFile(restored_package) as archive:
                assert archive.read(imported["assetReferences"][0]["path"]) == runtime_texture.read_bytes()
            print("PASS Model Editor opens the saved graph and preserves original bytes through Undo and Redo")

            click("texture-studio/new")
            wait(lambda: live()["globalWidgets"]["texture-studio/render-state"]["catalogThumbnails"] > 0)
            click("texture-studio/preset-search")
            ui.text("Flemish Brick")
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            click("texture-studio/recipe/masonry-flemish-brick/variants")
            click("texture-studio/recipe/masonry-flemish-brick/variants/1")
            wait(model_ready)
            click("texture-studio/layers-tab")
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            click("texture-studio/layer-add")
            wait(model_ready)
            click("texture-studio/save")
            material_file = output / "flemish.rbtex.json"
            choose(material_file)
            graph = wait(lambda: json.loads(material_file.read_text(encoding="utf-8")))
            assert graph["provenance"]["preset"] == "masonry-flemish-brick"
            nodes = {node["id"]: node for node in graph["graph"]["nodes"]}
            assert nodes["stain-mask"]["parameters"]["shape"] == 0  # Speckled
            assert nodes["stain-mask"]["parameters"]["amount"] > 0
            selected = nodes[graph["outputs"]["baseColor"]["node"]]
            assert selected["type"] == "Blend" and selected["label"] == "Color layer"
            print("PASS native preset search, layer editor, and saved source share the same graph")
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            ui.key("z", ("ctrl",))
            wait(model_ready)
            click("texture-studio/save")
            previous = wait(lambda: (doc if (doc := json.loads(material_file.read_text(encoding="utf-8"))) != graph else None))
            assert len(previous["graph"]["nodes"]) < len(graph["graph"]["nodes"])
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            ui.key("y", ("ctrl",))
            wait(model_ready)
            click("texture-studio/save")
            wait(lambda: json.loads(material_file.read_text(encoding="utf-8")) == graph)
            print("PASS Ctrl+Z and Ctrl+Y target the focused Texture Studio history")
            click("texture-studio/quick-tab")
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            click("texture-studio/randomize-parameters")
            wait(model_ready)
            click("texture-studio/save")
            randomized = wait(lambda: (doc if (doc := json.loads(material_file.read_text(encoding="utf-8"))) != graph else None))
            assert randomized["rootSeed"] == graph["rootSeed"]
            assert randomized["graph"]["nodes"] != graph["graph"]["nodes"]
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            ui.key("z", ("ctrl",))
            wait(model_ready)
            click("texture-studio/save")
            wait(lambda: json.loads(material_file.read_text(encoding="utf-8")) == graph)
            print("PASS randomizing Quick parameters preserves the root seed and can be undone")

            click("texture-studio/graph-tab")
            def graph_action(name, navigation="tab"):
                key = "texture-studio/graph-add" if name == "add" else "texture-studio/graph/" + name
                if not args.graph_keyboard:
                    click(key)
                    return
                wait(lambda: live()["globalWidgets"].get(key))
                for _ in range(54):
                    if live()["globalWidgets"].get(key, {}).get("focused"):
                        ui.key("enter")
                        return
                    ui.key(navigation)
                raise AssertionError(f"Could not focus graph control with keyboard: {name}")

            graph_action("node")
            graph_action("node/pattern", "Down")
            graph_action("include")
            graph_action("group")
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            graph_action("group-create")
            wait(model_ready)
            click("texture-studio/save")
            grouped = wait(lambda: (doc if (doc := json.loads(material_file.read_text(encoding="utf-8")))[
                "graph"].get("groups") else None))
            group_id, definition = next(iter(grouped["graph"]["groups"].items()))
            pin = definition["inputs"][0]["name"]
            graph_action("input/" + pin)
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            graph_action("source/detail/value", "Down")
            wait(model_ready)
            click("texture-studio/save")
            connected = wait(lambda: (doc if (doc := json.loads(material_file.read_text(encoding="utf-8"))) != grouped else None))
            assert {"fromNode": "detail", "fromPort": "value", "toNode": group_id, "toPort": pin} in connected[
                "graph"]["edges"]
            print("PASS native graph grouping and typed group input reconnection persist in the source")
            if args.graph_keyboard:
                before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
                graph_action("add")
                graph_action("add/Constant", "Down")
                wait(model_ready)
                click("texture-studio/save")
                with_constant = wait(lambda: (doc if len((doc := json.loads(material_file.read_text(
                    encoding="utf-8")))["graph"]["nodes"]) > len(connected["graph"]["nodes"]) else None))
                graph_action("parameters")
                parameter_key = "texture-studio/graph/parameter-input"
                for _ in range(8):
                    if live()["globalWidgets"].get(parameter_key, {}).get("focused"):
                        break
                    ui.key("tab")
                else:
                    raise AssertionError("Could not focus the node parameter input with Tab")
                if not live()["globalWidgets"][parameter_key].get("active"):
                    ui.key("enter")
                wait(lambda: live()["globalWidgets"][parameter_key].get("active"))
                invalid_parameters = '{"value": 2}'
                ui.key("a", ("ctrl",))
                ui.text(invalid_parameters, paste=True)
                wait(lambda: live()["globalWidgets"][parameter_key].get("value") == invalid_parameters)
                graph_action("parameters-apply")
                wait(lambda: "outside its supported range" in live()["globalWidgets"][
                    "texture-studio/render-state"]["error"])
                graph_action("parameters-cancel")
                graph_action("locate-error")
                before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
                graph_action("delete")
                wait(model_ready)
                click("texture-studio/save")
                wait(lambda: json.loads(material_file.read_text(encoding="utf-8")) == connected)
                before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
                ui.key("z", ("ctrl",))
                wait(model_ready)
                click("texture-studio/save")
                wait(lambda: json.loads(material_file.read_text(encoding="utf-8")) == with_constant)
                print("PASS keyboard-only graph grouping, connecting, error navigation, deleting and undo")
        finally:
            if game.poll() is None:
                game.kill()
            game.wait(timeout=15)


if __name__ == "__main__":
    main()
