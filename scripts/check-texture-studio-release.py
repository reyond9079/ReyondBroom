#!/usr/bin/env python3
"""Validate evidence before giving a Texture Studio publisher its credentials."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys


CHECKS = frozenset(
    f"{prefix}{index:02}"
    for prefix, count in (("U", 16), ("I", 12), ("E", 5), ("G", 6), ("S", 7), ("P", 9), ("TS", 15))
    for index in range(1, count + 1)
) | {"TS17"}
SHA = re.compile("[0-9a-f]{40}")
DIGEST = re.compile("[0-9a-f]{64}")
PLATFORMS = ("Linux-SDL2-x64", "Linux-x64", "Windows-x64")


def fail(reason):
    raise ValueError(reason)


def candidate_digest(root, version):
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?", version):
        fail("Invalid candidate version")
    root = root.resolve(strict=True)
    manifest = []
    for platform in PLATFORMS:
        extension = "zip" if platform.startswith("Windows-") else "tar.gz"
        archive_names = (f"ReyondBroom-{version}-{platform}.{extension}",
                         f"ReyondBroom-{version}-{platform}-Sources.zip")
        directory = root / f"release-packages-{platform}"
        names = {entry.name for entry in directory.iterdir()}
        if names != set(archive_names) | {name + ".sha256" for name in archive_names}:
            fail(f"Unexpected package inventory for {platform}")
        for name in archive_names:
            archive, sidecar = directory / name, directory / (name + ".sha256")
            if any(path.resolve() != path or not path.is_file() for path in (archive, sidecar)):
                fail(f"Missing or linked package for {platform}")
            with archive.open("rb") as stream:
                digest = hashlib.file_digest(stream, "sha256").hexdigest()
            if sidecar.stat().st_size > 1024 or sidecar.read_text(encoding="ascii").split() != [digest, name]:
                fail(f"Package checksum mismatch for {platform}")
            manifest.append(f"{digest}  {name}\n")
    return hashlib.sha256((version + "\n" + "".join(manifest)).encode("ascii")).hexdigest()


def verify_file(root, name, checksum):
    if not isinstance(name, str) or not name or "\\" in name or ":" in name:
        fail("Evidence name must be a safe relative path")
    relative = PurePosixPath(name)
    if relative.is_absolute() or ".." in relative.parts or relative.as_posix() != name:
        fail("Evidence path escapes its directory")
    path = root.joinpath(*relative.parts)
    if path.resolve() != path or not path.is_file():
        fail(f"Missing or linked evidence file {name}")
    if not isinstance(checksum, str) or not DIGEST.fullmatch(checksum):
        fail(f"Invalid evidence checksum for {name}")
    with path.open("rb") as data:
        if hashlib.file_digest(data, "sha256").hexdigest() != checksum:
            fail(f"Evidence checksum mismatch for {name}")


def validate(root, source, build, publication=False, authorized=False):
    root = root.resolve(strict=True)
    if not SHA.fullmatch(source) or not DIGEST.fullmatch(build):
        fail("Expected the exact source commit and build checksum")
    path = root / "gate.json"
    if path.resolve() != path or not path.is_file() or path.stat().st_size > 2 * 1024 * 1024:
        fail("Missing or oversized gate report")
    report = json.loads(path.read_text(encoding="utf-8"))
    if report.get("schemaVersion") != 1 or report.get("specVersion") != "1.0":
        fail("Unsupported Texture Studio evidence version")
    if report.get("sourceCommit") != source or report.get("sourceTreeClean") is not True:
        fail("Evidence must cover this exact clean source commit")
    if report.get("buildSha256") != build:
        fail("Evidence is for another build")
    if report.get("status") != "PASS" or report.get("openDefects") != []:
        fail("Texture Studio has incomplete checks or open release defects")
    if publication and (not authorized or report.get("publicationAuthorized") is not True):
        fail("Publication needs separate authorization for this candidate")
    checks = report.get("requiredChecks")
    if not isinstance(checks, list) or not checks:
        fail("The required check inventory is empty")
    seen = set()
    verified = {}
    for check in checks:
        name = check["id"]
        if name in seen or check.get("status") != "PASS" or type(check.get("exitCode")) is not int or check["exitCode"] != 0:
            fail(f"Failed, skipped, invalid or duplicate check {name}")
        files = check.get("evidence")
        if not isinstance(files, dict) or not files:
            fail(f"No recorded evidence for {name}")
        for file, checksum in files.items():
            if file in verified and verified[file] != checksum:
                fail(f"Conflicting evidence checksums for {file}")
            if file not in verified:
                verify_file(root, file, checksum)
                verified[file] = checksum
        seen.add(name)
    if missing := CHECKS - seen:
        fail(f"Missing required checks {', '.join(sorted(missing))}")
    return len(seen)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--evidence", required=True, type=Path)
    cli.add_argument("--source", required=True)
    cli.add_argument("--build", required=True)
    cli.add_argument("--repository", required=True, type=Path)
    cli.add_argument("--publish", action="store_true")
    cli.add_argument("--publication-authorized", action="store_true")
    cli.add_argument("--packages", type=Path)
    cli.add_argument("--version")
    args = cli.parse_args()
    if subprocess.check_output(["git", "-C", str(args.repository), "rev-parse", "HEAD"], text=True).strip() != args.source:
        fail("Checkout revision differs from the requested candidate")
    if subprocess.check_output(["git", "-C", str(args.repository), "status", "--porcelain"], text=True):
        fail("Checkout contains unverified changes")
    if args.publish and (not args.packages or not args.version):
        fail("Publication requires the actual package archives")
    if args.packages and (not args.version or candidate_digest(args.packages, args.version) != args.build):
        fail("Actual release packages differ from the verified build")
    count = validate(args.evidence, args.source, args.build, args.publish, args.publication_authorized)
    print(f"PASS Texture Studio release gate with {count} checks")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        print(f"Texture Studio release blocked. {error}", file=sys.stderr)
        sys.exit(1)
