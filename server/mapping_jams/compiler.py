"""Validate source snapshots and build Quake 1 play artifacts."""

import hashlib
import io
import itertools
import json
import math
import re
import shutil
import struct
import subprocess
import zipfile
from pathlib import Path

MAX_SOURCE = 2 * 1024 * 1024
MAX_ARTIFACT = 32 * 1024 * 1024
TOKEN = re.compile(r'//[^\n]*|"(?:[^"\\\n]|\\["\\])*"|[{}()[\]]|[^\s{}()[\]"]+')
FACE = re.compile(
    r'^\s*(\([^()]+\)\s*\([^()]+\)\s*\([^()]+\))\s+'
    r'("[a-zA-Z0-9_+*./{}-]+"|[a-zA-Z0-9_+*./{}-]+)\s+([\d.eE+\-\[\]\s]+)$'
)
POINT = re.compile(r'\(([^()]+)\)')
KEY = re.compile(r'^[a-zA-Z_][a-zA-Z0-9_]*$')


class MapError(ValueError):
    def __init__(self, message, violations=None):
        super().__init__(message)
        self.violations = violations or []


def digest(data):
    return hashlib.sha256(data).hexdigest()


def vector(text):
    numbers = tuple(map(float, text.split()))
    if len(numbers) != 3 or any(not math.isfinite(n) or abs(n) > 131072 for n in numbers):
        raise MapError("Invalid Quake coordinate.")
    return numbers


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def mul(a, k):
    return tuple(x * k for x in a)


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0])


def brush_vertices(planes):
    if not 4 <= len(planes) <= 32:
        raise MapError("A brush must have between 4 and 32 sides in the local Jam.")
    vertices = []
    for (a, da), (b, db), (c, dc) in itertools.combinations(planes, 3):
        bc = cross(b, c)
        determinant = dot(a, bc)
        if abs(determinant) < 1e-7:
            continue
        p = mul(add(add(mul(bc, da), mul(cross(c, a), db)), mul(cross(a, b), dc)), 1 / determinant)
        if all(dot(n, p) <= d + 0.001 for n, d in planes):
            vertices.append(p)
    if len(vertices) < 4:
        raise MapError("A brush is degenerate or has incorrectly oriented faces.")
    return vertices


def source_map(data):
    if data.startswith(b"PK"):
        try:
            with zipfile.ZipFile(io.BytesIO(data)) as package:
                entries = package.infolist()
                if sorted(x.filename for x in entries) != ["manifest.json", "map/source.map"]:
                    raise MapError("The local Jam accepts a manifest and one MAP using base assets.")
                if any(x.file_size > MAX_SOURCE or x.flag_bits & 1 for x in entries):
                    raise MapError("Source archive exceeds the local limits.")
                manifest = json.loads(package.read("manifest.json"))
                data = package.read("map/source.map")
                if manifest.get("schemaVersion") != 1 or manifest.get("sha256") != digest(data):
                    raise MapError("Source manifest does not match the MAP.")
        except (zipfile.BadZipFile, ValueError, TypeError, AttributeError) as error:
            if isinstance(error, MapError):
                raise
            raise MapError("Invalid source archive.") from error
    if not data or len(data) > MAX_SOURCE:
        raise MapError("Invalid MAP size.")
    return data.decode("utf-8-sig")


def map_snapshot(data, rules, trusted_wads):
    try:
        return _map_snapshot(source_map(data), rules, trusted_wads)
    except (UnicodeError, ValueError) as error:
        if isinstance(error, MapError):
            raise
        raise MapError("Invalid MAP syntax or numeric value.") from error


