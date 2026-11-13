"""Launch the private Mapping Jams API and the isolated Windows editor."""

import argparse
import json
import os
from pathlib import Path
import secrets
import subprocess
import sys
import time
import urllib.request


def local_jam(port):
    try:
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        with opener.open(f"http://127.0.0.1:{port}/v1/jams/current", timeout=1) as response:
            return json.load(response).get("jam")
    except (OSError, ValueError):
        return None


def main():
    repo = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, default=repo / "out/jam-editor-clean")
    parser.add_argument("--state", type=Path, default=repo / "out/jam-v2")
    parser.add_argument("--tools", type=Path, default=Path("C:/Games/FTEBroom/tools/ericw"))
    parser.add_argument("--librequake", type=Path,
                        default=Path("C:/Games/FTEBroom/downloads/librequake-v0.09/mod_lite/lq1"))
    parser.add_argument("--port", type=int, default=8770)
    parser.add_argument("--server-only", action="store_true")
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error("The port must be between 1024 and 65535.")
    bundle = args.bundle.resolve()
    engine = bundle / "bin/fteqw64.exe"
    module = bundle / "fteplug_ftebroom_x64.dll"
    if not args.server_only and not (engine.is_file() and module.is_file()):
        parser.error(f"Local editor or module missing in {bundle}")
    env = dict(os.environ, REYONDBROOM_JAM_PORT=str(args.port),
               REYONDBROOM_JAM_TOOLS=str(args.tools.resolve()), PYTHONPATH=str(repo / "server"))
    env.pop("REYONDBROOM_JAM_QA_STATE", None)
    jam = local_jam(args.port)
    if jam is None:
        state = args.state.resolve()
        state.mkdir(parents=True, exist_ok=True)
        key = state / "moderator.key"
        try:
            with key.open("x", encoding="ascii") as output:
                output.write(secrets.token_urlsafe(32) + "\n")
        except FileExistsError:
            pass
        with (state / "api.log").open("a", encoding="utf-8") as log:
            process = subprocess.Popen(
                [sys.executable, "-m", "mapping_jams", "--data", str(state),
                 "--tools", str(args.tools.resolve()), "--librequake", str(args.librequake.resolve()),
                 "--plant-preset", str(repo / "runtime/plants/builtin/catalog.json"), "--port", str(args.port),
                 "--moderator-key-file", str(key)],
                cwd=repo, env=env, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
                start_new_session=os.name != "nt")
        for _ in range(40):
            jam = local_jam(args.port)
            if jam is not None or process.poll() is not None:
                break
            time.sleep(.25)
        if jam is None:
            parser.error(f"The local API did not start. See {state / 'api.log'}")
    if jam.get("jamId") != "local-first-jam" or jam.get("localApiRevision") != 5:
        parser.error(f"Port {args.port} is serving another API or an older Mapping Jams server. "
                     "Restart that local server or choose another port.")
    if not (args.state.resolve() / "moderator.key").is_file():
        parser.error("The local API is running with another data directory or without moderation enabled.")
    print(f"Mapping Jams is ready at http://127.0.0.1:{args.port}", flush=True)
    if args.server_only:
        return
    editor = subprocess.Popen([str(engine), "-window", "-maximize", "-nohome", "-noupdates",
                               "-condebug", "-basedir", str(bundle), "+exec", "ftebroom.cfg"],
                              cwd=bundle, env=env)
    try:
        code = editor.wait(timeout=3)
    except subprocess.TimeoutExpired:
        return
    parser.error(f"The editor exited during startup ({code}). See {bundle / 'ftebroom/qconsole.log'}")


if __name__ == "__main__":
    main()
