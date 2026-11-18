#!/usr/bin/env python3
"""Independent structural checks for the Texture Studio reference fixtures."""
import argparse
import hashlib
from pathlib import Path
import struct
import zlib


def png_payload(path):
    stream = path.read_bytes()
    assert stream[:8] == b"\x89PNG\r\n\x1a\n"
    pos = 8
    chunks = {}
    while pos < len(stream):
        size, tag = struct.unpack_from(">I4s", stream, pos)
        assert pos + 12 + size <= len(stream)
        chunk = stream[pos + 8:pos + 8 + size]
        expected = struct.unpack_from(">I", stream, pos + 8 + size)[0]
        assert zlib.crc32(tag + chunk) == expected
        chunks.setdefault(tag, []).append(chunk)
        pos += 12 + size
        if tag == b"IEND":
            assert pos == len(stream)
            break
    return struct.unpack(">2I5B", chunks[b"IHDR"][0]), chunks, zlib.decompress(b"".join(chunks[b"IDAT"]))


def verify_wad(path, magic):
    data = path.read_bytes()
    signature, count, directory = struct.unpack_from("<4sII", data)
    assert signature == magic and count == 1 and directory + 32 == len(data)
    offset, stored, raw, kind, compression, _, _, name = struct.unpack_from("<3I4B16s", data, directory)
    assert offset == 12 and stored == raw and offset + raw == directory and compression == 0
    assert kind == (67 if magic == b"WAD3" else 68)
    image_name, width, height, *offsets = struct.unpack_from("<16s6I", data, offset)
    assert name == image_name and (width, height) == (128, 128)
    previous = 40
    for level, index in enumerate(offsets):
        size = (width >> level) * (height >> level)
        assert index == previous and index + size <= raw
        values = data[offset + index:offset + index + size]
        if magic == b"WAD3":
            assert values.count(255) == size // 2
        else:
            assert max(values) < 224
        previous = index + size
    if magic == b"WAD3":
        assert name[0] == ord("{")
        assert data[offset + previous:offset + previous + 2] == b"\0\1"
        assert data[offset + previous + 767:offset + previous + 770] == b"\0\0\xff"


def wal(path):
    data = path.read_bytes()
    name, width, height, *fields = struct.unpack_from("<32s6I32s3i", data)
    assert name.rstrip(b"\0") == b"studio/brick" and (width, height) == (128, 128)
    assert fields[4].rstrip(b"\0") == b"" and fields[5:] == [0, 0, 0]
    offset = 100
    for level, current in enumerate(fields[:4]):
        assert current == offset
        offset += (width >> level) * (height >> level)
    assert offset == len(data)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("fixtures", type=Path)
    cli.add_argument("--compare", type=Path)
    options = cli.parse_args()
    root = options.fixtures
    header, chunks, rgba = png_payload(root / "brick.png")
    assert header == (128, 128, 8, 6, 0, 0, 0) and len(chunks[b"sRGB"]) == 1
    width = 128
    assert len(rgba) == 128 * (1 + 4 * width)
    tga = (root / "brick.tga").read_bytes()
    assert tga[2] == 2 and tga[16:18] == bytes((32, 40))
    assert struct.unpack_from("<2H", tga, 12) == (128, 128)
    for y in range(128):
        start = y * (1 + width * 4)
        assert rgba[start] == 0
        for x in range(width):
            r, g, b, a = rgba[start + 1 + x * 4:start + 5 + x * 4]
            pos = 18 + (y * width + x) * 4
            assert tga[pos:pos + 4] == bytes((b, g, r, a)) and a == 255
    height_header, height_chunks, height_data = png_payload(root / "height16.png")
    assert height_header == (128, 128, 16, 0, 0, 0, 0) and b"sRGB" not in height_chunks
    assert len(height_data) == 128 * (1 + 2 * width)
    normal_header, normal_chunks, _ = png_payload(root / "normal.png")
    assert normal_header == (128, 128, 8, 2, 0, 0, 0) and b"sRGB" not in normal_chunks
    verify_wad(root / "brick.wad2", b"WAD2")
    verify_wad(root / "brick.wad3", b"WAD3")
    wal(root / "brick.wal")
    print("PASS independent PNG, TGA, WAD2, WAD3 and WAL validation")
    if options.compare:
        for name in ("brick.png", "normal.png", "height16.png", "brick.tga", "brick.wad2", "brick.wad3", "brick.wal"):
            digest = hashlib.sha256((root / name).read_bytes()).hexdigest()
            assert hashlib.sha256((options.compare / name).read_bytes()).hexdigest() == digest, name
            print(f"PASS identical reference fixture {name} {digest}")


if __name__ == "__main__":
    main()