def _map_snapshot(text, rules, trusted_wads):
    # Re-emit only parsed entities, geometry and trusted WAD paths.
    depth, brushes, entities = 0, 0, []
    output, points, planes = [], [], []
    for source in text.splitlines():
        line = re.sub(r'//[^"]*$', "", source).strip() if not source.lstrip().startswith('"') else source.strip()
        if not line:
            continue
        if line == "{":
            depth += 1
            if depth == 1:
                entities.append({})
                if len(entities) > rules["maxEntities"]:
                    raise MapError("Too many entities.", [violation("entities", len(entities), rules["maxEntities"])])
            elif depth == 2:
                brushes += 1
                if brushes > rules["maxBrushes"]:
                    raise MapError("Too many brushes.", [violation("brushes", brushes, rules["maxBrushes"])])
                planes = []
            else:
                raise MapError("Unsupported MAP nesting.")
        elif line == "}":
            if depth == 2:
                points.extend(brush_vertices(planes))
            elif depth == 1 and len(entities) == 1:
                output.append('"wad" "' + ";".join(p.as_posix() for p in trusted_wads) + '"')
            elif depth != 1:
                raise MapError("Unbalanced MAP braces.")
            depth -= 1
        elif depth == 1:
            tokens = TOKEN.findall(line)
            if len(tokens) != 2 or any(not t.startswith('"') for t in tokens):
                raise MapError("Invalid entity property.")
            key, value = (t[1:-1] for t in tokens)
            if not KEY.fullmatch(key) or len(value) > 512 or any(ord(ch) < 32 for ch in value):
                raise MapError("Invalid entity property.")
            key = key.lower()
            if key == "origin":
                # The editor's QuakeC vector formatter can add paired apostrophes.
                coordinates = value.strip()
                if coordinates.startswith("'") and coordinates.endswith("'"):
                    value = " ".join(coordinates[1:-1].split())
                    line = f'"origin" "{value}"'
                points.append(vector(value))
            if key in entities[-1]:
                raise MapError("Duplicate entity property.")
            entities[-1][key] = value
            if key in {"wad", "_wad"}:
                continue
            if key in {"model", "noise", "noise1", "noise2", "noise3", "sounds"} and ("/" in value or "\\" in value):
                if not re.fullmatch(r'(?:progs|maps|ambience|misc|doors|plats|sound)/[a-zA-Z0-9_/.-]+', value) or ".." in value:
                    raise MapError("An entity refers to an unsupported resource path.")
            if key in {"map", "nextmap"}:
                raise MapError("Level transitions are unavailable in the local Jam.")
        elif depth == 2:
            face = FACE.fullmatch(line)
            if not face:
                raise MapError("Unsupported brush face syntax.")
            a, b, c = [vector(p) for p in POINT.findall(face[1])]
            n = cross(sub(c, a), sub(b, a))
            norm = math.sqrt(dot(n, n))
            if norm < 1e-8:
                raise MapError("Degenerate brush plane.")
            n = mul(n, 1 / norm)
            planes.append((n, dot(n, a)))
            if len(planes) > 32:
                raise MapError("Too many sides on a brush.")
            line = " ".join((face[1], face[2].strip('"'), face[3]))
        else:
            raise MapError("Unexpected text outside an entity.")
        output.append(line)
    if depth or not entities or entities[0].get("classname") != "worldspawn":
        raise MapError("The first entity must be worldspawn.")
    if sum(e.get("classname") == "worldspawn" for e in entities) != 1:
        raise MapError("A MAP must have exactly one worldspawn.")
    if not brushes or not any(e.get("classname") == "info_player_start" and "origin" in e for e in entities):
        raise MapError("A MAP needs geometry and an info_player_start.")
    violations = []
    for axis, name in enumerate("xyz"):
        low, high = min(p[axis] for p in points), max(p[axis] for p in points)
        if high - low > rules["bounds"]["maxSize"][axis] + 0.001:
            violations.append(violation("size_" + name, high-low, rules["bounds"]["maxSize"][axis], "quake_units"))
        if max(abs(high), abs(low)) > rules["bounds"]["worldLimit"] + 0.001:
            violations.append(violation("world_" + name, max(abs(high), abs(low)), rules["bounds"]["worldLimit"], "quake_units"))
    if violations:
        raise MapError("Map exceeds the Jam bounds.", violations)
    return "\n".join(output) + "\n"


def violation(metric, actual, limit, unit="count"):
    return {"metric": metric, "actual": actual, "limit": limit, "unit": unit}


def build_map(source, output, rules, tools, wads, *, hub=False):
    output.mkdir(parents=True, exist_ok=True)
    (output / "w").mkdir()
    local_wads = [Path("w") / (str(i) + ".wad") for i in range(len(wads))]
    for original, local in zip(wads, local_wads):
        try:
            (output / local).hardlink_to(original)
        except OSError:
            shutil.copyfile(original, output / local)
    stem = "jam_hub" if hub else "jam_map"
    map_path = output / (stem + ".map")
    if hub:
        # Only the generated, first-party Hub uses Quake level transitions.
        text = source.decode("ascii").replace("@jam-wad@", ";".join(p.as_posix() for p in local_wads))
    else:
        text = map_snapshot(source, rules, local_wads)
    map_path.write_text(text, encoding="utf-8")
    bsp = output / (stem + ".bsp")
    suffix = ".exe" if (tools / "qbsp.exe").is_file() else ""
    for name, args in (("qbsp", ["-noverbose", str(map_path)]), ("light", ["-lit", str(bsp)])):
        exe = tools / (name + suffix)
        if not exe.is_file():
            raise MapError(f"Trusted compiler {name} is missing.")
        with (output / (name + ".log")).open("wb") as log:
            result = subprocess.run([str(exe), *args], cwd=output, stdin=subprocess.DEVNULL,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=45, check=False)
        if result.returncode:
            raise MapError(f"{name} failed. Inspect the local build log.")
        if name == "qbsp" and any((output / (stem + ext)).exists() for ext in (".pts", ".lin")):
            raise MapError("The map leaks. Seal the level before publishing.")
    if not bsp.is_file() or not 124 <= bsp.stat().st_size <= MAX_ARTIFACT:
        raise MapError("Invalid compiled BSP.")
    with bsp.open("rb") as stream:
        if struct.unpack("<I", stream.read(4))[0] != 29:
            raise MapError("The compiler did not produce Quake 1 BSP29.")
    lit = bsp.with_suffix(".lit")
    files = {"maps/" + bsp.name: bsp.read_bytes()}
    if lit.is_file():
        if lit.stat().st_size + bsp.stat().st_size > MAX_ARTIFACT:
            raise MapError("Compiled files exceed the local size limit.")
        files["maps/" + lit.name] = lit.read_bytes()
    artifact = output / "play.zip"
    with zipfile.ZipFile(artifact, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, value in files.items():
            archive.writestr(name, value)
    return artifact, [{"path": name, "sizeBytes": len(data), "sha256": digest(data)}
                      for name, data in files.items()]
