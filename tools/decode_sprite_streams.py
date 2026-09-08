#!/usr/bin/env python3
"""Decode Blood Pong's custom four-opcode, scanline sprite resources.

Recovered from the original renderer at VA 0x00402F68:
  0 = end image
  1 = begin next scanline
  2 = copy N literal palette indices (payload padded to a DWORD)
  3 = skip N transparent destination pixels
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from PIL import Image


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def decode(data: bytes) -> list[list[int | None]] | None:
    offset = 0
    rows: list[list[int | None]] = []
    ended = False
    while offset + 4 <= len(data):
        command = struct.unpack_from("<I", data, offset)[0]
        offset += 4
        opcode, count = command >> 24, command & 0xFFFFFF
        if opcode == 0:
            ended = True
            break
        if opcode == 1:
            rows.append([])
        elif opcode == 2:
            if not rows or offset + count > len(data):
                return None
            rows[-1].extend(data[offset : offset + count])
            offset += (count + 3) & ~3
        elif opcode == 3:
            if not rows:
                return None
            rows[-1].extend([None] * count)
        else:
            return None
    if not ended or not rows or offset != len(data):
        return None
    return rows


def render(rows: list[list[int | None]], palette: list[int]) -> Image.Image:
    width = max((len(row) for row in rows), default=1)
    image = Image.new("RGBA", (max(1, width), len(rows)), (0, 0, 0, 0))
    pixels = image.load()
    for y, row in enumerate(rows):
        for x, value in enumerate(row):
            if value is None:
                continue
            base = value * 3
            pixels[x, y] = (*palette[base : base + 3], 255)
    return image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("resources", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--palette", type=Path)
    args = parser.parse_args()

    palette_source = args.palette or next(args.resources.rglob("*.bmp"))
    with Image.open(palette_source) as reference:
        palette = reference.getpalette()
    if palette is None or len(palette) < 768:
        raise SystemExit(f"No 256-color palette in {palette_source}")

    records: list[dict[str, object]] = []
    rejected: list[str] = []
    for source in sorted(args.resources.rglob("*.bin")):
        data = source.read_bytes()
        rows = decode(data)
        if rows is None:
            rejected.append(source.relative_to(args.resources).as_posix())
            continue
        image = render(rows, palette)
        relative = source.relative_to(args.resources).with_suffix(".png")
        target = args.output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        image.save(target, optimize=True)
        records.append(
            {
                "source": source.relative_to(args.resources).as_posix(),
                "output": relative.as_posix(),
                "width": image.width,
                "height": image.height,
                "sourceSize": len(data),
                "sourceSha256": sha256(data),
            }
        )

    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {
        "format": {
            "rendererAddress": "0x00402F68",
            "commands": {"0": "end", "1": "next scanline", "2": "literal pixels", "3": "transparent skip"},
            "literalAlignment": 4,
            "palette": str(palette_source),
        },
        "decodedCount": len(records),
        "rejectedCount": len(rejected),
        "sprites": records,
        "rejected": rejected,
    }
    (args.output / "sprite_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"decoded={len(records)} rejected={len(rejected)} palette={palette_source}")


if __name__ == "__main__":
    main()
