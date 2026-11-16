#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Authorize a draft upload only when the exact RB-21 release evidence passes."""
import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import json
from pathlib import Path
import re
import sys


REQUIRED = frozenset((Path(__file__).resolve().parents[1] / "qa/rb-21/required-gates.txt")
                     .read_text(encoding="utf-8").split())
PLATFORMS = ("Windows-x64", "Linux-x64", "Linux-SDL2-x64")
MAX_AGE = timedelta(days=3)


def checked_digest(root, name, expected):
    if not isinstance(name, str) or not name or "\\" in name or ":" in name:
        raise ValueError("Invalid evidence filename")
    relative = Path(name)
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"Evidence path escapes its directory: {name}")
    path = root / relative
    if not path.is_file() or path.resolve() != path:
        raise ValueError(f"Missing evidence file or path points through a link: {name}")
    if not isinstance(expected, str) or not re.fullmatch(r"[0-9a-f]{64}", expected):
        raise ValueError(f"Invalid checksum for {name}")
    with path.open("rb") as stream:
        if hashlib.file_digest(stream, "sha256").hexdigest() != expected:
            raise ValueError(f"Checksum mismatch for {name}")


def validate_approval(records, owner):
    reviews = [review for review in records
               if review["user"]["login"].casefold() == owner.casefold() and
               any(env["name"] == "rb21-release" for env in review["environments"])]
    if not reviews or any(review["state"] != "approved" for review in reviews):
        raise ValueError("RB-21 draft upload requires an explicit approval from the repository owner")


def validate(evidence, release, commit, platform, now):
    evidence, release = evidence.resolve(), release.resolve()
    path = evidence / "test-results.json"
    if not path.is_file() or path.resolve() != path or path.stat().st_size > 2 * 1024 * 1024:
        raise ValueError("Missing or invalid RB-21 acceptance report")
    report = json.loads(path.read_text(encoding="utf-8"))
    if not re.fullmatch(r"[0-9a-f]{40}", commit) or report["commit"] != commit or report["clean"] is not True:
        raise ValueError("Acceptance must cover this exact clean source revision")
    finished = datetime.fromisoformat(report["finishedAt"])
    if finished.tzinfo is None or not timedelta() <= now - finished <= MAX_AGE:
        raise ValueError("Acceptance report is stale or has an invalid completion time")
    if len(REQUIRED) < 80:
        raise ValueError("Required gate inventory is incomplete")
    seen = set()
    verified = {}
    for gate in report["gates"]:
        gate_id = gate["id"]
        exit_code = gate["exitCode"]
        if gate_id in seen or gate["status"] != "PASS" or type(exit_code) is not int or exit_code != 0:
            raise ValueError(f"Acceptance gate failed, was skipped or was repeated: {gate_id}")
        seen.add(gate_id)
        files = gate["evidence"]
        if not isinstance(files, dict) or not files:
            raise ValueError(f"No evidence for {gate_id}")
        for name, digest in files.items():
            if verified.get(name) != digest:
                checked_digest(evidence, name, digest)
                verified[name] = digest
    if missing := REQUIRED - seen:
        raise ValueError(f"Missing acceptance gates: {', '.join(sorted(missing))}")
    if set(report["packages"]) != set(PLATFORMS):
        raise ValueError("Acceptance must cover Windows, Linux and Linux SDL2 packages")
    expected = report["packages"][platform]
    packaged = sorted(path.name for path in release.iterdir())
    if sorted(expected) != packaged:
        raise ValueError(f"Package list differs from validated {platform} release inputs")
    archives = [name for name in expected if name.endswith((".zip", ".tar.gz"))]
    if len(archives) != 2 or sum(name.endswith("-Sources.zip") for name in archives) != 1 or \
            set(expected) != set(archives) | {name + ".sha256" for name in archives}:
        raise ValueError("The runtime, corresponding source and both checksums are required")
    if any(platform not in name for name in archives):
        raise ValueError(f"Archive name does not match the {platform} package target")
    for name, digest in expected.items():
        checked_digest(release, name, digest)
    for name in archives:
        fields = (release / (name + ".sha256")).read_text(encoding="ascii").strip().split(maxsplit=1)
        if len(fields) != 2 or fields[0] != expected[name] or fields[1].lstrip("*") != name:
            raise ValueError(f"Detached checksum does not identify {name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--release", required=True, type=Path)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--platform", required=True, choices=PLATFORMS)
    parser.add_argument("--approvals", required=True, type=Path)
    parser.add_argument("--owner", required=True)
    args = parser.parse_args()
    try:
        validate_approval(json.loads(args.approvals.read_text(encoding="utf-8")), args.owner)
        validate(args.evidence, args.release, args.commit, args.platform, datetime.now(timezone.utc))
    except (KeyError, OSError, TypeError, ValueError) as error:
        print(f"RB-21 release denied. {error}", file=sys.stderr)
        return 1
    print(f"RB-21 evidence accepted for {args.commit} on {args.platform}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
