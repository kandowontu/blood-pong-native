#!/usr/bin/env python3
"""Catalog extracted bitmap resources and build deterministic contact sheets."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def write_sheet(items: list[dict[str, object]], root: Path, target: Path, thumb: tuple[int, int]) -> None:
    if not items:
        return
    columns = min(4, len(items))
    rows = math.ceil(len(items) / columns)
    cell_w, cell_h = thumb[0] + 20, thumb[1] + 44
    sheet = Image.new("RGB", (columns * cell_w, rows * cell_h), (18, 18, 22))
    draw = ImageDraw.Draw(sheet)
    for index, item in enumerate(items):
        source = root / str(item["path"])
        with Image.open(source) as opened:
            frame = opened.convert("RGB")
        frame.thumbnail(thumb, Image.Resampling.NEAREST)
        x0 = (index % columns) * cell_w + (cell_w - frame.width) // 2
        y0 = (index // columns) * cell_h + 6
        sheet.paste(frame, (x0, y0))
        label = source.stem.replace("_lang0", "")
        draw.text(((index % columns) * cell_w + 8, y0 + frame.height + 5), label, fill=(235, 235, 235))
        draw.text(
            ((index % columns) * cell_w + 8, y0 + frame.height + 19),
            f"{item['width']}x{item['height']}",
            fill=(150, 150, 160),
        )
    target.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(target, optimize=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("resources", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    records: list[dict[str, object]] = []
    for path in sorted(args.resources.rglob("*.bmp")):
        with Image.open(path) as image:
            record = {
                "path": path.relative_to(args.resources).as_posix(),
                "width": image.width,
                "height": image.height,
                "mode": image.mode,
                "colors": len(image.getcolors(maxcolors=1 << 24) or []),
                "sha256": sha256(path),
            }
        records.append(record)

    screens = [item for item in records if int(item["width"]) >= 200 and int(item["height"]) >= 150]
    portraits = [item for item in records if int(item["width"]) == 80 and int(item["height"]) == 100]
    interface = [item for item in records if item not in screens and item not in portraits]

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "bitmap_catalog.json").write_text(
        json.dumps({"count": len(records), "bitmaps": records}, indent=2) + "\n",
        encoding="utf-8",
    )
    write_sheet(screens, args.resources, args.output / "screens.png", (272, 216))
    write_sheet(portraits, args.resources, args.output / "portraits.png", (160, 200))
    write_sheet(interface, args.resources, args.output / "interface.png", (240, 100))
    print(f"bitmaps={len(records)} screens={len(screens)} portraits={len(portraits)} interface={len(interface)}")


if __name__ == "__main__":
    main()
