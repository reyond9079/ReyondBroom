#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the reproducible Character Creator core checks in a disposable directory."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def check_assets():
    root = ROOT / "runtime" / "characters"
    manifest = json.loads((root / "provenance.json").read_text(encoding="utf-8"))
    expected = {root/name: digest for name,digest in manifest["files"].items()}
    example_root = root / "examples"
    examples = json.loads((example_root/"provenance.json").read_text(encoding="utf-8"))["files"]
    if len(examples) != 10 or set(examples) != {sample.name for sample in example_root.glob("*.rbchar")}:
        raise ValueError("The bundled example registry must describe all ten character documents")
    expected.update({example_root/name: digest for name,digest in examples.items()})
    for shape in ("face","body"):
        shapes = json.loads((root/f"{shape}-shapes-provenance.json").read_text(encoding="utf-8"))
        expected.update({root/name: digest for name,digest in shapes["files"].items()})
    wardrobe = root / "wardrobe"
    registry = json.loads((wardrobe / "provenance.json").read_text(encoding="utf-8"))
    ids, atlas_ids = set(), set()
    for item in registry["assets"]:
        path = wardrobe / item["file"]
        if item["id"] in ids or path.parent != wardrobe or item["license"] != "CC0-1.0":
            raise ValueError("Invalid or duplicate wardrobe identity")
        ids.add(item["id"])
        atlas_ids.add(item.get("material","hd-"+item["id"]))
        expected[path] = item["sha256"]
    material_root = root / "materials"
    material_pack = json.loads((material_root/"provenance.json").read_text(encoding="utf-8"))
    if material_pack.get("version") != 2 or "pack" in material_pack:
        raise ValueError("Expected original Character Creator materials")
    for item in material_pack["textures"].values():
        path = material_root/item["file"]
        if path.parent != material_root or path.suffix not in {".jpg",".png"}:
            raise ValueError("Invalid bundled character image path")
        if "sources" in item or "makehuman" in item["source"].lower():
            raise ValueError(f"Upstream material atlas is referenced: {path.name}")
        for name, digest in item["authoringInputs"].items():
            source = ROOT/name
            if not source.is_relative_to(ROOT) or ".." in Path(name).parts:
                raise ValueError("Material input must be in the source tree")
            expected[source] = digest
        expected[path] = item["sha256"]
    source = material_pack["detailSource"]
    expected[ROOT / source.get("file","assets/characters/material-macro-source.png")] = source["sha256"]
    expected.update((ROOT/name, digest) for name, digest in material_pack["pbrInputs"].items())
    for label in ("pbrDetail","pbrFabric"):
        detail = material_pack[label]
        expected[ROOT/"runtime"/detail["file"]] = detail["sha256"]
    for key in atlas_ids | {"eyes-pbr-brown","eyes-pbr-blue","eyes-pbr-green"}:
        if material_pack["textures"][key]["size"]!=2048:
            raise ValueError(f"FTE character atlas is not 2K: {key}")
    for path,digest in expected.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Bundled asset checksum mismatch: {path.name}")
    print(f"PASS provenance checks for {len(expected)} assets",flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx",default=os.environ.get("CXX","g++"))
    parser.add_argument("--output",type=Path,help="Keep fixtures and results in this directory")
    parser.add_argument("--stress",action="store_true",help="Fit and skin 1000 randomized dressed characters")
    parser.add_argument("--sanitize",action="store_true",help="Build the core with AddressSanitizer and UBSan")
    args = parser.parse_args()
    check_assets()
    with tempfile.TemporaryDirectory(prefix="reyondbroom-character-") as scratch:
        out = (args.output or Path(scratch)).resolve()
        out.mkdir(parents=True,exist_ok=True)
        binary = out / ("character-core.exe" if os.name == "nt" else "character-core")
        flags = ["-O1","-g","-fno-omit-frame-pointer","-fsanitize=address,undefined",
                 "-fno-sanitize-recover=all"] if args.sanitize else ["-O2"]
        subprocess.run([args.cxx,"-std=c++17",*flags,str(ROOT/"tests/test-character-core.cpp"),
                        "-o",str(binary)],check=True)
        glb = out/"character.glb"
        iqm = out/"character.iqm"
        legacy = out/"legacy"
        subprocess.run([str(binary),str(ROOT/"runtime/characters/humanoid-v1.rbh"),
                        str(glb),str(iqm),str(legacy)],check=True)
        subprocess.run([sys.executable,str(ROOT/"scripts/test-character-fixtures.py"),
                        str(glb),str(iqm),"--legacy",str(legacy)],check=True)
        samples = sorted((legacy/"characters").glob("*.rbchar"))
        if len(samples) != 10 or any(
            json.loads(sample.read_text(encoding="utf-8"))["name"] != sample.stem or
            sample.read_bytes() != (ROOT/"runtime/characters/examples"/sample.name).read_bytes()
            for sample in samples
        ):
            raise ValueError("The bundled character examples differ from the preset library")
        if args.stress:
            stress = out / ("character-stress.exe" if os.name == "nt" else "character-stress")
            subprocess.run([args.cxx,"-std=c++17",*flags,str(ROOT/"tests/test-character-stress.cpp"),
                            "-o",str(stress)],check=True)
            fixture_args = [str(stress),str(ROOT/"runtime/characters"),str(out/"stress-documents")]
            subprocess.run(fixture_args,check=True)
            subprocess.run([*fixture_args,"--reopen"],check=True)


if __name__ == "__main__":
    main()
