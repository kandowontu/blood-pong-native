#!/usr/bin/env python3
"""Catalog the legacy Creative VOC assets and recovered gameplay bindings."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


FIGHTER_NAMES = [
    "Fung Shwei", "Lo Than", "Jewel", "Raptor", "So Frio", "Nai Palm",
    "One Eye", "Raider", "Show Lin", "Dawg Cau", "Omoh", "Carmack",
    "Pain", "Lo Pan", "Mai Lai", "Baka",
]
FIGHTER_VOICE_IDS = [
    2000, 2004, 2001, 2005, 2002, 2007, 2003, 2006,
    2010, 2014, 2011, 2015, 2012, 2017, 2013, 2016,
]
PROJECTILE_SOUND_IDS = [
    0, 3000, 3003, 3005, 3008, 3007, 3010, 3010, 3012, 3032,
    3013, 2050, 3014, 3019, 3020, 3013, 3012, 3021, 3031, 3030,
    3029, 3023, 3025, 3025, 3020,
]


def parse_voc(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    if len(data) < 26 or not data.startswith(b"Creative Voice File"):
        raise ValueError(f"not a supported Creative VOC file: {path}")
    offset = int.from_bytes(data[20:22], "little")
    sample_rate = 0
    sample_count = 0
    blocks: list[int] = []
    while offset < len(data):
        block_type = data[offset]
        offset += 1
        if block_type == 0:
            break
        if offset + 3 > len(data):
            raise ValueError(f"truncated VOC block header: {path}")
        length = int.from_bytes(data[offset:offset + 3], "little")
        offset += 3
        if offset + length > len(data):
            raise ValueError(f"truncated VOC block body: {path}")
        blocks.append(block_type)
        if block_type == 1 and length >= 2:
            time_constant, codec = data[offset], data[offset + 1]
            if codec != 0 or time_constant == 255:
                raise ValueError(f"unsupported VOC codec/rate: {path}")
            rate = 1_000_000 // (256 - time_constant)
            if sample_rate and sample_rate != rate:
                raise ValueError(f"changing VOC sample rate: {path}")
            sample_rate = rate
            sample_count += length - 2
        elif block_type == 2:
            if not sample_rate:
                raise ValueError(f"continuation before sound-data block: {path}")
            sample_count += length
        offset += length
    return {
        "resourceId": int(path.stem.split("_")[0]),
        "bytes": len(data),
        "sampleRate": sample_rate,
        "sampleCount": sample_count,
        "durationSeconds": round(sample_count / sample_rate, 6) if sample_rate else 0,
        "blockTypes": blocks,
        "sha256": hashlib.sha256(data).hexdigest().upper(),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("resource_directory", type=Path)
    parser.add_argument("json_output", type=Path)
    parser.add_argument("markdown_output", type=Path)
    args = parser.parse_args()

    assets = sorted(
        (parse_voc(path) for path in args.resource_directory.glob("*_lang*.voc")),
        key=lambda item: int(item["resourceId"]),
    )
    by_id = {int(item["resourceId"]): item for item in assets}
    bindings = {
        "announcer": {
            "round": 1000, "round1": 1001, "round2": 1002, "round3": 1003,
            "fight": 1004, "finishHim": 1005, "finishHer": 1006,
            "fatality": 250, "realmTransport": 5001,
        },
        "ui": {"move": 3001, "select": 3002},
        "physics": {"paddleHit": 501, "arenaBounce": 500},
        "projectileAlternate": {"type4Mode11": 3028, "type7SecondPhase": 3011},
        "fighterVoices": dict(zip(FIGHTER_NAMES, FIGHTER_VOICE_IDS)),
        "projectileStartByType": {
            str(index): resource_id
            for index, resource_id in enumerate(PROJECTILE_SOUND_IDS)
            if resource_id
        },
    }
    referenced_ids: set[int] = set()

    def collect(value: object) -> None:
        if isinstance(value, int):
            referenced_ids.add(value)
        elif isinstance(value, dict):
            for child in value.values():
                collect(child)

    collect(bindings)
    missing = sorted(referenced_ids - by_id.keys())
    payload = {
        "format": "Creative Voice File, unsigned 8-bit PCM mono",
        "resourceType": 2001,
        "assetCount": len(assets),
        "bindings": bindings,
        "missingReferencedResources": missing,
        "assets": assets,
    }
    args.json_output.parent.mkdir(parents=True, exist_ok=True)
    args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
    args.json_output.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Audio resource audit",
        "",
        f"All {len(assets)} resources of custom PE type 2001 are Creative VOC, "
        "unsigned 8-bit mono PCM. The native mixer converts them to a concurrent "
        "22,050 Hz/16-bit output stream at playback time; source bytes remain embedded unchanged.",
        "",
        "## Recovered call-site bindings",
        "",
        "- `0x00413BD0` loads announcer IDs 1000-1008 and 250.",
        "- `0x004141EC` plays ROUND at stage 1, the round-number voice at stage 19, and FIGHT at stage 37.",
        "- `0x004143C4` selects FINISH HIM (1005) or FINISH HER (1006).",
        "- `0x0040AA64` loads the sixteen character voices and UI cues 3001/3002.",
        "- `0x0041A570` loads the projectile sound bank used by the 25-way dispatcher.",
        "",
        "### Character-selection voices",
        "",
        "| Fighter | Resource ID | Duration |",
        "|---|---:|---:|",
    ]
    for name, resource_id in zip(FIGHTER_NAMES, FIGHTER_VOICE_IDS):
        duration = by_id[resource_id]["durationSeconds"]
        lines.append(f"| {name} | {resource_id} | {duration:.3f} s |")
    lines.extend([
        "",
        "### Projectile start cues",
        "",
        "| Original type | Resource ID |",
        "|---:|---:|",
    ])
    for index, resource_id in enumerate(PROJECTILE_SOUND_IDS):
        if resource_id:
            lines.append(f"| {index} | {resource_id} |")
    lines.extend([
        "",
        "Type 4 uses 3028 instead of 3008 in mode 11. Type 7 plays 3011 when its rising phase changes into the falling phase.",
        "",
        f"Referenced resource validation: {'PASS' if not missing else 'FAIL'}"
        + ("" if not missing else f" (missing: {missing})"),
        "",
    ])
    args.markdown_output.write_text("\n".join(lines), encoding="utf-8")

    print(f"assets={len(assets)} referenced={len(referenced_ids)} missing={len(missing)}")


if __name__ == "__main__":
    main()
