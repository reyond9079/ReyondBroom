#!/usr/bin/env python3
"""Check command-line export paths and the contents of a generated PK3."""
import json
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zipfile


def run(executable, root):
    def invoke(*args):
        proc = subprocess.run([str(executable), *map(str, args)], text=True, capture_output=True)
        return proc.returncode, json.loads(proc.stdout)

    source = root / "Кирпич 日本.rbtex.json"
    assert invoke("new", source)[0] == 0
    assert invoke("new", source)[0] != 0
    assert invoke("validate", source)[1]["state"] == "valid"
    palette = root / "grayscale.lmp"
    palette.write_bytes(bytes(value for value in range(256) for _ in range(3)))

    def export(profile, folder, texture, *extra):
        return invoke("export", source, "--profile", profile, "--out", root / folder,
                      "--name", texture, *extra)

    for profile, name, location, magic in (
        ("q1-baseline", "mortar", "texture-studio.wad", b"WAD2"),
        ("goldsrc-baseline", "mortar", "texture-studio.wad", b"WAD3"),
        ("q2-baseline", "custom/mortar", "textures/custom/mortar.wal", b"custom/mortar\0"),
    ):
        assert export(profile, profile, name, "--palette", palette)[0] == 0
        assert (root / profile / location).read_bytes().startswith(magic)
        manifest = json.loads((root / profile / "manifest.json").read_text(encoding="utf-8"))
        assert manifest["profile"] == profile and manifest["files"] == [location]
        assert manifest["paletteSha256"] == hashlib.sha256(palette.read_bytes()).hexdigest()
        packaged = root / profile / "source.rbtex.json"
        assert manifest["sourceSha256"] == hashlib.sha256(packaged.read_bytes()).hexdigest()
        assert manifest["artifacts"][0]["sha256"] == hashlib.sha256((root / profile / location).read_bytes()).hexdigest()

        original = root / profile / location
        imported_dir = root / f"indexed-{profile}"
        indexed_args = ("import", original, "--license", "CC0-1.0", "--out", imported_dir)
        encoding = {"q1-baseline": "wad2", "goldsrc-baseline": "wad3", "q2-baseline": "wal"}[profile]
        if encoding in {"wad2", "wal"}:
            assert invoke(*indexed_args)[0] != 0 and not imported_dir.exists()
            indexed_args += ("--palette", palette)
        if encoding in {"wad2", "wad3"}:
            assert invoke(*indexed_args, "--texture", "absent")[0] != 0
            indexed_args += ("--texture", "MORTAR")
        assert invoke(*indexed_args)[0] == 0
        indexed_source = imported_dir / "source.rbtex.json"
        indexed_doc = json.loads(indexed_source.read_text(encoding="utf-8"))
        indexed_ref, = indexed_doc["assetReferences"]
        assert indexed_ref["encoding"] == encoding
        assert (imported_dir / indexed_ref["path"]).read_bytes() == original.read_bytes()
        installed = root / f"repack-{encoding}.rbtexpkg"
        assert invoke("bundle", indexed_source, "--out", installed)[0] == 0
        restored = root / f"restored-{encoding}"
        assert invoke("unpack", installed, "--out", restored)[0] == 0
        assert invoke("validate", restored / "source.rbtex.json")[0] == 0
        assert (restored / indexed_ref["path"]).read_bytes() == original.read_bytes()

    texture_path = "textures/custom/mortar.tga"
    assert export("q3-baseline", "q3", "custom/mortar")[0] == 0
    assert export("fte-diffuse", "fte", "custom/mortar")[0] == 0
    loose = (root / "q3" / texture_path).read_bytes()
    assert loose == (root / "fte" / texture_path).read_bytes()
    assert export("q3-baseline", "pk3", "custom/mortar", "--pk3")[0] == 0
    status, planned = export("q3-baseline", "preview", "custom/mortar", "--pk3", "--dry-run")
    assert status == 0 and planned["state"] == "planned" and not (root / "preview").exists()
    assert planned["manifest"] == json.loads((root / "pk3/manifest.json").read_text(encoding="utf-8"))
    assert export("q3-baseline", "pk3-repeat", "custom/mortar", "--pk3", "--threads", 4)[0] == 0
    pk3 = root / "pk3" / "texture-studio.pk3"
    assert pk3.read_bytes() == (root / "pk3-repeat" / pk3.name).read_bytes()
    with zipfile.ZipFile(pk3) as archive:
        assert archive.namelist() == [texture_path]
        assert archive.testzip() is None and archive.read(texture_path) == loose

    for invalid in ("custom/../outside", "C:/name", "custom//name", "custom/con", "aux/name"):
        assert export("q3-baseline", "rejected", invalid)[0] != 0
        assert not (root / "rejected").exists() and not (root / "rejected.incoming").exists()
    assert export("fte-diffuse", "wrong-pk3", "custom/mortar", "--pk3")[0] != 0
    assert export("q3-baseline", "jpeg", "custom/mortar", "--jpeg", 92)[0] == 0
    jpeg = (root / "jpeg/textures/custom/mortar.jpg").read_bytes()
    assert jpeg.startswith(b"\xff\xd8") and jpeg.endswith(b"\xff\xd9")
    assert export("q3-baseline", "invalid-jpeg", "custom/mortar", "--jpeg", 101)[0] != 0
    assert export("fte-rich", "missing-emission", "custom/mortar")[0] != 0
    rich = json.loads(source.read_text(encoding="utf-8"))
    rich["outputs"]["emissive"] = {"node": "diffuse", "port": "color"}
    source.write_text(json.dumps(rich), encoding="utf-8")
    assert export("fte-rich", "fte-rich", "custom/mortar")[0] == 0
    rich = root / "fte-rich"
    shader = (rich / "scripts/reyond-texture.shader").read_text(encoding="utf-8")
    assert all(f"{field} textures/custom/mortar{suffix}.tga" in shader for field, suffix in (
        ("diffusemap", ""), ("normalmap", "_norm"), ("fullbrightmap", "_luma")
    ))
    assert (rich / "textures/custom/mortar_luma.tga").read_bytes() == loose
    assert (rich / "textures/custom/mortar_norm.tga").read_bytes() != loose
    assert export("q3-baseline", "invalid-workers", "custom/mortar", "--threads", 17)[0] != 0
    assert export("q3-baseline", "q3", "custom/mortar")[0] != 0
    assert (root / "q3" / texture_path).read_bytes() == loose

    original = root / "q3" / texture_path
    assert invoke("import", original, "--out", root / "unlicensed")[0] != 0
    assert not (root / "unlicensed").exists()
    status, _ = invoke("import", original, "--license", "CC0-1.0", "--out", root / "imported")
    assert status == 0
    imported = root / "imported/source.rbtex.json"
    document = json.loads(imported.read_text(encoding="utf-8"))
    reference, = document["assetReferences"]
    original_copy = imported.parent / reference["path"]
    assert original_copy.read_bytes() == loose and reference["license"] == "CC0-1.0"
    assert invoke("validate", imported)[1]["channels"] == 4
    assert invoke("render", imported, "--out", root / "rendered-import")[0] == 0
    assert (root / "rendered-import/normal.png").is_file()
    assert (root / "rendered-import" / reference["path"]).read_bytes() == loose
    assert original.read_bytes() == loose
    package = root / "portable.rbtexpkg"
    assert invoke("bundle", imported, "--out", package)[0] == 0
    package2 = root / "portable-again.rbtexpkg"
    assert invoke("bundle", imported, "--out", package2)[0] == 0
    assert package.read_bytes() == package2.read_bytes()
    assert invoke("unpack", package, "--out", root / "unpacked")[0] == 0
    assert (root / "unpacked/source.rbtex.json").read_bytes() == imported.read_bytes()
    assert (root / "unpacked" / reference["path"]).read_bytes() == loose
    for member in ("../outside.txt", "unreferenced.txt"):
        bad = root / "bad.rbtexpkg"
        with zipfile.ZipFile(bad, "w") as archive:
            archive.writestr("source.rbtex.json", imported.read_bytes())
            archive.writestr(reference["path"], loose)
            archive.writestr(member, b"synthetic rejection fixture")
        assert invoke("unpack", bad, "--out", root / "rejected-package")[0] != 0
        assert not (root / "rejected-package").exists() and not (root / "outside.txt").exists()
    bad_link = zipfile.ZipInfo(reference["path"])
    bad_link.create_system = 3
    bad_link.external_attr = 0o120777 << 16
    with zipfile.ZipFile(bad, "w") as archive:
        archive.writestr("source.rbtex.json", imported.read_bytes())
        archive.writestr(bad_link, loose)
    assert invoke("unpack", bad, "--out", root / "rejected-link-package")[0] != 0
    reference["path"] = "../outside.tga"
    imported.write_text(json.dumps(document), encoding="utf-8")
    assert invoke("validate", imported)[0] != 0

    animation = root / "ripples.rbtex.json"
    assert invoke("new", animation, "--preset", "Water Ripples")[0] == 0
    for frame in (0, 5, 16):
        assert invoke("render", animation, "--frame", frame, "--out", root / f"frame-{frame}")[0] == 0
    first, middle, wrap = ((root / f"frame-{f}/baseColor.png").read_bytes() for f in (0, 5, 16))
    assert first != middle and wrap == first
    sequence = root / "sequence"
    assert invoke("render", animation, "--all-frames", "--out", sequence)[0] == 0
    assert (sequence / "frames/000/baseColor.png").read_bytes() == first
    assert (sequence / "frames/005/baseColor.png").read_bytes() == middle
    sequence_manifest = json.loads((sequence / "manifest.json").read_text(encoding="utf-8"))
    assert sequence_manifest["frames"] == 16
    assert len(list((sequence / "frames").iterdir())) == 16
    catalog_animation = root / "catalog-ripples.rbtex.json"
    assert invoke("new", catalog_animation, "--preset", "liquids-water", "--seed", 42)[0] == 0
    # This 16-bit channel exposed one-bit libm differences between Linux and Windows.
    for frame, expected in (
        (0, "a41074ca899fd864ee12b97a2c01d23f13a9906d027096115010be3297b84aa4"),
        (3, "2b1cd50ed5a2c7931f69e35f5a88f686a600f8e74aeab9c8b3ba24779db75321"),
        (11, "8ab8213ae14995d8bca2cf47f0334e63f25d8d2df1ae759f3c6fcb466d7e71f6"),
    ):
        frame_output = root / f"catalog-ripples-frame-{frame}"
        assert invoke("render", catalog_animation, "--frame", frame, "--threads", 4, "--out", frame_output)[0] == 0
        assert hashlib.sha256((frame_output / "height.png").read_bytes()).hexdigest() == expected
    common = ("export", catalog_animation, "--all-frames")
    for profile, frame_count, fps, wants_rate_change in (
        ("q1-baseline", 8, 5, False), ("goldsrc-baseline", 8, 10, True),
        ("q2-baseline", 8, 2, True), ("q3-baseline", 8, 5, False), ("fte-diffuse", 8, 5, False),
    ):
        name = "ripples" if profile in ("q1-baseline", "goldsrc-baseline") else "reyond/ripples"
        destination = root / f"anim-{profile}"
        animation_args = (*common, "--profile", profile, "--name", name, "--palette", palette, "--out", destination)
        if profile != "q2-baseline":
            assert invoke(*animation_args)[0] != 0 and not destination.exists()
        animation_args += ("--frames", frame_count)
        if wants_rate_change:
            assert invoke(*animation_args)[0] != 0 and not destination.exists()
            animation_args += ("--accept-rate-change",)
        if profile in ("q3-baseline", "fte-diffuse"):
            animation_args += ("--pk3",)
        status, expected = invoke(*animation_args, "--dry-run")
        assert status == 0 and not destination.exists()
        status, actual = invoke(*animation_args)
        assert status == 0 and actual["manifest"] == expected["manifest"]
        assert actual["manifest"]["frameMap"] == list(range(0, 16, 2))
        assert actual["manifest"]["fps"] == fps
        if name == "ripples":
            archive = (destination / "texture-studio.wad").read_bytes()
            magic, count, directory = struct.unpack_from("<4sII", archive)
            assert magic == (b"WAD2" if profile == "q1-baseline" else b"WAD3")
            assert count == frame_count
            actual_names = [struct.unpack_from("<16s", archive, directory + i * 32 + 16)[0]
                            .rstrip(b"\0").decode("ascii") for i in range(count)]
            assert actual_names == [f"+{i}{name}" for i in range(count)]
        elif profile == "q2-baseline":
            for i in range(frame_count):
                wal = (destination / f"textures/{name}_{i}.wal").read_bytes()
                next_name = wal[56:88].rstrip(b"\0").decode("ascii")
                assert next_name == f"{name}_{(i+1)%frame_count}"
        else:
            with zipfile.ZipFile(destination / "texture-studio.pk3") as runtime:
                assert runtime.testzip() is None
                script_name = actual["manifest"]["shaderListEntry"]
                shader = runtime.read(f"scripts/{script_name}.shader").decode("ascii")
                assert f"animMap {fps} " in shader
                assert all(runtime.getinfo(f"textures/{name}_{i}.tga") for i in range(frame_count))
                assert runtime.read(f"textures/{name}_0.tga") != runtime.read(f"textures/{name}_1.tga")

    corpus = set()
    for variation in range(1, 5):
        path = root / f"diamond-{variation}.rbtex.json"
        assert invoke("new", path, "--preset", "metal-diamond-plate", "--variation", variation, "--seed", 0)[0] == 0
        assert invoke("validate", path)[0] == 0
        variant_source = json.loads(path.read_text(encoding="utf-8"))
        assert variant_source["rootSeed"] == 0
        corpus.add(json.dumps(variant_source["graph"], sort_keys=True))
    assert len(corpus) == 4
    assert invoke("new", root / "rejected-variant", "--preset", "metal-diamond-plate", "--variation", 5)[0] != 0
    roles = {"wall", "floor", "ceiling", "panel", "trim", "door", "damaged", "marking"}
    for family in ("gothic-fortress", "industrial-bunker", "retro-laboratory", "brutalist-city",
                   "abandoned-facility", "alien-infestation"):
        destination = root / family
        family_args = ("family", family, "--out", destination, "--seed", 4294967295)
        code, planned = invoke(*family_args, "--dry-run")
        assert code == 0 and not destination.exists()
        code, generated = invoke(*family_args)
        assert code == 0 and generated["manifest"] == planned["manifest"], (family, code, generated)
        manifest = json.loads((destination / "family.json").read_text(encoding="utf-8"))
        assert manifest == planned["manifest"] and manifest["family"] == family
        assert {member["role"] for member in manifest["materials"]} == roles
        assert len({member["seed"] for member in manifest["materials"]}) == 8
        for member in manifest["materials"]:
            path = destination / member["source"]
            assert hashlib.sha256(path.read_bytes()).hexdigest() == member["sha256"]
            assert invoke("validate", path)[0] == 0
            source = json.loads(path.read_text(encoding="utf-8"))
            assert source["rootSeed"] == member["seed"]
            assert source["provenance"]["family"]["id"] == family
        assert invoke(*family_args)[0] != 0

    reference["path"] = original_copy.relative_to(imported.parent).as_posix()
    imported.write_text(json.dumps(document), encoding="utf-8")
    original_copy.write_bytes(loose[:-1] + b"\0")
    assert invoke("validate", imported)[0] != 0


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="RB Texture Studio 日本-") as tmp:
        run(Path(sys.argv[1]).resolve(), Path(tmp))
    print("PASS profile routes, deterministic PK3, import preservation, safe paths, and refusal to overwrite")
