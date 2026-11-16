#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run available RB-21 acceptance checks and record unmet release gates."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--runtime", type=Path, help="Prepared runtime for the current host")
    parser.add_argument("--legacy-fixture", type=Path,
                        help="A Field Technician .rbchar saved before the preset was retired")
    parser.add_argument("--benchmark-fixture", type=Path,
                        help="Looping .rbchar to benchmark on a real GPU and active desktop")
    parser.add_argument("--fte-game-qcc", type=Path,
                        help="FTEQCC for the standalone IQM test using the Linux runtime under Xvfb")
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--sanitize", action="store_true", help="Also run core ASan/UBSan checks")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    clean = not subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True)
    results = []

    def report():
        (output / "test-results.json").write_text(
            json.dumps({"commit": revision, "clean": clean, "platform": platform.platform(),
                        "gates": results}, indent=2) + "\n", encoding="utf-8")
        lines = ["# RB-21 acceptance", "", f"Commit `{revision}`",
                 f"Working tree: {'clean' if clean else 'local changes'}", ""]
        lines += [f'- **{gate["status"]}** {gate["id"]} · {gate["detail"]}' for gate in results]
        (output / "test-summary.md").write_text("\n".join(lines)+"\n", encoding="utf-8")

    def block(test_id, reason):
        results.append({"id": test_id, "status": "BLOCKED", "detail": reason})
        report()

    def run_command(test_id, command, detail):
        started = datetime.now(timezone.utc).isoformat()
        command = list(map(str,command))
        log = output/(test_id+".log")
        with log.open("wb") as stream:
            try:
                code = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                      timeout=1200).returncode
            except subprocess.TimeoutExpired:
                code = 124
        results.append({"id": test_id, "status": "PASS" if code==0 else "FAIL",
                "detail": detail, "command": command, "exitCode": code,
                        "start": started, "end": datetime.now(timezone.utc).isoformat(),
                        "log": log.name, "sha256": hashlib.sha256(log.read_bytes()).hexdigest()})
        report()
        return code

    def run(test_id, script, *inputs):
        return run_command(test_id, [sys.executable,ROOT/"scripts"/script,*inputs],script)

    run("CORE-ASSETS-EXPORT-DECODERS", "test-character-core.py", "--cxx", args.cxx,
        "--stress", "--output", output/"exports")
    fit_exe = output/("fit-matrix.exe" if os.name=="nt" else "fit-matrix")
    fit_build = run_command("FIT-MATRIX-BUILD",
        [args.cxx,"-std=c++17","-O2",ROOT/"tests/test-character-fit-matrix.cpp","-o",fit_exe],
        "Build the native garment/body intersection test")
    if fit_build==0:
        run_command("FIT-MATRIX", [fit_exe, ROOT/"runtime/characters", output/"fit-matrix.csv",
                                   output/"fit-contacts.csv"],
                    "All bundled outfits across five body variants and eight poses, with five-view depth diagnostics")
    else:
        block("FIT-MATRIX","A working native fit test is required")
    if args.sanitize:
        run("CORE-ASAN-UBSAN", "test-character-core.py", "--cxx", args.cxx,
            "--sanitize", "--output", output/"sanitized")
    else:
        block("CORE-ASAN-UBSAN", "Run with --sanitize on a compiler with ASan and UBSan")
    if args.runtime and args.runtime.is_dir():
        runtime = args.runtime.resolve()
        legacy_args = ["--legacy-fixture", args.legacy_fixture.resolve()] if args.legacy_fixture else []
        if not legacy_args:
            block("NATIVE-LEGACY-DOCUMENT", "Supply the previously saved Field Technician .rbchar")
        run("NATIVE-DOCUMENT-TIMELINE", "test-character-placement.py", runtime,
            output/"document", "--document-only", "--motion-fixture", output/"exports/character.glb", *legacy_args)
        run("NATIVE-CATALOG", "test-character-placement.py", runtime,
            output/"catalog", "--catalog-only")
        run("NATIVE-MAP-Q3-BUNDLE", "test-character-placement.py", runtime,
            output/"placement", "--compile-assets")
        run("NATIVE-MAP-FTE-STARTUP", "test-character-placement.py", runtime,
            output/"startup-placement", "--startup-placement")
        if args.fte_game_qcc and sys.platform.startswith("linux"):
            actors = ("Casey","Drew","Jamie")
            game_output = output/"standalone-fte"
            exports = [
                run("NATIVE-IQM-"+name.upper(), "test-character-placement.py", runtime, game_output/name,
                    "--fit-fixture", ROOT/"runtime"/"characters"/"examples"/(name+".rbchar"),
                    "--export-iqm", name.lower())
                for name in actors
            ]
            if all(code==0 for code in exports):
                run("STANDALONE-FTE-IQM", "test-character-fte-game.py", game_output/"game",
                    "--engine", runtime/"fteqw-gl64", "--compiler", args.fte_game_qcc.resolve(),
                    "--map-tools", runtime/"tools"/"ericw", "--bundles",
                    *(game_output/name/"bundle" for name in actors))
            else:
                block("STANDALONE-FTE-IQM", "The three native exports must succeed")
        else:
            block("STANDALONE-FTE-IQM", "Supply --fte-game-qcc on Linux with Xvfb")
        if args.benchmark_fixture:
            run("VIEWPORT-HARDWARE-FRAMES", "test-character-placement.py", runtime,
                output/"hardware-frames", "--fit-fixture", args.benchmark_fixture.resolve(), "--benchmark")
        else:
            block("VIEWPORT-HARDWARE-FRAMES", "Supply a looping --benchmark-fixture on a hardware desktop")
    else:
        block("NATIVE-DOCUMENT-TIMELINE", "Supply a prepared runtime and a usable desktop")
        block("NATIVE-CATALOG", "Supply a prepared runtime and a usable desktop")
        block("NATIVE-MAP-Q3-BUNDLE", "Supply a prepared runtime and a usable desktop")
        block("NATIVE-MAP-FTE-STARTUP", "Supply a prepared runtime and a usable desktop")
        block("STANDALONE-FTE-IQM", "Supply a Linux runtime, FTEQCC and Xvfb")
        block("VIEWPORT-HARDWARE-FRAMES", "Supply a prepared runtime, a hardware desktop and a looping fixture")
    run_command("RELEASE-LOCK-DRY-RUN", [sys.executable,ROOT/"tests/test-character-release-lock.py"],
                "Reject unapproved, incomplete, stale or hash-mismatched inputs locally")
    block("REMAINING-ACCEPTANCE-MATRIX",
          "Standalone engine matrix, full renderer/UI/regression tests, separate CPU/GPU timings, "
          "native lifecycle sanitizers and clean package validation still require their own evidence")
    return 1 if any(gate["status"]!="PASS" for gate in results) else 0


if __name__ == "__main__":
    sys.exit(main())
