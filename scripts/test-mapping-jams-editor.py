"""Exercise Mapping Jams in an isolated native editor installation."""

import argparse
import json
import os
import shutil
import subprocess
import time
from pathlib import Path
from PIL import Image


def run(bundle, state, port, resource_retry=False):
    bundle = bundle.resolve()
    game_dir = bundle / "ftebroom"
    config = game_dir / "jam-test.cfg"
    config.write_text("set ftebroom_skip_setup 1\nexec ftebroom.cfg\nin 145 quit force\n", encoding="ascii")
    avatar = game_dir / "mapping-jams-qa-avatar.png"
    Image.new("RGB", (320, 180), (30, 140, 185)).save(avatar)
    env = {**os.environ, "REYONDBROOM_JAM_PORT": str(port), "REYONDBROOM_JAM_QA_STATE": str(state.resolve()),
           "REYONDBROOM_JAM_TOOLS": os.environ.get("REYONDBROOM_JAM_TOOLS", "C:/Games/FTEBroom/tools/ericw"),
           "REYONDBROOM_JAM_QA_AVATAR": str(avatar)}
    engine = bundle / ("bin/fteqw64.exe" if os.name == "nt" else "fteqw-gl64")
    report = game_dir / "mapping-jams-qa.json"
    started = time.time()
    launch = [str(engine), "-ftebroom-child-processes", "-nohome", "-noupdates", "-basedir", str(bundle),
              "-window", "-width", "1280", "-height", "800", "-condebug", "+exec", config.name]
    if resource_retry:
        stall_library = state.resolve() / "qa-resource-stall.so"
        stall_marker = state.resolve() / "qa-resource-stall-marker"
        assert not stall_marker.exists(), "Use a clean disposable test server state."
        fault = Path(__file__).resolve().parents[1] / "tests/mapping-jam-resource-stall.c"
        subprocess.run(["cc","-shared","-fPIC",str(fault),"-ldl","-o",str(stall_library)],check=True)
        env.update({"LD_PRELOAD":str(stall_library), "RB_QA_STALL_MARKER":str(stall_marker),
                    "REYONDBROOM_JAM_QA_RESOURCE_RETRY":"1"})
    with subprocess.Popen(launch, cwd=bundle, env=env) as editor:
        try:
            observations=state.resolve()/"qa-game-observations"
            play=bundle/"mapping-jams-local"/str(port)/"play"
            while editor.poll() is None:
                if time.time()-started>155:
                    raise subprocess.TimeoutExpired(launch,155)
                # Finished game copies are disposable. Capture their test evidence
                # while each game is running, before the editor retires its files.
                for directory in play.glob("*"):
                    for source in directory.rglob("*"):
                        if source.is_file() and (source.name=="qconsole.log" or source.suffix==".bsp"):
                            destination=observations/source.relative_to(play)
                            try:
                                destination.parent.mkdir(parents=True,exist_ok=True)
                                shutil.copy2(source,destination)
                            except OSError:
                                pass
                time.sleep(.15)
            assert editor.returncode==0,editor.returncode
        except subprocess.TimeoutExpired:
            editor.terminate()
            editor.wait(timeout=10)
            raise
    assert report.is_file() and report.stat().st_mtime >= started - 1, "No fresh native test report."
    result = json.loads(report.read_text(encoding="utf-8"))
    assert result["passed"], result
    if resource_retry:
        assert result["stage"] == 90 and stall_marker.is_file(), result
        print(result["message"])
        env.pop("LD_PRELOAD")
        env.pop("REYONDBROOM_JAM_QA_RESOURCE_RETRY")
        report.unlink()
        subprocess.run(launch, cwd=bundle, env={**env,"REYONDBROOM_JAM_QA_RESUME":"1"}, timeout=40, check=True)
        resumed = json.loads(report.read_text(encoding="utf-8"))
        assert resumed["passed"] and resumed["stage"] == 100, resumed
        print(resumed["message"])
        return
    draft = bundle / "mapping-jams-local" / str(port) / "drafts"
    built = [path for path in (game_dir / "cache/jam-builds").glob("*/snapshot.bsp")
             if path.stat().st_mtime >= started-1]
    assert built, "The editor did not compile the named Jam map in the build cache."
    assert any((path.parent / "saved-portals.prt").is_file() and
               (path.parent / "snapshot.lit").is_file() for path in built), (
        "The Jam build did not save the portal backup and lighting output.")
    assert any(draft.glob("*/*/Editor's compiled Jam room.map")), (
        "The named source map is missing from the compiled plot's workspace.")
    logs = [p for p in observations.rglob("qconsole.log") if p.stat().st_mtime >= started-1]
    assert not list(play.glob("*")),"Finished game resource copies were not cleaned up."
    direct_runs = hub_runs = returns_to_draft = 0
    errors = ("couldn't spawn server", "could not load maps/jam_map", "couldn't load maps/jam_map", "couldn't load progs.dat")
    for log in logs:
        game_log = log.read_text(encoding="utf-8", errors="replace")
        assert not any(text in game_log.lower() for text in errors), "The game failed to load the map."
        if "SpawnServer: jam_hub" in game_log:
            hub_runs += 1
            if "execing jam-return-menu.cfg" in game_log:
                assert "JAM_RETURN_TO_EDITING" in game_log and "JAM_RETURN_FALLBACK" not in game_log, (
                    "The Hub menu did not close the game.")
                returns_to_draft += 1
            assert any(log.parent.joinpath("maps").glob("jam_??.bsp")), "The Hub plot maps were not staged."
        elif "SpawnServer: jam_map" in game_log:
            direct_runs += 1
        else:
            # FTE can start additional console logs while searching fallback game directories.
            continue
        assert ("client Player connected" in game_log or "Player joined the server" in game_log), (
            "The game never admitted the local player.")
    assert direct_runs >= 3 and hub_runs >= 2, "Expected fresh engine logs for the draft, owner, visitor and Hub."
    assert returns_to_draft == 1, "The in-game Return to editing action did not run."
    print(result["message"])
    for log in logs:
        print(log)
    report.unlink()
    subprocess.run(launch, cwd=bundle, env={**env,"REYONDBROOM_JAM_QA_RESUME":"1"}, timeout=40, check=True)
    resumed = json.loads(report.read_text(encoding="utf-8"))
    assert resumed["passed"] and resumed["stage"]==100, resumed
    print(resumed["message"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", required=True, type=Path)
    parser.add_argument("--state", required=True, type=Path)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--resource-retry",action="store_true",help="Simulate a stalled local receipt write on Linux")
    args = parser.parse_args()
    if args.resource_retry and os.name != "posix":
        parser.error("--resource-retry needs the Linux editor")
    run(args.bundle, args.state, args.port, args.resource_retry)
