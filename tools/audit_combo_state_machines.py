#!/usr/bin/env python3
"""Recover character combo recipes by emulating the inert input recognizers.

The supplied executable is never launched.  Its mapped bytes are interpreted in
an isolated Unicorn x86 VM with only a synthetic fighter/input structure. Calls
that would activate a component or fatality are intercepted and recorded.
"""

from __future__ import annotations

import argparse
import json
from collections import deque
from pathlib import Path

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP


FIGHTERS = [
    ("Fung Shwei", 0x4082A4), ("Lo Than", 0x408538),
    ("Jewel", 0x4087DC), ("Raptor", 0x408A7C),
    ("So Frio", 0x408D10), ("Nai Palm", 0x408FC4),
    ("One Eye", 0x409278), ("Raider", 0x409530),
    ("Show Lin", 0x4097E8), ("Dawg Cau", 0x40A5E0),
    ("Omoh", 0x409D48), ("Carmack", 0x409F64),
    ("Pain", 0x40A188), ("Lo Pan", 0x40A3B8),
    ("Mai Lai", 0x409A9C), ("Baka", 0x40A848),
]

COMPONENT_ACTIVATOR = 0x4081E8
FATALITY_ACTIVATOR = 0x408248
OBJECT = 0x100000
OPPONENT = 0x102000
INPUT = 0x104000
STACK = 0x200000
STOP = 0x300000
PAGE = 0x1000

BUTTONS = {
    "A1": (0x133, 0x11),
    "A2": (0x134, 0x12),
    "A3": (0x135, 0x13),
    "TURBO": (0x130, 0x14),
    "SUPER": (0x136, 0x15),
}


def put32(vm: Uc, address: int, value: int) -> None:
    vm.mem_write(address, int(value & 0xFFFFFFFF).to_bytes(4, "little"))


def get32(vm: Uc, address: int) -> int:
    return int.from_bytes(vm.mem_read(address, 4), "little")


class RecognizerVm:
    def __init__(self, executable: Path) -> None:
        pe = pefile.PE(str(executable), fast_load=True)
        image = pe.get_memory_mapped_image()
        image_base = pe.OPTIONAL_HEADER.ImageBase
        image_size = (max(len(image), pe.OPTIONAL_HEADER.SizeOfImage) + PAGE - 1) & -PAGE
        self.vm = Uc(UC_ARCH_X86, UC_MODE_32)
        self.vm.mem_map(image_base, image_size)
        self.vm.mem_write(image_base, image)
        self.vm.mem_map(OBJECT, PAGE * 6)
        self.vm.mem_map(STACK, PAGE * 4)
        self.vm.mem_map(STOP, PAGE)
        self.actions: list[dict[str, int | str]] = []
        self.vm.hook_add(UC_HOOK_CODE, self._hook)

    def _return_from_intercepted_call(self) -> None:
        esp = self.vm.reg_read(UC_X86_REG_ESP)
        return_address = get32(self.vm, esp)
        self.vm.reg_write(UC_X86_REG_ESP, esp + 4)
        self.vm.reg_write(UC_X86_REG_EIP, return_address)

    def _hook(self, _vm: Uc, address: int, _size: int, _data: object) -> None:
        if address == COMPONENT_ACTIVATOR:
            esp = self.vm.reg_read(UC_X86_REG_ESP)
            self.actions.append({"kind": "component", "index": get32(self.vm, esp + 8)})
            self._return_from_intercepted_call()
        elif address == FATALITY_ACTIVATOR:
            self.actions.append({"kind": "fatality"})
            self._return_from_intercepted_call()

    def transition(self, function: int, state: int, button: str) -> tuple[int, list[dict[str, int | str]]]:
        self.vm.mem_write(OBJECT, bytes(PAGE * 4))
        self.vm.mem_write(INPUT, b"\0" * 16)
        put32(self.vm, OBJECT + 0xD8, INPUT)
        put32(self.vm, OBJECT + 0xE0, state)
        put32(self.vm, OBJECT + 0xE4, 1)
        put32(self.vm, OBJECT + 0xFC, 186)
        put32(self.vm, OBJECT + 0x100, 58)
        put32(self.vm, OBJECT + 0x104, 154)
        put32(self.vm, OBJECT + 0x13C, 1)
        put32(self.vm, OBJECT + 0x140, OPPONENT)
        put32(self.vm, OPPONENT + 0xFC, 186)
        put32(self.vm, OPPONENT + 0x13C, 2)
        for _name, (offset, code) in BUTTONS.items():
            self.vm.mem_write(OBJECT + offset, bytes([code]))
        self.vm.mem_write(INPUT, bytes([BUTTONS[button][1]]))

        stack_top = STACK + PAGE * 3
        put32(self.vm, stack_top, STOP)
        put32(self.vm, stack_top + 4, OBJECT)
        self.vm.reg_write(UC_X86_REG_ESP, stack_top)
        self.actions = []
        self.vm.emu_start(function, STOP, count=10000)
        return get32(self.vm, OBJECT + 0xE0), list(self.actions)


def shortest_recipes(machine: RecognizerVm, function: int) -> list[dict[str, object]]:
    queue = deque([(0, [])])
    visited = {0}
    recipes: list[dict[str, object]] = []
    seen_actions: set[tuple[str, int | None]] = set()
    while queue:
        state, sequence = queue.popleft()
        if len(sequence) >= 12:
            continue
        for button in BUTTONS:
            next_state, actions = machine.transition(function, state, button)
            next_sequence = sequence + [button]
            for action in actions:
                key = (str(action["kind"]), int(action["index"]) if "index" in action else None)
                if key not in seen_actions:
                    seen_actions.add(key)
                    recipes.append({"sequence": next_sequence, **action})
            if next_state not in visited:
                visited.add(next_state)
                queue.append((next_state, next_sequence))
    return recipes


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    machine = RecognizerVm(args.executable)
    result = []
    for name, function in FIGHTERS:
        result.append({
            "fighter": name,
            "recognizer": function,
            "recipes": shortest_recipes(machine, function),
        })
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    markdown = [
        "# Character combo recipes", "",
        "Recovered by deterministic emulation of the 16 original input-state recognizers. "
        "Each input must arrive within the recognizer's 60-update window.", "",
        "| Fighter | Result | Exact input sequence |", "|---|---|---|",
    ]
    for item in result:
        for recipe in item["recipes"]:
            outcome = "Fatality" if recipe["kind"] == "fatality" else f"Component {recipe['index']}"
            sequence = " → ".join(str(button) for button in recipe["sequence"])
            markdown.append(f"| {item['fighter']} | {outcome} | `{sequence}` |")
    markdown_path = args.output.with_name("COMBO_RECIPES.md")
    markdown_path.write_text("\n".join(markdown) + "\n", encoding="utf-8")
    print(
        f"fighters={len(result)} recipes={sum(len(item['recipes']) for item in result)} "
        f"output={args.output} markdown={markdown_path}"
    )


if __name__ == "__main__":
    main()
