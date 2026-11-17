#!/usr/bin/env python3
"""Retain release logs and representative evidence without copying runtime assets."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


ARTIFACT_BUDGET = 480 * 1024 * 1024
QA_EXTENSIONS = {".json", ".jsonl", ".png", ".log", ".map", ".cfg", ".txt", ".sha256"}
SUPPLEMENTAL_EXTENSIONS = {".png", ".map"}


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def collect(qa, release, output):
    output.mkdir(parents=True)
    files, duplicates, supplemental = {}, {}, {}
    canonical = {}
    total = 0

    def retain(source, relative, optional):
        nonlocal total
        key = relative.as_posix()
        checksum = digest(source)
        size = source.stat().st_size
        if checksum in canonical:
            duplicates[key] = {"path": canonical[checksum], "sha256": checksum}
            return
        if optional and total + size >= ARTIFACT_BUDGET:
            supplemental[key] = {"sha256": checksum, "bytes": size}
            return
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        if digest(target) != checksum:
            raise RuntimeError(f"Evidence changed while copying {relative}")
        files[key] = checksum
        canonical[checksum] = key
        total += size

    images_and_maps = []
    for root, prefix, extensions in [(release, "release-sidecars", {".sha256"}),
                                     (qa, "qa", QA_EXTENSIONS)]:
        for source in sorted(root.rglob("*")):
            if source.is_symlink():
                raise ValueError(f"Unexpected symlink in QA output: {source}")
            suffix = source.suffix.lower()
            if not source.is_file() or suffix not in extensions:
                continue
            relative = Path(prefix) / source.relative_to(root)
            if suffix in SUPPLEMENTAL_EXTENSIONS:
                images_and_maps.append((source, relative))
                continue
            retain(source, relative, False)

    # Keep UI captures ahead of exported source maps when both sets are large.
    for source, relative in sorted(images_and_maps, key=lambda pair: pair[0].suffix.lower() != ".png"):
        retain(source, relative, True)
    if not files:
        raise ValueError("No release evidence was produced")
    inventory = {"files": files, "duplicates": duplicates, "supplementalOverBudget": supplemental,
                 "bytes": total, "method": "Exact hashes for copied files and omitted supplemental evidence. This inventory does not assert test success."}
    (output / "collection.json").write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
    print(f"Retained {len(files)} evidence files ({total} bytes), referenced {len(duplicates)} duplicates "
          f"and inventoried {len(supplemental)} supplemental files beyond the artifact budget")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qa", type=Path, required=True)
    parser.add_argument("--release", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    collect(args.qa, args.release, args.output)


if __name__ == "__main__":
    main()
