"""Exercise Mapping Jams in an isolated native editor installation."""

import argparse
import json
import os
import subprocess
import time
from pathlib import Path
from PIL import Image


def run(bundle, state, port):
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
    launch = [str(engine), "-nohome", "-noupdates", "-basedir", str(bundle),
              "-window", "-width", "1280", "-height", "800", "-condebug", "+exec", config.name]
    with subprocess.Popen(launch, cwd=bundle, env=env) as editor:
        try:
            editor.wait(timeout=155)
        except subprocess.TimeoutExpired:
            editor.terminate()
            editor.wait(timeout=10)
            raise
    assert report.is_file() and report.stat().st_mtime >= started - 1, "No fresh native test report."
    result = json.loads(report.read_text(encoding="utf-8"))
    assert result["passed"], result
    draft = bundle / "mapping-jams-local" / str(port) / "drafts"
    built = [path for path in (game_dir / "cache/jam-builds").glob("*/snapshot.bsp")
             if path.stat().st_mtime >= started-1]
    assert built, "The editor did not compile the named Jam map in the build cache."
    assert any((path.parent / "saved-portals.prt").is_file() and
               (path.parent / "snapshot.lit").is_file() for path in built), (
        "The Jam build did not save the portal backup and lighting output.")
    assert any(draft.glob("*/*/Editor's compiled Jam room.map")), (
        "The named source map is missing from the compiled plot's workspace.")
    logs = [p for p in bundle.rglob("qconsole.log") if p.stat().st_mtime >= started-1 and "play" in p.parts]
    direct_runs = hub_runs = returns_to_draft = 0
    errors = ("couldn't spawn server", "could not load maps/jam_map", "couldn't load maps/jam_map", "couldn't load progs.dat")
    for log in logs:
        game_log = log.read_text(encoding="utf-8", errors="replace")
        if "SpawnServer: jam_hub" in game_log:
            hub_runs += 1
            if "execing jam-return-menu.cfg" in game_log:
                assert "JAM_RETURN_TO_EDITING" in game_log and "JAM_RETURN_FALLBACK" not in game_log, (
                    "The Hub menu did not close the game.")
                returns_to_draft += 1
            assert any(log.parent.joinpath("maps").glob("jam_??.bsp")), "The Hub plot maps were not staged."
        elif "SpawnServer: jam_map" in game_log:
            direct_runs += 1
        assert "client Player connected" in game_log, "The game never admitted the local player."
        assert not any(text in game_log.lower() for text in errors), "The game failed to load the map."
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
    args = parser.parse_args()
    run(args.bundle, args.state, args.port)
