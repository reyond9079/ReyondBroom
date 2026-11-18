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


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("runtime", type=Path)
    cli.add_argument("--output", type=Path, required=True)
    cli.add_argument("--profile", default="fte", choices=("fte", "quake1", "quake2", "quake3", "halflife"))
    cli.add_argument("--three-faces", action="store_true")
    cli.add_argument("--layout-only", action="store_true")
    cli.add_argument("--preset-library", action="store_true")
    cli.add_argument("--theme", choices=("light", "dark"), default="dark")
    cli.add_argument("--scale", choices=(100, 150, 200), type=int, default=100)
    cli.add_argument("--size", choices=("1280x720", "1440x900", "1920x1080"), default="1440x900")
    args = cli.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True)
    runtime = output / "editor"
    shutil.copytree(args.runtime.resolve(), runtime)
    profile_ids = {"quake3": 0, "fte": 1, "quake1": 2, "quake2": 3, "halflife": 4}
    profile = profile_ids[args.profile]
    wad = profile in (2, 4)
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
    key = "when-the-sky-went-dark" if args.profile == "fte" else args.profile
    style = "quake-standard" if profile == 2 else "valve220" if profile in (3, 4) else "quake3"
    source = map_file.read_text(encoding="ascii").replace("ftebroom/grid", original_texture)
    source = source.replace(" 0 0 0 1 1 0 0 0", " 7 -3 0 2 1" if wad else " 7 -3 0 2 1 0 0 0")
    if wad:
        magic = b"WAD2" if profile == 2 else b"WAD3"
        (project / "prior.wad").write_bytes(magic + struct.pack("<II", 0, 12))
        source = source.replace('"classname" "worldspawn"\n', '"classname" "worldspawn"\n"wad" "prior.wad"\n', 1)
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
    config.write_text("exec ftebroom.cfg\nset ftebroom_skip_setup 1\n"
                      f'set ftebroom_test_import_root "{output.as_posix()}"\n'
                      "in 7 mapedit maps/texture-studio-qa.map\n"
                      "in 9 ftebroom_select all\n"
                      f"in 9 ftebroom_test_desktop reyon8qa {args.theme}\n",
                      encoding="ascii")
    engine = runtime / ("bin/fteqw64.exe" if os.name == "nt" else "fteqw-gl64")
    width, height = args.size.split("x")
    flags = ["-nohome", "-noupdates", "-nosound", "-window", "-width", width, "-height", height]
    with (output / "engine.log").open("wb") as log:
        game = subprocess.Popen([str(engine), *flags, "+exec", config.name], cwd=runtime,
                                stdout=log, stderr=subprocess.STDOUT)
        try:
            def live():
                return read_ui_json(state_path)

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

            def click(name):
                rect = wait(lambda: live()["globalWidgets"].get(name))
                ui.click(rect["x"] + rect["width"] * .5, rect["y"] + rect["height"] * .5)

            def save_when(predicate):
                click("menu/File")
                click("Menu/File/Save")
                return wait(lambda: (rows if predicate(rows := faces(map_file)) else None))

            def choose(path):
                click("choose-file/file-path")
                ui.key("a", ("ctrl",))
                ui.text(path.as_posix())
                click("choose-file/file-choose")

            wait(lambda: live()["globalWidgets"].get("menu/Assets"))
            if args.scale != 100:
                ui.console(f"ftebroom_test_run {60 + (args.scale - 100) // 25}")
            click("menu/Assets")
            click("Menu/Assets/Texture Studio...")

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
                if os.name != "nt":
                    from PIL import ImageGrab
                    ImageGrab.grab().save(output / "quick.png")
                assert state["scale"] == args.scale / 100 and state["theme"] == (3 if args.theme == "light" else 0)
                assert live()["display"] == [int(width), int(height)]
                preview = widgets["texture-studio/preview-image"]
                assert min(preview["width"], preview["height"]) >= 64, preview
                assert preview["y"] + preview["height"] <= int(height), preview
                controls = ("new", "import", "save", "save-package", "export", "apply",
                            "preview-mode", "quick-tab", "layers-tab", "graph-tab")
                controls = [widgets[f"texture-studio/{name}"] for name in controls]
                for rect in controls:
                    assert 0 <= rect["x"] < rect["x"] + rect["width"] <= int(width), rect
                    assert 0 <= rect["y"] < rect["y"] + rect["height"] <= int(height), rect
                for i, a in enumerate(controls):
                    for b in controls[i + 1:]:
                        assert a["x"] + a["width"] <= b["x"] or b["x"] + b["width"] <= a["x"] or \
                            a["y"] + a["height"] <= b["y"] or b["y"] + b["height"] <= a["y"], (a, b)
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

            folder = project / ("texture-wads" if wad else "textures/reyond-studio")

            def installed():
                state = live()["globalWidgets"]["texture-studio/render-state"]
                if state["error"]:
                    raise RuntimeError(state["error"])
                return next(iter(folder.glob("*.reyond.json")), None)

            ui.console("ftebroom_deselect")
            wait(lambda: live()["globalWidgets"]["texture-studio/render-state"]["faceCount"] == 0)
            click("texture-studio/apply")
            manifest_file = wait(installed)
            manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
            asset_name = manifest["material"]
            stem = folder / Path(asset_name).name
            runtime_texture = stem.with_suffix(".wad" if wad else ".wal" if profile == 3 else ".tga")
            source = stem.with_suffix(".rbtex.json")
            assert hashlib.sha256(runtime_texture.read_bytes()).hexdigest() == manifest["runtimeHash"]
            assert hashlib.sha256(source.read_bytes()).hexdigest() == manifest["sourceHash"]
            if wad:
                content = runtime_texture.read_bytes()
                assert content[:4] == (b"WAD2" if profile == 2 else b"WAD3")
                assert struct.unpack_from("<II", content, 28) == (256, 256)
            elif profile == 3:
                assert struct.unpack_from("<II", runtime_texture.read_bytes(), 32) == (256, 256)

            def expected_textures(rows, applied):
                if len(rows) < 3:
                    return False
                generated = asset_name.removeprefix("textures/")
                if not args.three_faces:
                    return all(row[1].endswith(generated if applied else original_texture) for row in rows)
                changed = generated if applied else selected_texture
                return sum(row[1].endswith(changed) for row in rows) == 3 and all(
                    row[1].endswith((changed, original_texture)) for row in rows)

            unchanged = save_when(lambda rows: expected_textures(rows, False))
            shutil.copy2(map_file, output / "before-apply.map")
            if wad:
                assert re.search(r'"wad"\s+"prior\.wad"', map_file.read_text(encoding="ascii"))
            print("PASS Generate Asset creates the game texture without changing the map")
            if args.three_faces:
                ui.console(f"set ftebroom_material_query {selected_texture}")
                ui.console("ftebroom_material_select faces")
            else:
                ui.console("ftebroom_select all")
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
            ui.console("ftebroom_undo")
            restored = save_when(lambda rows: expected_textures(rows, False))
            if wad:
                text = map_file.read_text(encoding="ascii")
                assert re.search(r'"wad"\s+"prior\.wad"', text), text[:300]
            shutil.copy2(map_file, output / "restored.map")
            assert sorted((row[0], row[2]) for row in applied) == sorted((row[0], row[2]) for row in restored), (
                "Apply must preserve geometry and UVs. Compare applied.map and restored.map."
            )
            assert runtime_texture.is_file(), "Undo must retain a potentially shared asset"
            ui.console("ftebroom_redo")
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
            click("texture-studio/recipe/masonry-flemish-brick")
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
            click("texture-studio/graph/node")
            click("texture-studio/graph/node/pattern")
            click("texture-studio/graph/include")
            click("texture-studio/graph/group")
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            click("texture-studio/graph/group-create")
            wait(model_ready)
            click("texture-studio/save")
            grouped = wait(lambda: (doc if (doc := json.loads(material_file.read_text(encoding="utf-8")))[
                "graph"].get("groups") else None))
            group_id, definition = next(iter(grouped["graph"]["groups"].items()))
            pin = definition["inputs"][0]["name"]
            click("texture-studio/graph/input/" + pin)
            before = live()["globalWidgets"]["texture-studio/render-state"]["revision"]
            click("texture-studio/graph/source/detail/value")
            wait(model_ready)
            click("texture-studio/save")
            connected = wait(lambda: (doc if (doc := json.loads(material_file.read_text(encoding="utf-8"))) != grouped else None))
            assert {"fromNode": "detail", "fromPort": "value", "toNode": group_id, "toPort": pin} in connected[
                "graph"]["edges"]
            print("PASS native graph grouping and typed group input reconnection persist in the source")
        finally:
            if game.poll() is None:
                game.kill()
            game.wait(timeout=15)


if __name__ == "__main__":
    main()
