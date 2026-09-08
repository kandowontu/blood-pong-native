#!/usr/bin/env python3
"""Verify that a native build embeds every original game-data resource exactly."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import pefile


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def numeric(entry: object) -> int | None:
    return None if entry.name is not None else int(entry.struct.Id)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("native_executable", type=Path)
    args = parser.parse_args()

    original = json.loads(args.manifest.read_text(encoding="utf-8"))
    expected = {
        (int(item["typeId"]), int(item["name"])): item["sha256"]
        for item in original["resources"]
        if item["typeId"] is not None and int(item["typeId"]) >= 1000 and str(item["name"]).isdecimal()
    }

    pe = pefile.PE(str(args.native_executable), fast_load=False)
    pe.parse_data_directories()
    actual: dict[tuple[int, int], str] = {}
    for type_entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        type_id = numeric(type_entry)
        if type_id is None or type_id < 1000:
            continue
        for name_entry in type_entry.directory.entries:
            name_id = numeric(name_entry)
            if name_id is None:
                continue
            leaves = name_entry.directory.entries
            if len(leaves) != 1:
                raise SystemExit(f"Unexpected language count for {type_id}/{name_id}: {len(leaves)}")
            leaf = leaves[0].data.struct
            actual[(type_id, name_id)] = digest(pe.get_data(leaf.OffsetToData, leaf.Size))

    missing = sorted(expected.keys() - actual.keys())
    extra = sorted(actual.keys() - expected.keys())
    changed = sorted(key for key in expected.keys() & actual.keys() if expected[key] != actual[key])
    print(f"expected={len(expected)} actual={len(actual)} missing={len(missing)} extra={len(extra)} changed={len(changed)}")
    if missing:
        print(f"first_missing={missing[:10]}")
    if extra:
        print(f"first_extra={extra[:10]}")
    if changed:
        print(f"first_changed={changed[:10]}")
    if missing or extra or changed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
