#!/usr/bin/env python3
"""Decode Blood Pong type-1002 raw indexed images using the shared game palette."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from PIL import Image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("palette_bmp", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    raw = args.source.read_bytes()
    if len(raw) < 8:
        raise SystemExit("resource is shorter than its width/height header")
    width, height = struct.unpack_from("<II", raw)
    if width <= 0 or height <= 0 or len(raw) != 8 + width * height:
        raise SystemExit("resource size does not match the raw indexed-image header")

    palette_source = Image.open(args.palette_bmp).convert("P")
    image = Image.frombytes("P", (width, height), raw[8:])
    image.putpalette(palette_source.getpalette())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output, optimize=True)
    print(f"decoded={width}x{height} output={args.output}")


if __name__ == "__main__":
    main()
