#!/usr/bin/env python3
"""Inventory statically resolvable calls to the original custom-resource loader."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import capstone
import pefile
from capstone.x86 import X86_OP_IMM


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    pe = pefile.PE(str(args.executable), fast_load=False)
    base = int(pe.OPTIONAL_HEADER.ImageBase)
    code_section = next(section for section in pe.sections if section.Name.rstrip(b"\0") == b"CODE")
    code = code_section.get_data()
    code_base = base + int(code_section.VirtualAddress)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    md.skipdata = True
    instructions = list(md.disasm(code, code_base))

    rows: list[dict[str, int | None]] = []
    for index, instruction in enumerate(instructions):
        if instruction.mnemonic != "call" or not instruction.operands:
            continue
        operand = instruction.operands[0]
        if operand.type != X86_OP_IMM or (int(operand.imm) & 0xFFFFFFFF) != 0x004030E8:
            continue
        pushes = []
        for previous in reversed(instructions[max(0, index - 14):index]):
            if previous.mnemonic == "call":
                break
            if previous.mnemonic == "push":
                value = None
                if previous.operands and previous.operands[0].type == X86_OP_IMM:
                    value = int(previous.operands[0].imm) & 0xFFFFFFFF
                pushes.append(value)
                if len(pushes) == 3:
                    break
        destination, resource_type, resource_id = (pushes + [None, None, None])[:3]
        rows.append({
            "callAddress": int(instruction.address),
            "destination": destination,
            "resourceType": resource_type,
            "resourceId": resource_id,
        })

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "resource_loads.json").write_text(json.dumps(rows, indent=2) + "\n")
    resolved = [row for row in rows if row["resourceType"] is not None and row["resourceId"] is not None]
    with (args.output / "RESOURCE_LOADS.md").open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("# Statically resolved custom-resource loads\n\n")
        handle.write(
            f"Found {len(rows)} calls to loader `0x004030E8`; {len(resolved)} have literal "
            "resource type and ID arguments recoverable from the local push sequence. Dynamic loop "
            "IDs remain represented in the full disassembly.\n\n"
        )
        handle.write("| Call address | Type | ID |\n|---:|---:|---:|\n")
        for row in resolved:
            handle.write(
                f"| `0x{row['callAddress']:08X}` | {row['resourceType']} | {row['resourceId']} |\n"
            )
    print(f"calls={len(rows)} literal_pairs={len(resolved)} output={args.output}")


if __name__ == "__main__":
    main()
