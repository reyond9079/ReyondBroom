#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the draft release gate with disposable local files and no GitHub calls."""
from datetime import datetime, timedelta, timezone
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
COMMIT = "1" * 40
OWNER = "Owner"
PLATFORM = "Linux-x64"


def main():
    with tempfile.TemporaryDirectory(prefix="rb21-release-lock-") as scratch:
        evidence, release = (Path(scratch) / part for part in ("evidence", "release"))
        evidence.mkdir()
        release.mkdir()
        proof = evidence / "fixture.log"
        proof.write_bytes(b"Synthetic dry-run evidence only\n")
        packages = {}
        for name in (f"ReyondBroom-qa-{PLATFORM}.tar.gz", f"ReyondBroom-qa-{PLATFORM}-Sources.zip"):
            (release / name).write_bytes(name.encode())
            packages[name] = sha256((release / name).read_bytes()).hexdigest()
            companion = name + ".sha256"
            (release / companion).write_bytes(f"{packages[name]}  {name}\n".encode())
            packages[companion] = sha256((release / companion).read_bytes()).hexdigest()
        ids = (ROOT / "qa/rb-21/required-gates.txt").read_text().split()
        approved = {"user": {"login": OWNER}, "environments": [{"name":"rb21-release"}], "state":"approved"}
        manifest = evidence / "test-results.json"
        approval_file = evidence / "approvals.json"

        def fixture():
            return {"commit":COMMIT,"clean":True,"finishedAt":datetime.now(timezone.utc).isoformat(),
                    "gates":[{"id":gate,"status":"PASS","exitCode":0,
                              "evidence":{proof.name:sha256(proof.read_bytes()).hexdigest()}} for gate in ids],
                    "packages":{key:packages for key in (PLATFORM,"Windows-x64","Linux-SDL2-x64")}}

        def attempt(label, expected, change=None, approvals=None, remove_manifest=False):
            report = fixture()
            if change:
                change(report)
            manifest.write_text(json.dumps(report),encoding="utf-8")
            if remove_manifest:
                manifest.unlink()
            approval_file.write_text(json.dumps([approved] if approvals is None else approvals),encoding="utf-8")
            result = subprocess.run([sys.executable,ROOT/"scripts/check-character-release.py","--evidence",evidence,
                                     "--release",release,"--commit",COMMIT,"--platform",PLATFORM,
                                     "--approvals",approval_file,"--owner",OWNER],
                                    text=True,capture_output=True,timeout=10)
            assert result.returncode == expected, (label,result.stdout,result.stderr)
            print("PASS",label)

        attempt("complete synthetic report is accepted locally",0)
        attempt("unapproved workflow is rejected",1,approvals=[])
        attempt("approval by another reviewer is rejected",1,approvals=[{**approved,"user":{"login":"Reviewer"}}])
        attempt("missing report is rejected",1,remove_manifest=True)
        attempt("wrong source revision is rejected",1,lambda r:r.update(commit="2"*40))
        attempt("dirty source is rejected",1,lambda r:r.update(clean=False))
        attempt("stale report is rejected",1,lambda r:r.update(
            finishedAt=(datetime.now(timezone.utc)-timedelta(days=4)).isoformat()))
        for status in ("FAIL","BLOCKED","SKIP"):
            attempt(f"{status} gate is rejected",1,lambda r:r["gates"][0].update(status=status))
        attempt("missing gate is rejected",1,lambda r:r["gates"].pop())
        attempt("missing evidence is rejected",1,lambda r:r["gates"][0].update(evidence={}))
        attempt("evidence checksum mismatch is rejected",1,lambda r:r["gates"][0].update(
            evidence={proof.name:"0"*64}))
        attempt("package checksum mismatch is rejected",1,lambda r:(release/next(iter(packages))).write_bytes(b"wrong"))


if __name__ == "__main__":
    main()
