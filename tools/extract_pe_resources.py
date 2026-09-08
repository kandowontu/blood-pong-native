#!/usr/bin/env python3
"""Extract every Blood Pong PE resource and write a hashed manifest.

The script never loads or executes the legacy image.  It parses PE structures as
data with pefile and preserves each leaf resource byte-for-byte.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

import pefile


STANDARD_TYPES = {
    1: "cursor",
    2: "bitmap",
    3: "icon",
    4: "menu",
    5: "dialog",
    6: "string",
    9: "accelerator",
    10: "rcdata",
    12: "group_cursor",
    14: "group_icon",
    16: "version",
    24: "manifest",
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def safe(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value).strip("_") or "unnamed"


def entry_name(entry: object) -> str:
    name = getattr(entry, "name", None)
    return str(name) if name is not None else str(entry.struct.Id)


def extension(type_id: int | None, data: bytes) -> str:
    if data.startswith(b"BM"):
        return ".bmp"
    if data.startswith(b"Creative Voice File"):
        return ".voc"
    if data.startswith(b"RIFF"):
        return ".wav"
    if data.startswith(b"MThd"):
        return ".mid"
    if type_id == 3:
        return ".icon.bin"
    if type_id == 5:
        return ".dialog.bin"
    if type_id == 14:
        return ".group-icon.bin"
    if type_id == 16:
        return ".version.bin"
    return ".bin"


def version_strings(pe: pefile.PE) -> dict[str, str]:
    result: dict[str, str] = {}
    for outer in getattr(pe, "FileInfo", []):
        for block in outer:
            for table in getattr(block, "StringTable", []):
                for key, value in table.entries.items():
                    result[key.decode(errors="replace")] = value.decode(errors="replace")
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    image = args.executable.read_bytes()
    pe = pefile.PE(data=image, fast_load=False)
    pe.parse_data_directories()
    args.output.mkdir(parents=True, exist_ok=True)

    resources: list[dict[str, object]] = []
    for type_entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        type_text = entry_name(type_entry)
        type_id = None if type_entry.name is not None else int(type_entry.struct.Id)
        type_label = STANDARD_TYPES.get(type_id, f"custom_{type_text}")
        type_dir = args.output / f"type_{safe(type_text)}_{safe(type_label)}"
        type_dir.mkdir(parents=True, exist_ok=True)

        for name_entry in type_entry.directory.entries:
            name_text = entry_name(name_entry)
            for language_entry in name_entry.directory.entries:
                leaf = language_entry.data.struct
                data = pe.get_data(leaf.OffsetToData, leaf.Size)
                suffix = extension(type_id, data)
                filename = f"{safe(name_text)}_lang{language_entry.struct.Id}{suffix}"
                destination = type_dir / filename
                destination.write_bytes(data)
                resources.append(
                    {
                        "type": type_text,
                        "typeId": type_id,
                        "typeLabel": type_label,
                        "name": name_text,
                        "language": int(language_entry.struct.Id),
                        "rva": int(leaf.OffsetToData),
                        "fileOffset": int(pe.get_offset_from_rva(leaf.OffsetToData)),
                        "size": len(data),
                        "sha256": sha256(data),
                        "magic": data[:16].hex().upper(),
                        "path": destination.relative_to(args.output).as_posix(),
                    }
                )

    imports = []
    for descriptor in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        imports.append(
            {
                "dll": descriptor.dll.decode(errors="replace"),
                "symbols": [
                    item.name.decode(errors="replace") if item.name else f"ordinal:{item.ordinal}"
                    for item in descriptor.imports
                ],
            }
        )

    sections = []
    for section in pe.sections:
        sections.append(
            {
                "name": section.Name.rstrip(b"\0").decode(errors="replace"),
                "rva": int(section.VirtualAddress),
                "virtualSize": int(section.Misc_VirtualSize),
                "fileOffset": int(section.PointerToRawData),
                "rawSize": int(section.SizeOfRawData),
                "sha256": sha256(section.get_data()),
            }
        )

    manifest = {
        "source": {
            "path": str(args.executable),
            "size": len(image),
            "sha256": sha256(image),
        },
        "pe": {
            "machine": int(pe.FILE_HEADER.Machine),
            "timestamp": int(pe.FILE_HEADER.TimeDateStamp),
            "imageBase": int(pe.OPTIONAL_HEADER.ImageBase),
            "entryPointRva": int(pe.OPTIONAL_HEADER.AddressOfEntryPoint),
            "subsystem": int(pe.OPTIONAL_HEADER.Subsystem),
            "versionStrings": version_strings(pe),
            "sections": sections,
            "imports": imports,
        },
        "resourceCount": len(resources),
        "resources": resources,
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    by_type: dict[str, tuple[int, int]] = {}
    for item in resources:
        key = f"{item['type']} ({item['typeLabel']})"
        count, total = by_type.get(key, (0, 0))
        by_type[key] = (count + 1, total + int(item["size"]))
    print(f"source_sha256={manifest['source']['sha256']}")
    print(f"resources={len(resources)}")
    for key, (count, total) in by_type.items():
        print(f"{key}: count={count} bytes={total}")


if __name__ == "__main__":
    main()

