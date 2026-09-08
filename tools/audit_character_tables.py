#!/usr/bin/env python3
"""Recover Blood Pong's fighter constructor and projectile-initializer tables."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

import capstone
import pefile
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG


NAMES = [
    "Fung Shwei", "Lo Than", "Jewel", "Raptor", "So Frio", "Nai Palm",
    "One Eye", "Raider", "Show Lin", "Dawg Cau", "Omoh", "Carmack",
    "Pain", "Lo Pan", "Mai Lai", "Baka",
]

RESOURCE_TYPES = [
    2017, 2006, 2005, 2014, 2016, 2010, 2011, 2015,
    2018, 2000, 2012, 2002, 2013, 2008, 2020, 2009,
]

# Recovered from the resource-loader loop at 0x0041A7D4. Keys are the original
# global sprite-pointer arrays used by the projectile constructor switch.
RESOURCE_BANKS = {
    0x00436690: (2004, 500, 1),
    0x00436764: (2022, 300, 2),
    0x0043676C: (2022, 700, 6),
    0x00436784: (2022, 1300, 6),
    0x0043679C: (2022, 160, 7),
    0x004367B8: (2022, 180, 4),
    0x004367C8: (2022, 128, 6),
    0x004367E0: (2022, 150, 2),
    0x004367E8: (2022, 1500, 4),
    0x004367F8: (2022, 1200, 7),
    0x00436814: (2022, 200, 6),
    0x0043682C: (2022, 1000, 15),
    0x00436868: (2022, 600, 3),
    0x00436874: (2022, 210, 6),
    0x0043688C: (2022, 170, 1),
    0x00436890: (2022, 400, 3),
    0x0043689C: (2022, 1550, 5),
    0x004368B0: (2022, 1600, 1),
    0x004368B4: (2022, 350, 5),
    0x004368C8: (2022, 500, 3),
    0x004368D4: (2022, 250, 4),
    0x004368E4: (2022, 260, 4),
}


def immediate(instruction: capstone.CsInsn, operand_index: int = 0) -> int | None:
    if (len(instruction.operands) > operand_index and
            instruction.operands[operand_index].type == X86_OP_IMM):
        return int(instruction.operands[operand_index].imm) & 0xFFFFFFFF
    return None


def absolute_memory(instruction: capstone.CsInsn, operand_index: int) -> int | None:
    if len(instruction.operands) <= operand_index:
        return None
    operand = instruction.operands[operand_index]
    if operand.type != X86_OP_MEM or operand.mem.base or operand.mem.index:
        return None
    return int(operand.mem.disp) & 0xFFFFFFFF


def projectile_types(pe: pefile.PE, md: capstone.Cs, base: int) -> list[dict]:
    handlers = struct.unpack("<25I", pe.get_data(0x0001ACB5, 25 * 4))
    rows = []
    for projectile_type, address in enumerate(handlers):
        visual_pointer = None
        visual_source = "none"
        callback = None
        width = None
        height = None
        last_global = None
        for instruction in md.disasm(pe.get_data(address - base, 0x180), address):
            if instruction.address >= 0x0041B8AE:
                break
            if instruction.mnemonic == "mov":
                global_address = absolute_memory(instruction, 1)
                if global_address is not None:
                    last_global = global_address
                if "[eax + 0x9c]" in instruction.op_str and last_global is not None:
                    callback_bytes = pe.get_data(last_global - base, 4)
                    if len(callback_bytes) == 4:
                        callback = struct.unpack("<I", callback_bytes)[0]
                if "[eax + 0x48]" in instruction.op_str:
                    value = immediate(instruction, 1)
                    if value is not None:
                        visual_pointer = value
                        visual_source = "embedded resource"
                    else:
                        visual_source = "owner paddle"
                if "[eax + 0x30]" in instruction.op_str:
                    width = immediate(instruction, 1) or width
                if "[eax + 0x34]" in instruction.op_str:
                    height = immediate(instruction, 1) or height
            if (instruction.mnemonic == "jmp" and
                    immediate(instruction) == 0x0041B8AE):
                break

        bank = RESOURCE_BANKS.get(visual_pointer)
        resource_type = bank[0] if bank else None
        resource_ids = list(range(bank[1], bank[1] + bank[2])) if bank else []
        rows.append({
            "projectileType": projectile_type,
            "handler": address,
            "callback": callback,
            "visualSource": visual_source,
            "visualPointer": visual_pointer,
            "resourceType": resource_type,
            "resourceIds": resource_ids,
            "width": width,
            "height": height,
        })
    return rows


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    pe = pefile.PE(str(args.executable), fast_load=False)
    base = int(pe.OPTIONAL_HEADER.ImageBase)
    table = pe.get_data(0x0000D6F6, 17 * 4)
    constructors = list(struct.unpack("<17I", table))[1:]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    rows = []
    for index, address in enumerate(constructors):
        code = pe.get_data(address - base, 0x900)
        history: list[capstone.CsInsn] = []
        moves = []
        paddle_pointer = None
        fighter_callbacks: dict[str, int] = {}
        register_globals: dict[int, int] = {}
        for instruction in md.disasm(code, address):
            if instruction.mnemonic == "mov" and "[ebx + 0x48]" in instruction.op_str:
                value = immediate(instruction, 1)
                if value is not None:
                    paddle_pointer = value
            if instruction.mnemonic == "mov":
                source = absolute_memory(instruction, 1)
                if (source is not None and len(instruction.operands) > 0 and
                        instruction.operands[0].type == X86_OP_REG):
                    register_globals[instruction.operands[0].reg] = source
                callback_field = next(
                    (field for field in (0x6C, 0x78, 0x84)
                     if f"[ebx + 0x{field:x}]" in instruction.op_str),
                    None,
                )
                callback_global = source
                if (callback_global is None and callback_field is not None and
                        len(instruction.operands) > 1 and
                        instruction.operands[1].type == X86_OP_REG):
                    callback_global = register_globals.get(instruction.operands[1].reg)
                if callback_global is not None and callback_field is not None:
                    raw_callback = pe.get_data(callback_global - base, 4)
                    if len(raw_callback) == 4:
                        fighter_callbacks[f"field0x{callback_field:02X}"] = struct.unpack(
                            "<I", raw_callback)[0]
            if instruction.mnemonic == "call" and immediate(instruction) == 0x0041AC54:
                pushes = [item for item in reversed(history[-16:]) if item.mnemonic == "push"]
                values = [immediate(item) for item in pushes[:5]]
                if len(values) == 5:
                    moves.append({
                        "projectileType": values[1],
                        "variant": values[2],
                        "delay": values[3],
                        "damage": values[4],
                    })
            history.append(instruction)
            if instruction.mnemonic == "jmp" and immediate(instruction) == 0x0040E72B:
                break
        rows.append({
            "characterIndex": index,
            "name": NAMES[index],
            "resourceType": RESOURCE_TYPES[index],
            "constructor": address,
            "paddlePointer": paddle_pointer,
            "callbacks": fighter_callbacks,
            "projectiles": moves,
        })

    type_rows = projectile_types(pe, md, base)

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "character_tables.json").write_text(json.dumps(rows, indent=2) + "\n")
    (args.output / "projectile_types.json").write_text(
        json.dumps(type_rows, indent=2) + "\n")
    with (args.output / "CHARACTER_TABLES.md").open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("# Fighter constructor table\n\n")
        handle.write(
            "Recovered from the original 17-way constructor jump table at `0x0040D6F6`. "
            "Projectile type/damage arguments are literal parameters passed to `0x0041AC54`; "
            "blank entries are dynamically configured or use a different initializer.\n\n"
        )
        handle.write(
            "The three callback columns are the exact code pointers assigned to fighter "
            "fields `+0x6C`, `+0x78`, and `+0x84`; neutral field names avoid assigning "
            "semantics that are still under translation.\n\n"
        )
        handle.write(
            "| # | Fighter | Resource type | Constructor | Paddle pointer | Callback +6C | Callback +78 | Callback +84 | Projectile type:damage |\n"
        )
        handle.write("|---:|---|---:|---:|---:|---:|---:|---:|---|\n")
        for row in rows:
            moves = ", ".join(
                f"{move['projectileType']}:{move['damage']}" for move in row["projectiles"]
            )
            handle.write(
                f"| {row['characterIndex'] + 1} | {row['name']} | {row['resourceType']} | "
                f"`0x{row['constructor']:08X}` | `0x{row['paddlePointer']:08X}` | "
                f"`0x{row['callbacks'].get('field0x6C', 0):08X}` | "
                f"`0x{row['callbacks'].get('field0x78', 0):08X}` | "
                f"`0x{row['callbacks'].get('field0x84', 0):08X}` | {moves} |\n"
            )
    with (args.output / "PROJECTILE_TYPES.md").open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("# Projectile type switch\n\n")
        handle.write(
            "Recovered from the 25-way switch at `0x0041ACB5`. Resource banks are "
            "cross-referenced against the exact loader loop at `0x0041A7D4`. "
            "`owner paddle` means the handler deliberately copies the firing "
            "fighter's current sprite pointer.\n\n"
        )
        handle.write("| Type | Handler | Callback | Visual source | Resource bank | Size |\n")
        handle.write("|---:|---:|---:|---|---|---:|\n")
        for row in type_rows:
            callback = f"`0x{row['callback']:08X}`" if row["callback"] else "—"
            if row["resourceIds"]:
                ids = row["resourceIds"]
                id_text = str(ids[0]) if len(ids) == 1 else f"{ids[0]}–{ids[-1]}"
                bank = f"type {row['resourceType']}, IDs {id_text}"
            else:
                bank = "—"
            size = (f"{row['width']}×{row['height']}"
                    if row["width"] is not None and row["height"] is not None else "—")
            handle.write(
                f"| {row['projectileType']} | `0x{row['handler']:08X}` | {callback} | "
                f"{row['visualSource']} | {bank} | {size} |\n"
            )
    print(
        f"fighters={len(rows)} "
        f"projectile_initializers={sum(len(row['projectiles']) for row in rows)} "
        f"projectile_types={len(type_rows)}"
    )


if __name__ == "__main__":
    main()
