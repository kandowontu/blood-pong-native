#!/usr/bin/env python3
"""Create a reproducible, byte-complete static audit of a 32-bit PE image.

This tool treats the executable as inert data.  It emits a linear CODE-section
listing (including undecodable bytes), a candidate-function index, and a JSON
machine-readable ledger.  Function boundaries in a stripped Borland binary are
necessarily evidence-ranked rather than asserted as debug-symbol truth.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from collections import defaultdict
from pathlib import Path

import capstone
import pefile
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_REG_INVALID


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def section_name(section: object) -> str:
    return section.Name.rstrip(b"\0").decode(errors="replace")


def printable_strings(image: bytes, image_base: int, pe: pefile.PE) -> dict[int, str]:
    strings: dict[int, str] = {}
    pattern = re.compile(rb"[\x20-\x7e]{4,}\x00")
    for section in pe.sections:
        raw = section.get_data()
        for match in pattern.finditer(raw):
            address = image_base + section.VirtualAddress + match.start()
            strings[address] = match.group()[:-1].decode("ascii", errors="replace")
    return strings


def import_symbols(pe: pefile.PE) -> dict[int, str]:
    symbols: dict[int, str] = {}
    for descriptor in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        dll = descriptor.dll.decode(errors="replace")
        for item in descriptor.imports:
            name = item.name.decode(errors="replace") if item.name else f"ordinal_{item.ordinal}"
            symbols[int(item.address)] = f"{dll}!{name}"
    return symbols


def decode_section(code: bytes, base: int, forced_starts: set[int] | None = None) -> list[dict[str, object]]:
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    decoded: list[dict[str, object]] = []
    offset = 0
    while offset < len(code):
        insns = list(md.disasm(code[offset : offset + 15], base + offset, count=1))
        crosses_boundary = False
        if insns and forced_starts:
            end = base + offset + insns[0].size
            crosses_boundary = any(base + offset < start < end for start in forced_starts)
        if not insns or crosses_boundary:
            decoded.append(
                {
                    "address": base + offset,
                    "size": 1,
                    "bytes": code[offset : offset + 1],
                    "mnemonic": "db",
                    "op_str": f"0x{code[offset]:02X}",
                    "instruction": None,
                }
            )
            offset += 1
            continue
        insn = insns[0]
        decoded.append(
            {
                "address": int(insn.address),
                "size": int(insn.size),
                "bytes": bytes(insn.bytes),
                "mnemonic": insn.mnemonic,
                "op_str": insn.op_str,
                "instruction": insn,
            }
        )
        offset += insn.size
    return decoded


def plausible_prologue(code: bytes, offset: int) -> bool:
    starts = (
        b"\x55\x8b\xec",  # push ebp / mov ebp,esp
        b"\x55\x89\xe5",  # alternate syntax encoding
    )
    return any(code.startswith(pattern, offset) for pattern in starts)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    image = args.executable.read_bytes()
    pe = pefile.PE(data=image, fast_load=False)
    pe.parse_data_directories()
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    code_section = next((s for s in pe.sections if section_name(s).upper() == "CODE"), None)
    if code_section is None:
        code_section = next(
            (s for s in pe.sections if s.Characteristics & 0x20000000),
            None,
        )
    if code_section is None:
        raise SystemExit("No executable section found")

    code = code_section.get_data()
    code_base = image_base + int(code_section.VirtualAddress)
    code_end = code_base + len(code)
    image_end = image_base + int(pe.OPTIONAL_HEADER.SizeOfImage)
    decoded = decode_section(code, code_base)
    imports = import_symbols(pe)
    strings = printable_strings(image, image_base, pe)

    reasons: dict[int, set[str]] = defaultdict(set)
    callers: dict[int, set[int]] = defaultdict(set)
    outgoing: dict[int, set[int]] = defaultdict(set)
    string_refs: dict[int, set[int]] = defaultdict(set)
    import_refs: dict[int, set[str]] = defaultdict(set)
    data_refs: dict[int, set[int]] = defaultdict(set)
    instruction_counts: dict[int, int] = defaultdict(int)
    undecoded_counts: dict[int, int] = defaultdict(int)

    entry = image_base + int(pe.OPTIONAL_HEADER.AddressOfEntryPoint)
    if code_base <= entry < code_end:
        reasons[entry].add("PE entry point")

    for symbol in getattr(pe, "DIRECTORY_ENTRY_EXPORT", []).symbols if hasattr(pe, "DIRECTORY_ENTRY_EXPORT") else []:
        address = image_base + int(symbol.address)
        if code_base <= address < code_end:
            label = symbol.name.decode(errors="replace") if symbol.name else f"ordinal {symbol.ordinal}"
            reasons[address].add(f"export {label}")

    direct_calls: list[tuple[int, int]] = []
    for row in decoded:
        insn = row["instruction"]
        if insn is None:
            continue
        if insn.mnemonic == "call" and insn.operands and insn.operands[0].type == X86_OP_IMM:
            target = int(insn.operands[0].imm) & 0xFFFFFFFF
            if code_base <= target < code_end:
                direct_calls.append((int(insn.address), target))
                reasons[target].add("direct CALL target")
                callers[target].add(int(insn.address))

    # Relocation-backed absolute code pointers commonly identify constructors,
    # callbacks, virtual methods, and exception handlers.  Requiring a PE
    # relocation avoids treating coincidental DWORDs in the huge resource
    # section as pointers.
    for block in getattr(pe, "DIRECTORY_ENTRY_BASERELOC", []):
        for relocation in block.entries:
            if int(relocation.type) != 3:  # IMAGE_REL_BASED_HIGHLOW
                continue
            location_rva = int(relocation.rva)
            value = int.from_bytes(pe.get_data(location_rva, 4), "little")
            if code_base <= value < code_end:
                owner = next(
                    (section_name(s) for s in pe.sections if s.contains_rva(location_rva)),
                    "PE image",
                )
                reasons[value].add(f"relocated code pointer in {owner}")

    # Stripped binaries can contain unreferenced routines.  Prologue evidence is
    # deliberately weaker and is kept separately in the ledger.
    for offset in range(0, len(code), 4):
        if plausible_prologue(code, offset):
            reasons[code_base + offset].add("prologue pattern")

    starts = sorted(reasons)
    start_set = set(starts)
    decoded = decode_section(code, code_base, start_set)
    owner_by_address: dict[int, int] = {}
    cursor = 0
    for row in decoded:
        address = int(row["address"])
        while cursor + 1 < len(starts) and starts[cursor + 1] <= address:
            cursor += 1
        if starts and starts[cursor] <= address:
            owner_by_address[address] = starts[cursor]

    for source, target in direct_calls:
        owner = owner_by_address.get(source)
        if owner is not None:
            outgoing[owner].add(target)

    for row in decoded:
        insn = row["instruction"]
        owner = owner_by_address.get(int(row["address"]))
        if owner is None:
            continue
        if insn is None:
            undecoded_counts[owner] += 1
            continue
        instruction_counts[owner] += 1
        for operand in insn.operands:
            if operand.type == X86_OP_IMM:
                value = int(operand.imm) & 0xFFFFFFFF
                if value in strings:
                    string_refs[owner].add(value)
                elif image_base <= value < image_end and not (code_base <= value < code_end):
                    data_refs[owner].add(value)
            elif operand.type == X86_OP_MEM:
                memory = operand.mem
                if memory.base == X86_REG_INVALID and memory.index == X86_REG_INVALID:
                    address = int(memory.disp) & 0xFFFFFFFF
                    if address in imports:
                        import_refs[owner].add(imports[address])
                    elif image_base <= address < image_end and not (code_base <= address < code_end):
                        data_refs[owner].add(address)

    args.output.mkdir(parents=True, exist_ok=True)
    listing_path = args.output / "LISTING.asm"
    with listing_path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(f"; Source: {args.executable}\n")
        handle.write(f"; Source SHA-256: {sha256(image)}\n")
        handle.write(f"; CODE VA: 0x{code_base:08X}-0x{code_end:08X}\n")
        handle.write(f"; CODE SHA-256: {sha256(code)}\n")
        handle.write("; Linear byte-complete listing; data embedded in CODE may decode as instructions.\n\n")
        for row in decoded:
            address = int(row["address"])
            if address in start_set:
                why = "; ".join(sorted(reasons[address]))
                handle.write(f"\nsub_{address:08X}: ; {why}\n")
            raw = row["bytes"].hex(" ").upper()
            handle.write(
                f"{address:08X}  {raw:<29} {row['mnemonic']:<8} {row['op_str']}\n"
            )

    semantic_names = {
        0x00401094: "versus kode input and presentation",
        0x00401658: "versus portrait presentation",
        0x004017D4: "character-selection loop",
        0x00401F3C: "title/menu loop",
        0x004027A8: "registration status presentation",
        0x00402A44: "registration dialog",
        0x00402F68: "proprietary sprite stream renderer",
        0x004030E8: "custom numeric-resource loader",
        0x0040ED48: "startup registration-file validation",
        0x00413EAC: "versus-kode effect dispatcher",
        0x00415F74: "PE resource copy/load",
        0x00418C98: "registration key validation",
        0x00425B8C: "credits-roll setup",
    }

    def automated_category(start: int, called_imports: set[str], texts: list[str]) -> str:
        joined = " ".join(texts).lower()
        dlls = {item.split("!", 1)[0].upper() for item in called_imports}
        if "DDRAW.DLL" in dlls or any(word in joined for word in ("direct draw", "color key", "display mode")):
            return "graphics/display"
        if "DINPUT.DLL" in dlls or "keyboard" in joined:
            return "input"
        if "DSOUND.DLL" in dlls or "sound" in joined:
            return "audio"
        if any(word in joined for word in ("register", "mbreg.dat", "serial")):
            return "registration/file"
        if start < 0x00426500:
            return "game/engine"
        return "compiler/runtime"

    resolved_import_refs: dict[int, set[str]] = defaultdict(set)
    for start in starts:
        resolved_import_refs[start].update(import_refs[start])
        for target in outgoing[start]:
            resolved_import_refs[start].update(import_refs[target])

    function_rows: list[dict[str, object]] = []
    for index, start in enumerate(starts):
        end = starts[index + 1] if index + 1 < len(starts) else code_end
        start_offset = start - code_base
        end_offset = max(start_offset, min(len(code), end - code_base))
        evidence = sorted(reasons[start])
        confidence = "high" if any(
            item == "PE entry point" or item.startswith("export ") or item == "direct CALL target"
            for item in evidence
        ) else "medium" if any(item.startswith("relocated code pointer") for item in evidence) else "low"
        text_rows = [
            {"address": address, "text": strings[address]}
            for address in sorted(string_refs[start])
        ]
        called_imports = resolved_import_refs[start]
        function_rows.append(
            {
                "address": start,
                "label": f"sub_{start:08X}",
                "endExclusive": end,
                "size": end_offset - start_offset,
                "sha256": sha256(code[start_offset:end_offset]),
                "confidence": confidence,
                "evidence": evidence,
                "callers": sorted(callers[start]),
                "directCalls": sorted(outgoing[start]),
                "directImportReferences": sorted(import_refs[start]),
                "importReferences": sorted(called_imports),
                "strings": text_rows,
                "dataReferences": sorted(data_refs[start]),
                "instructionRows": instruction_counts[start],
                "undecodedByteRows": undecoded_counts[start],
                "category": automated_category(start, called_imports, [row["text"] for row in text_rows]),
                "semanticName": semantic_names.get(start),
                "auditStatus": "static instruction/reference audit complete",
            }
        )

    ledger = {
        "source": {
            "path": str(args.executable),
            "size": len(image),
            "sha256": sha256(image),
        },
        "code": {
            "section": section_name(code_section),
            "address": code_base,
            "endExclusive": code_end,
            "size": len(code),
            "sha256": sha256(code),
            "listingInstructionOrByteRows": len(decoded),
        },
        "methodology": {
            "high": "PE entry point, export, or direct CALL target",
            "medium": "relocation-backed absolute code pointer",
            "low": "unreferenced compiler-like prologue pattern",
            "boundaryCaveat": "Symbols were stripped; candidates and boundaries are evidence-ranked.",
            "auditScope": "Every candidate range is hashed, instruction-counted, and scanned for strings, direct calls, and absolute imported-API references. This does not assert that every candidate is a true source-level function.",
        },
        "imports": [{"address": address, "symbol": imports[address]} for address in sorted(imports)],
        "candidateFunctionCount": len(function_rows),
        "functions": function_rows,
    }
    (args.output / "audit.json").write_text(json.dumps(ledger, indent=2) + "\n", encoding="utf-8")

    counts = {level: sum(row["confidence"] == level for row in function_rows) for level in ("high", "medium", "low")}
    with (args.output / "FUNCTION_INDEX.md").open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("# Blood Pong static function index\n\n")
        handle.write(
            f"Source SHA-256: `{sha256(image)}`. CODE is fully represented in `LISTING.asm`; "
            "candidate boundaries are evidence-ranked because the original symbol table is absent.\n\n"
        )
        handle.write(
            f"Candidates: **{len(function_rows)}** ({counts['high']} high, {counts['medium']} medium, "
            f"{counts['low']} low confidence). Every range has completed instruction/reference auditing; "
            "manual semantic names are only asserted where evidence is sufficient.\n\n"
        )
        handle.write("| Address | Bytes | Confidence | Category | Semantic name | Imports | Callers | Status |\n")
        handle.write("|---:|---:|---|---|---|---:|---:|---|\n")
        for row in function_rows:
            handle.write(
                f"| `0x{row['address']:08X}` | {row['size']} | {row['confidence']} | "
                f"{row['category']} | {row['semanticName'] or ''} | {len(row['importReferences'])} | "
                f"{len(row['callers'])} | {row['auditStatus']} |\n"
            )

    print(f"source_sha256={sha256(image)}")
    print(f"code_bytes={len(code)} listing_rows={len(decoded)}")
    print(f"candidate_functions={len(function_rows)} high={counts['high']} medium={counts['medium']} low={counts['low']}")
    print(f"output={args.output}")


if __name__ == "__main__":
    main()
