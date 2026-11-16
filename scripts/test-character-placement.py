#!/usr/bin/env python3
"""Exercise Character Creator's map binding through native input in a private copy."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
from contextlib import contextmanager
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

from PIL import ImageChops, ImageGrab

from editor_os_input import NativeInput, read_ui_json


@contextmanager
def editor_process(arguments, root, stream, hardware=False):
    environment = {**os.environ}
    if hardware:
        environment.pop("LIBGL_ALWAYS_SOFTWARE", None)
    else:
        environment["LIBGL_ALWAYS_SOFTWARE"] = "1"
    if os.name != "nt" and environment.get("DISPLAY"):
        environment.pop("WAYLAND_DISPLAY", None)
        environment["SDL_VIDEODRIVER"] = "x11"
    process = subprocess.Popen(arguments, cwd=root, stdout=stream, stderr=subprocess.STDOUT, env=environment)
    try:
        yield process
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)


def exercise(root, out, compile_assets, document_only=False, startup_placement=False, motion_fixture=None,
             legacy_fixture=None, fit_fixture=None, pose_phase=.25, benchmark=False, export_goldsrc=False,
             export_iqm=None, catalog_only=False):
    data = root / "ftebroom"
    telemetry = data / "placement-ui-live.json"
    telemetry.unlink(missing_ok=True)
    benchmark_result = data / "character-benchmark.json"
    if benchmark:
        benchmark_result.unlink(missing_ok=True)
    game = root / "test-game" / "baseq3"
    game.mkdir(parents=True)
    if compile_assets:
        for folder in (root,data):
            (folder/"ftebroom-project.txt").write_text(game.as_posix(),encoding="utf-8")
        # Give the room a material available to both the compiler and runtime.
        name = Path("textures/rb21/compile-room.tga")
        header = bytearray(18)
        header[2] = 2
        struct.pack_into("<HHBB",header,12,64,64,24,0)
        for folder in (game,data):
            texture = folder/name
            texture.parent.mkdir(parents=True,exist_ok=True)
            texture.write_bytes(bytes(header) + bytes([116,108,92]) * (64*64))
    profile = 4 if export_goldsrc else 1 if startup_placement or fit_fixture or catalog_only else 0
    (root / "ftebroom-game-profile.txt").write_text(f'{profile} 0 "" "" "" "" 1 0 "" 1 1\n')
    fit_command = ""
    if fit_fixture:
        fixture = data/"fit-fixture.rbchar"
        shutil.copy2(fit_fixture,fixture)
        fit_command = f'in 7 ftebroom_character "{fixture.as_posix()}"\n'
    performance_setup = ("vid_vsync 0\ncl_maxfps 0\ncl_yieldcpu 0\n"
                         "set ftebroom_character_benchmark_frames 240\n") if benchmark else ""
    (data / "rb21-placement.cfg").write_text(
        ("exec ftebroom.cfg\n" if startup_placement or fit_fixture or catalog_only else "exec qa-clip.cfg\n")+
        "set ftebroom_placement_qa 1\n"
        +fit_command+performance_setup+
        "in 11 echo RB_CHARACTER_FIXTURE_READY\n"
    )
    engine = root / ("bin/fteqw64.exe" if os.name == "nt" else "fteqw-gl64")
    arguments = [str(engine), "-ftebroom-child-processes", "-basedir", str(root), "-nohome", "-noupdates", "-nosound",
                 "-condebug", "-window", "-width", "1920" if benchmark else "1440",
                 "-height", "1080" if benchmark else "900", "+set",
                 "ftebroom_skip_setup", "1", "+set", "ftebroom_input_qa", "1", "+exec",
                 "rb21-placement.cfg"]
    log = out / "editor.log"
    checks = []
    with log.open("wb") as stream, editor_process(arguments,root,stream,hardware=benchmark) as editor:
        def live():
            return read_ui_json(telemetry, timeout=.5)

        def wait(condition, timeout=35):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                if editor.poll() is not None:
                    raise RuntimeError(f"Editor exited. See {log}")
                try:
                    if result := condition():
                        return result
                except (KeyError, FileNotFoundError, PermissionError, json.JSONDecodeError):
                    pass
                time.sleep(.07)
            raise AssertionError(f"Timed out. Latest character state: {live().get('character')}")

        def check(value, label):
            assert value, label
            checks.append(label)
            (out / "results.json").write_text(json.dumps(checks, indent=2)+"\n")
            print("PASS",label,flush=True)

        if os.name == "nt":
            class WindowsInput(NativeInput):
                def text(self, value):
                    # Use the driver's Unicode paste path for short console commands too.
                    # This keeps commands usable with non-Latin keyboard layouts.
                    super().text(value.ljust(21) if self.in_console else value)

            ui = WindowsInput(editor,telemetry,live,wait,out,focus_required=True)
        else:
            # Xvfb without a window manager has no click-to-focus policy.
            # xdotool also avoids the Xlib driver's capture/focus round trip.
            class XvfbInput:
                def __init__(self):
                    try:
                        windows = subprocess.check_output(
                            ["xdotool","search","--sync","--onlyvisible","--pid",str(editor.pid)],
                            text=True,timeout=8)
                    except subprocess.TimeoutExpired:
                        # Some FTE X11 backends omit _NET_WM_PID. The test uses
                        # a dedicated display, so its editor title is unique.
                        windows = subprocess.check_output(
                            ["xdotool","search","--sync","--onlyvisible","--name","^ReyondBroom:"],
                            text=True,timeout=15)
                    self.window = windows.splitlines()[0]

                def run(self,*args):
                    subprocess.run(["xdotool",*args],check=True,timeout=10)

                def position(self):
                    self.run("windowfocus",self.window)

                def settle(self):
                    time.sleep(.25)

                def move(self,x,y):
                    self.run("mousemove","--window",self.window,str(int(x)),str(int(y)))
                    self.settle()

                def click(self,x,y,button=1,hold=.12):
                    self.move(x,y)
                    self.run("mousedown",str(button),"sleep",str(hold),"mouseup",str(button))
                    self.settle()

                def orbit(self,x,y,target):
                    self.move(x,y)
                    self.run("mousedown","1","sleep",".20","mousemove","--window",self.window,
                             str(int(target)),str(int(y)),"sleep",".20","mouseup","1")
                    self.settle()

                def key(self,key,mods=()):
                    key = {"backspace":"BackSpace", "enter":"Return", "escape":"Escape"}.get(key,key)
                    prefix="+".join((*mods,key))
                    self.run("keydown",prefix,"sleep",".16","keyup",prefix)
                    self.settle()

                def text(self,value):
                    self.run("type","--clearmodifiers",value)

                def console(self,command):
                    self.run("keydown","Shift_L","keydown","Escape","sleep",".2","keyup","Escape","keyup","Shift_L")
                    time.sleep(.25)
                    self.run("type","--clearmodifiers","--delay","15","/"+command)
                    self.run("key","Return","sleep",".35","key","Escape")
                    self.settle()

            ui = XvfbInput()

        def capture():
            if os.name == "nt":
                if ui.hwnd is None:
                    return
                window = ui.types.RECT()
                if not ui.user.GetWindowRect(ui.hwnd,ctypes.byref(window)):
                    return
                return ImageGrab.grab(bbox=(window.left,window.top,window.right,window.bottom),
                                      all_screens=True)
            return ImageGrab.grab()

        def screenshot(name):
            if frame := capture():
                frame.save(out/name)

        def item(key):
            r = wait(lambda:live()["widgets"].get(key))
            return r["x"]+r["width"]*.45, r["y"]+r["height"]*.5

        def click(key):
            ui.settle()
            ui.click(*item(key))

        def hover(key):
            ui.move(*item(key))
            time.sleep(.55)

        def capture_map(label):
            stem = "rb21-" + label
            file = data / "maps" / (stem+".map")
            file.unlink(missing_ok=True)
            ui.console(f'mod_terrain_save "" {stem}')
            wait(file.is_file)
            content = file.read_text(errors="replace")
            (out / file.name).write_text(content)
            return [dict(re.findall(r'"([^"\n]+)"\s+"([^"\n]*)"',entity))
                    for entity in re.findall(r'\{([^{}]*)\}',content)
                    if '"_reyondbroom_placement"' in entity]

        def compile_character(entity):
            room = data / "maps" / "rb21-compile.map"
            generator = Path(__file__).resolve().parents[1] / "tests/make-build-fixture.py"
            subprocess.run([sys.executable,str(generator),str(room)],check=True)
            manifest_name = entity["model"]+".reyond.json"
            source_manifest = json.loads((data/manifest_name).read_text())
            scene = room.read_text(encoding="ascii").replace("ftebroom/grid","rb21/compile-room")
            scene = scene.replace('"origin" "0 -160 64"','"origin" "0 -160 64"\n"angle" "100"')
            actor = {**entity,"origin":f"-55 15 {-source_manifest['low'][2]:.5f}"}
            room.write_text(scene+"{\n"+"".join(f'"{key}" "{value}"\n' for key,value in actor.items())+"}\n",encoding="ascii")
            ui.console("mapedit maps/rb21-compile.map")
            wait(lambda:live()["mapPath"].replace("\\","/").endswith("/maps/rb21-compile.map"))
            # The fixture uses the default floating window placement and UI scale.
            # Close its title bar so there is no window in front of Tools.
            panel = live()["widgets"]["character/save"]
            ui.click(panel["clip"][2]-15,panel["clip"][1]-13)
            wait(lambda:not state()["open"])
            click("build/compile")
            log_file = wait(lambda:next((p for p in root.rglob("build.log")
                if (p.parent/"snapshot.map").is_file()),None),timeout=75)
            shutil.copy2(log_file,out/"build.log")
            assert "Completed successfully." in log_file.read_text(errors="replace"),log_file.read_text(errors="replace")
            destination = log_file.parent
            wait(lambda:live()["bspPreview"] and not live()["buildRunning"])
            manifest = json.loads((destination/manifest_name).read_text())
            runtime_map = out/"quake3-runtime"
            for name in [entity["model"],manifest_name,*manifest["dependencies"]]:
                asset = destination/name
                assert asset.read_bytes()==(data/name).read_bytes(),name
                target = runtime_map/name
                target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(asset,target)
            (runtime_map/"maps").mkdir(exist_ok=True)
            shutil.copy2(destination/"snapshot.bsp",runtime_map/"maps/rb21-compile.bsp")
            tga = game/"textures/rb21/compile-room.tga"
            (runtime_map/"textures/rb21").mkdir(parents=True,exist_ok=True)
            shutil.copy2(tga,runtime_map/"textures/rb21/compile-room.tga")
            screenshot("compiled.png")
            check(True,"Toolbar Compile stages the entire character bundle and opens the BSP")

        def state():
            return live()["character"]

        def exercise_catalog():
            click("menu/plugins")
            click("menu/plugins/character")
            wait(lambda:state()["open"] and state()["applied"])
            names = ("Casey", "Morgan", "Riley", "Sam", "Devin", "Jordan", "Jamie", "Drew", "Taylor", "Robin")

            def preview():
                rect = live()["widgets"]["character/viewport"]
                x, y, w, h = (int(rect[key]) for key in ("x", "y", "width", "height"))
                frame = capture().convert("RGB")
                return frame.crop((x+3,y+3,x+w-3,y+h-3))

            images = set()
            for name in names:
                click("character/preset")
                wait(lambda:live()["widgets"].get("character/preset/"+name))
                assert "character/preset/Field Technician" not in live()["widgets"]
                click("character/preset/"+name)
                wait(lambda:state()["name"]==name and state()["revision"]==state()["applied"])
                ui.settle()
                view = preview()
                assert any(lo<hi for lo,hi in view.getextrema()), f"Blank preview for {name}"
                images.add(hashlib.sha256(view.tobytes()).digest())
                screenshot("preset-"+name.lower()+".png")
            check(len(images)==len(names),"Ten preset selections render distinct previews and Field Technician is absent")
            for tab_id,tab_name in enumerate(("body","face","appearance","wardrobe","rig")):
                click("character/tab-"+tab_name)
                wait(lambda:state()["tab"]==tab_id)
                screenshot("tab-"+tab_name+".png")
            check(True,"Body, Face, Appearance, Wardrobe and Rig render after native tab changes")

            click("character/tab-wardrobe")
            wait(lambda:state()["tab"]==3)
            for field,total in (("outfit",13),("hairstyle",8),("beard",4)):
                images.clear()
                for index in range(total):
                    click("character/"+field)
                    click(f"character/{field}/{index}")
                    wait(lambda:state()[field]==index and state()["revision"]==state()["applied"])
                    ui.settle()
                    images.add(hashlib.sha256(preview().tobytes()).digest())
                    screenshot(f"{field}-{index:02d}.png")
                check(len(images)==total,f"{total} native {field} choices render distinct previews")
            click("character/tab-body")
            click("character/clip")
            click("character/clip/walk_forward")
            wait(lambda:state()["clip"]=="walk_forward")
            ui.settle()
            first=preview()
            slider=live()["widgets"]["character/scrub"]
            ui.click(slider["x"]+slider["width"]*.32,slider["y"]+slider["height"]*.5)
            wait(lambda:state()["time"]>.1 and not state()["playing"])
            ui.settle()
            second=preview()
            changed=ImageChops.difference(first,second).convert("L").histogram()
            check(sum(changed[16:])>250,"Native timeline scrubbing changes the actual preview pixels")
            screenshot("walk-scrubbed.png")

        def exercise_document():
            click("menu/plugins")
            click("menu/plugins/character")
            wait(lambda:state()["open"])
            wait(lambda:state()["previewTriangles"]>15000)
            click("character/preview-detail")
            click("character/preview-detail/1")
            wait(lambda:state()["detail"]==1 and 0<state()["previewTriangles"]<=15000 and
                 state()["revision"]==state()["applied"])
            check(True,"The native preview switches to a fitted mesh within 15,000 triangles")
            click("character/tab-rig")
            click("character/gear-sockets")
            wait(lambda:state()["gearSockets"])
            screenshot("gear-sockets.png")
            click("character/tab-body")
            click("character/randomize")
            wait(lambda:state()["dirty"])
            name = state()["name"]
            click("character/new")
            click("character/unsaved-cancel")
            check(state()["dirty"] and state()["name"]==name,"Cancel preserves changes on New")
            click("character/new")
            click("character/unsaved-save")
            path = Path(wait(lambda:live()["browsePath"].endswith(".rbchar") and live()["browsePath"]))
            click("file-cancel")
            wait(lambda:state()["dirty"] and not path.exists())
            click("character/new")
            click("character/unsaved-save")
            click("file-choose")
            wait(lambda:path.is_file() and not state()["dirty"] and not state()["path"])
            check(True,"Cancelling Save As preserves edits, and saving a new document writes before resetting")
            click("character/randomize")
            click("character/open")
            click("file-entry/"+path.name)
            wait(lambda:Path(live()["browsePath"])==path)
            click("file-choose")
            wait(lambda:live()["widgets"].get("character/unsaved-discard",{}).get("visible"))
            screenshot("unsaved-changes.png")
            click("character/unsaved-discard")
            wait(lambda:state()["path"] and not state()["dirty"])
            check(Path(state()["path"])==path,"Open can discard local changes and reopen the saved source")
            click("character/clip")
            click("character/clip/walk_forward")
            wait(lambda:state()["clip"]=="walk_forward")
            click("character/frame-forward")
            wait(lambda:0<state()["time"]<.1 and not state()["playing"])
            click("character/playback")
            click("character/playback/Once")
            x,y=item("character/scrub")
            width=live()["widgets"]["character/scrub"]["width"]
            ui.click(x+.49*width,y)
            click("character/play")
            wait(lambda:state()["time"]>.25 and not state()["playing"])
            screenshot("playback-once.png")
            check(state()["playback"]==2,"A loop clip can be stepped and played once in the native timeline")
            original_map = live()["mapPath"]
            ui.console("ftebroom_ui new")
            click("character/unsaved-cancel")
            check(live()["mapPath"]==original_map and state()["dirty"],
                  "Cancelling a map change leaves both documents in place")
            if motion_fixture:
                def import_fixture(button, expected_name, file):
                    click(button)
                    target = Path(wait(lambda: (p := live()["browsePath"]) and Path(p).name == expected_name and p))
                    shutil.copy2(file, target)
                    click("file-choose")
                    wait(lambda: not live()["widgets"].get("file-choose",{}).get("visible"))
                    wait(lambda:state()["revision"] == state()["applied"])
                    return hashlib.sha256(file.read_bytes()).hexdigest()

                motion = import_fixture("character/import-motion","motion.glb",motion_fixture)
                wait(lambda:state()["clip"]=="imported_motion")
                click("character/tab-wardrobe")
                garment = import_fixture("character/import-garment","garment.rbw",
                                         data/"characters/wardrobe/male_casualsuit03.rbw")
                click("character/save")
                wait(lambda:not state()["dirty"])
                saved = json.loads(path.read_text(encoding="utf-8"))
                check(saved["animationAsset"]==motion and saved["customWardrobe"]==garment,
                      "Native import saves a fitted garment and a retargeted animation")
                for digest, extension in ((motion,".glb"),(garment,".rbw")):
                    dependency = path.parent/"rbchar-assets"/(digest+extension)
                    check(hashlib.sha256(dependency.read_bytes()).hexdigest()==digest,
                          f"Saved {extension} character dependency matches its source")
                click("character/new")
                wait(lambda:not state()["path"])
                click("character/open")
                click("file-entry/"+path.name)
                click("file-choose")
                wait(lambda:state()["path"] and state()["clip"]=="imported_motion" and
                     state()["revision"]==state()["applied"])
                screenshot("reopened-imported-character.png")
                check(not state()["error"],"Saved character with imported assets reopens in the native editor")
                click("character/export-glb")
                export = Path(wait(lambda: (p := live()["browsePath"]) and Path(p).name=="human.glb" and p))
                click("file-choose")
                wait(export.is_file,timeout=60)
                data_bytes = export.read_bytes()
                json_size, = struct.unpack_from("<I",data_bytes,12)
                gltf = json.loads(data_bytes[20:20+json_size])
                check("imported_motion" in {clip["name"] for clip in gltf["animations"]} and
                      len(gltf["meshes"][0]["extras"]["targetNames"])==5,
                      "Editor GLB includes the imported motion and editable expressions")
            if legacy_fixture:
                existing = path.parent/"legacy-field-technician.rbchar"
                shutil.copy2(legacy_fixture,existing)
                original = json.loads(existing.read_text(encoding="utf-8"))
                click("character/open")
                click("file-entry/"+existing.name)
                click("file-choose")
                if not motion_fixture:
                    click("character/unsaved-discard")
                wait(lambda:Path(state()["path"])==existing and
                     state()["revision"]==state()["applied"])
                click("character/save")
                wait(lambda:existing.with_suffix(".rbchar.bak").is_file())
                restored = json.loads(existing.read_text(encoding="utf-8"))
                keys = ("name", "faceTemplate", "outfit", "hairstyle", "morphs")
                check(all(original[key] == restored[key] for key in keys),
                      "An existing Field Technician document opens and saves without losing its preset data")
                screenshot("legacy-field-technician.png")

        try:
            if not startup_placement and not fit_fixture and not catalog_only:
                wait(lambda:live()["mapPath"].replace("\\","/").endswith("/maps/qa_clip.map"))
            wait(lambda:"RB_CHARACTER_FIXTURE_READY" in
                 "\n".join(p.read_text(errors="replace") for p in (log,data/"qconsole.log") if p.is_file()))
            ui.position()
            if catalog_only:
                exercise_catalog()
                return
            if fit_fixture:
                wait(lambda:state()["open"] and state()["applied"] and state()["revision"]==state()["applied"])
                assert not state()["error"] and live()["profile"]==profile
                if export_goldsrc or export_iqm:
                    extension=".mdl" if export_goldsrc else ".iqm"
                    click("character/export-profile")
                    default_path=Path(wait(lambda: (p := live()["browsePath"]) and
                                                   Path(p).name=="human"+extension and p))
                    model_file=default_path
                    if export_iqm:
                        model_file=out/"bundle/models"/(export_iqm+extension)
                        model_file.parent.mkdir(parents=True,exist_ok=True)
                        click("file-path")
                        wait(lambda:live()["widgets"].get("file-path",{}).get("active"))
                        ui.key("a",("ctrl",))
                        ui.key("backspace")
                        wait(lambda:live()["browsePath"]=="")
                        ui.text(model_file.as_posix())
                        wait(lambda:Path(live()["browsePath"])==model_file)
                    click("file-choose")
                    wait(lambda:model_file.is_file() and model_file.stat().st_size>244,timeout=60)
                    with model_file.open("rb") as stream:
                        signature=stream.read(16)
                    if export_goldsrc:
                        check(struct.unpack_from("<4sI",signature)==(b"IDST",10),
                              "The native Half-Life profile exports a Studio MDL")
                        shutil.copy2(model_file,out/"human.mdl")
                    else:
                        check(signature==b"INTERQUAKEMODEL\0","The native FTE profile exports IQM")
                        check((model_file.parent/"scripts"/("character-"+export_iqm+".shader")).is_file(),
                              "IQM export includes the FTE material script")
                    return
                if benchmark:
                    click("character/preview-detail")
                    click("character/preview-detail/1")
                    wait(lambda:state()["detail"]==1 and 0<state()["previewTriangles"]<=15000 and
                         state()["revision"]==state()["applied"])
                    click("character/play")
                    wait(benchmark_result.is_file,timeout=120)
                    stats=json.loads(benchmark_result.read_text())
                    frames=sorted(stats["frameMilliseconds"])
                    engine_log=(data/"qconsole.log").read_text(errors="replace")
                    renderer=re.findall(r"GL_(?:RENDERER|VERSION):[^\n]+",engine_log)
                    stats.update(renderer=renderer,averageFps=len(frames)*1000/sum(frames),
                                 medianMs=frames[len(frames)//2],p95Ms=frames[227],p99Ms=frames[237])
                    (out/"character-benchmark.json").write_text(json.dumps(stats,indent=2)+"\n")
                    screenshot("bench-playing.png")
                    check(len(frames)==240 and stats["vsync"]==stats["fpsCap"]==0,
                          "Collected 240 animated frame intervals with VSync and FPS cap disabled")
                    check([stats["width"],stats["height"]]==[1920,1080],"The live editor renders a 1920x1080 frame")
                    check(any("GL_RENDERER:" in line for line in renderer) and
                          not any(token in line.lower() for line in renderer
                                  for token in ("llvmpipe","softpipe","software")),
                          "The game window uses a hardware OpenGL renderer")
                    check(stats["averageFps"]>=60,f"Animated reduced preview averages {stats['averageFps']:.1f} FPS")
                    return
                screenshot("start.png")
                slider=wait(lambda:live()["widgets"].get("character/scrub"))
                ui.click(slider["x"]+6+(slider["width"]-12)*pose_phase,slider["y"]+slider["height"]*.5)
                wait(lambda:state()["time"]>0 and not state()["playing"])
                screenshot("pose.png")
                click("character/upper-body")
                wait(lambda:state()["framing"]==3)
                screenshot("upper-body.png")
                if os.name!="nt":
                    viewport=wait(lambda:live()["widgets"].get("character/viewport"))
                    x=viewport["x"]+viewport["width"]*.8
                    y=viewport["y"]+viewport["height"]*.5
                    for angle,name in ((90,"side"),(90,"rear")):
                        ui.orbit(x,y,x-angle/.4)
                        screenshot(name+".png")
                (out/"pose.json").write_text(json.dumps(state(),indent=2)+"\n")
                return
            if document_only:
                exercise_document()
                return
            assert live()["profile"] == profile
            if startup_placement:
                check(not live()["mapPath"],"FTE-specific startup scene is still untitled")
            view = live()["views"][0]
            x,y = view["x"]+view["width"]*.46,view["y"]+view["height"]*(.65 if startup_placement else .52)
            ui.click(x,y,button=3,hold=.55)
            hover("context/create")
            hover("context/model")
            click("context/new-character")
            bind = wait(lambda:state()["placement"]["active"] and state()["placement"])
            check(bind["id"]==live()["context"]["id"],"Viewport Create records the original surface hit")
            ui.console("ftebroom_camera_restore 800 -900 600 20 110 0 75")
            click("character/save")
            path = Path(wait(lambda:live()["browsePath"].endswith(".rbchar") and live()["browsePath"]))
            click("file-choose")
            wait(lambda:path.is_file() and state()["placement"]["created"] and not state()["placement"]["awaiting"])
            initial, = capture_map("initial")
            assert initial["classname"] == ("func_model" if startup_placement else "misc_model")
            xyz = [float(v) for v in initial["origin"].strip("'").split()]
            delta = [a-b for a,b in zip(xyz,bind["point"])]
            normal = bind["normal"]
            offset = sum(a*b for a,b in zip(delta,normal))
            check(sum((a-b*offset)**2 for a,b in zip(delta,normal))<256,
                  "Save places a single " +
                  ("FTE-specific func_model" if startup_placement else "Quake III misc_model") +
                  " at the captured surface")
            model = data / initial["model"]
            manifest = json.loads(Path(str(model)+".reyond.json").read_text())
            for dependency in manifest["dependencies"]:
                assert (data/dependency).is_file(),dependency
            assert (data/initial["_reyondbroom_source"]).suffix == ".rbchar"
            if startup_placement:
                assert model.suffix==".iqm" and any(name.endswith(".shader") for name in manifest["dependencies"])
                screenshot("startup-character.png")
                check(not live()["mapPath"],"Character placement on the FTE-specific startup scene does not save the map")
                return
            check(Path(str(model)+".framegroups").is_file(),"The model bundle includes source, textures and frame groups")

            ui.console('ftebroom_entity_property set angles "0 25 0"')
            click("character/randomize")
            click("character/save")
            wait(lambda:state()["status"]=="Saved the linked character model." and not state()["placement"]["awaiting"])
            updated, = capture_map("updated")
            check(updated["model"]!=initial["model"] and updated["origin"]==initial["origin"]
                  and updated.get("angles")=="0 25 0","Repeat Save updates the same entity and preserves its transform")
            ui.console("ftebroom_undo")
            undone, = capture_map("undo")
            check(undone["model"]==initial["model"] and model.is_file(),"Map Undo restores the previous immutable model")
            ui.console("ftebroom_redo")
            redone, = capture_map("redo")
            check(redone["model"]==updated["model"],"Map Redo restores the new model")
            ui.console("ftebroom_delete")
            check(not capture_map("deleted"),"The placed character entity can be deleted")
            click("character/randomize")
            click("character/save")
            wait(lambda:not state()["placement"]["active"])
            check(not capture_map("deleted-save"),"Save cannot recreate a deleted character entity")
            ui.console("mapedit maps/rb21-redo.map")
            wait(lambda:live()["mapPath"].replace("\\","/").endswith("/maps/rb21-redo.map"))
            reopened, = capture_map("reopened")
            check(reopened["model"]==redone["model"] and not state()["placement"]["active"],
                  "Reopening the map retains the asset and clears the old document binding")
            click("character/dock-panes")
            ui.settle()
            screenshot("docked.png")
            click("character/dock-panes")
            ui.settle()
            check(editor.poll() is None,"Character preview survives switching between docked and integrated panes")
            if compile_assets:
                compile_character(reopened)
        except Exception:
            screenshot("failure.png")
            if telemetry.exists():
                shutil.copy2(telemetry,out/"failure-state.json")
            raise
        finally:
            console = data/"qconsole.log"
            if console.is_file():
                shutil.copy2(console,out/"engine-console.log")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime",type=Path)
    parser.add_argument("output",type=Path)
    parser.add_argument("--compile-assets",action="store_true",help="Compile a sealed room with the placed model")
    parser.add_argument("--document-only",action="store_true",help="Exercise document guards and animation controls")
    parser.add_argument("--catalog-only",action="store_true",help="Inspect native previews, tabs and one rendered animation")
    parser.add_argument("--motion-fixture",type=Path,help="Canonical animation GLB for the document import test")
    parser.add_argument("--legacy-fixture",type=Path,help="Previously saved Field Technician document")
    parser.add_argument("--startup-placement",action="store_true",help="Place an IQM character on the untitled FTE-specific startup map")
    parser.add_argument("--fit-fixture",type=Path,help="Capture a built-in wardrobe .rbchar in the actual FTE viewport")
    parser.add_argument("--pose-phase",type=float,default=.25,help="Fraction of the fixture clip at which to capture the pose")
    parser.add_argument("--benchmark",action="store_true",help="Measure the animated 1080p native viewport on a GPU")
    parser.add_argument("--export-goldsrc",action="store_true",help="Export the fixture via the native Half-Life profile")
    parser.add_argument("--export-iqm",metavar="NAME",help="Export the fixture and FTE material dependencies")
    args = parser.parse_args()
    if not 0 < args.pose_phase < 1:
        parser.error("--pose-phase must be between 0 and 1")
    if args.benchmark and not args.fit_fixture:
        parser.error("--benchmark requires --fit-fixture with a looping animation")
    if args.export_goldsrc and (not args.fit_fixture or args.benchmark):
        parser.error("--export-goldsrc requires --fit-fixture and runs separately from --benchmark")
    if args.export_iqm and (not re.fullmatch(r"[A-Za-z0-9_-]{1,32}",args.export_iqm)
                            or not args.fit_fixture or args.benchmark or args.export_goldsrc):
        parser.error("--export-iqm requires an ASCII model name and an exclusive --fit-fixture export")
    args.output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="reyond-character-ui-") as scratch:
        root = Path(scratch)/"editor"
        shutil.copytree(args.runtime.resolve(),root)
        exercise(root,args.output.resolve(),args.compile_assets,args.document_only,args.startup_placement,
                 args.motion_fixture.resolve() if args.motion_fixture else None,
                 args.legacy_fixture.resolve() if args.legacy_fixture else None,
                 args.fit_fixture.resolve() if args.fit_fixture else None,args.pose_phase,args.benchmark,
                 args.export_goldsrc,args.export_iqm,args.catalog_only)


if __name__ == "__main__":
    main()
