#!/usr/bin/env python3
"""Build a labeled contact sheet for one decoded sprite-resource directory."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--columns", type=int, default=6)
    parser.add_argument("--width", type=int, default=120)
    parser.add_argument("--height", type=int, default=100)
    args = parser.parse_args()

    files = sorted(args.source.glob("*.png"), key=lambda p: int(p.stem.split("_")[0]))
    cell_w, cell_h = args.width + 16, args.height + 34
    rows = math.ceil(len(files) / args.columns)
    sheet = Image.new("RGB", (cell_w * args.columns, cell_h * rows), (16, 16, 20))
    draw = ImageDraw.Draw(sheet)
    for index, path in enumerate(files):
        with Image.open(path) as opened:
            sprite = opened.convert("RGBA")
        sprite.thumbnail((args.width, args.height), Image.Resampling.NEAREST)
        x = (index % args.columns) * cell_w + (cell_w - sprite.width) // 2
        y = (index // args.columns) * cell_h + 4
        sheet.paste(sprite, (x, y), sprite)
        label_x = (index % args.columns) * cell_w + 5
        draw.text((label_x, y + sprite.height + 4), path.stem.replace("_lang0", ""), fill=(245, 245, 245))
        draw.text((label_x, y + sprite.height + 17), f"{opened.width}x{opened.height}", fill=(145, 145, 155))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output, optimize=True)
    print(f"sprites={len(files)} size={sheet.width}x{sheet.height} output={args.output}")


if __name__ == "__main__":
    main()
