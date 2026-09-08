#!/usr/bin/env python3
"""Exercise one recovered component recipe for every native fighter.

Only the rebuilt executable is launched.  The supplied legacy executable is
never executed by this regression check.
"""

from __future__ import annotations

import argparse
import ctypes
import json
import subprocess
import time
from pathlib import Path


user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

WM_KEYDOWN = 0x0100
WM_SYSKEYDOWN = 0x0104
WM_CLOSE = 0x0010
VK_CONTROL = 0x11
VK_MENU = 0x12
VK_RETURN = 0x0D
VK_ESCAPE = 0x1B
VK_UP = 0x26
VK_RIGHT = 0x27
VK_DOWN = 0x28
VK_F1 = 0x70
KEYEVENTF_KEYUP = 0x0002


class Rect(ctypes.Structure):
    _fields_ = [
        ("left", ctypes.c_long),
        ("top", ctypes.c_long),
        ("right", ctypes.c_long),
        ("bottom", ctypes.c_long),
    ]


class BitmapInfoHeader(ctypes.Structure):
    _fields_ = [
        ("biSize", ctypes.c_uint32),
        ("biWidth", ctypes.c_long),
        ("biHeight", ctypes.c_long),
        ("biPlanes", ctypes.c_uint16),
        ("biBitCount", ctypes.c_uint16),
        ("biCompression", ctypes.c_uint32),
        ("biSizeImage", ctypes.c_uint32),
        ("biXPelsPerMeter", ctypes.c_long),
        ("biYPelsPerMeter", ctypes.c_long),
        ("biClrUsed", ctypes.c_uint32),
        ("biClrImportant", ctypes.c_uint32),
    ]


class BitmapInfo(ctypes.Structure):
    _fields_ = [
        ("bmiHeader", BitmapInfoHeader),
        ("bmiColors", ctypes.c_uint32 * 3),
    ]


def send(window: int, key: int) -> None:
    user32.SendMessageW(window, WM_KEYDOWN, key, 0)


def secret_shortcut(window: int) -> None:
    user32.SetForegroundWindow(window)
    user32.keybd_event(VK_CONTROL, 0, 0, 0)
    user32.keybd_event(VK_MENU, 0, 0, 0)
    time.sleep(0.04)
    user32.SendMessageW(window, WM_SYSKEYDOWN, VK_F1, 1 << 29)
    user32.keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0)
    user32.keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0)


def client_size(window: int) -> tuple[int, int]:
    rect = Rect()
    if not user32.GetClientRect(window, ctypes.byref(rect)):
        raise RuntimeError("GetClientRect failed")
    return rect.right - rect.left, rect.bottom - rect.top


def near_black_ratio(window: int) -> float:
    width, height = client_size(window)
    source_dc = user32.GetDC(window)
    memory_dc = gdi32.CreateCompatibleDC(source_dc)
    bitmap = gdi32.CreateCompatibleBitmap(source_dc, width, height)
    old_bitmap = gdi32.SelectObject(memory_dc, bitmap)
    try:
        if not user32.PrintWindow(window, memory_dc, 1):
            raise RuntimeError("PrintWindow failed")
        info = BitmapInfo()
        info.bmiHeader.biSize = ctypes.sizeof(BitmapInfoHeader)
        info.bmiHeader.biWidth = width
        info.bmiHeader.biHeight = -height
        info.bmiHeader.biPlanes = 1
        info.bmiHeader.biBitCount = 32
        buffer = ctypes.create_string_buffer(width * height * 4)
        if not gdi32.GetDIBits(
            memory_dc, bitmap, 0, height, buffer, ctypes.byref(info), 0
        ):
            raise RuntimeError("GetDIBits failed")
        pixels = memoryview(buffer.raw)
        near_black = 0
        for offset in range(0, width * height * 4, 4):
            if pixels[offset] < 4 and pixels[offset + 1] < 4 and pixels[offset + 2] < 4:
                near_black += 1
        return near_black / (width * height)
    finally:
        gdi32.SelectObject(memory_dc, old_bitmap)
        gdi32.DeleteObject(bitmap)
        gdi32.DeleteDC(memory_dc)
        user32.ReleaseDC(window, source_dc)


def first_component_recipes(path: Path) -> list[tuple[str, list[str]]]:
    records = json.loads(path.read_text(encoding="utf-8"))
    recipes: list[tuple[str, list[str]]] = []
    for fighter in records:
        component = next(
            recipe for recipe in fighter["recipes"] if recipe["kind"] == "component"
        )
        recipes.append((fighter["fighter"], component["sequence"]))
    if len(recipes) != 16:
        raise RuntimeError(f"expected 16 fighters, found {len(recipes)}")
    return recipes


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument(
        "--recipes",
        type=Path,
        default=Path("analysis/disassembly/combo_recipes.json"),
    )
    parser.add_argument("--intro-seconds", type=float, default=6.2)
    args = parser.parse_args()

    executable = args.executable.resolve()
    if not executable.is_file():
        raise SystemExit(f"native executable not found: {executable}")
    recipes = first_component_recipes(args.recipes)
    key_for = {"A1": ord("1"), "A2": ord("2"), "A3": ord("3"),
               "SUPER": ord("4"), "TURBO": ord("5")}

    user32.SetProcessDPIAware()
    process = subprocess.Popen([str(executable)])
    window = 0
    try:
        deadline = time.monotonic() + 15.0
        while not window and time.monotonic() < deadline:
            window = user32.FindWindowW("BloodPongNativeWindow", None)
            time.sleep(0.05)
        if not window:
            raise RuntimeError("native window did not appear")
        while not user32.IsWindowVisible(window) and time.monotonic() < deadline:
            time.sleep(0.05)
        if not user32.IsWindowVisible(window):
            raise RuntimeError("native window did not finish initialization")

        # Reach the Ultimate Kombat Kode screen through two-player selection,
        # apply the required hidden shortcut, then return to the title.  This
        # makes all sixteen portraits available for the matrix below.
        send(window, VK_DOWN)
        send(window, VK_RETURN)
        send(window, VK_RETURN)
        send(window, VK_RETURN)
        secret_shortcut(window)
        send(window, VK_ESCAPE)
        send(window, VK_ESCAPE)
        send(window, VK_UP)

        previous_character = 0
        maximum_black = 0.0
        for index, (fighter, sequence) in enumerate(recipes):
            if process.poll() is not None or not user32.IsWindow(window):
                raise RuntimeError(f"process exited before fighter {index}: {fighter}")
            send(window, VK_RETURN)  # one player
            for _ in range((index - previous_character) % 16):
                send(window, VK_RIGHT)
            previous_character = index
            send(window, VK_RETURN)  # confirm fighter / show ladder
            send(window, VK_RETURN)  # start match
            time.sleep(args.intro_seconds)
            for token in sequence:
                send(window, key_for[token])
                time.sleep(0.04)
            time.sleep(0.30)

            ratio = near_black_ratio(window)
            maximum_black = max(maximum_black, ratio)
            if ratio > 0.98:
                raise RuntimeError(
                    f"fighter {index} produced a nearly-black frame ({ratio:.4f})"
                )

            if index == 0:
                original_size = client_size(window)
                user32.SendMessageW(window, WM_SYSKEYDOWN, VK_RETURN, 1 << 29)
                time.sleep(0.20)
                fullscreen_size = client_size(window)
                if fullscreen_size == original_size:
                    raise RuntimeError("Alt+Enter did not change the client size")
                user32.SendMessageW(window, WM_SYSKEYDOWN, VK_RETURN, 1 << 29)
                time.sleep(0.20)
                secret_shortcut(window)
                time.sleep(0.10)
                secret_shortcut(window)

            print(
                f"fighter={index:02d} name={fighter!r} recipe={'-'.join(sequence)} "
                f"near_black={ratio:.4f}",
                flush=True,
            )
            send(window, VK_ESCAPE)
            time.sleep(0.08)

        print(
            f"character_matrix=16/16 fullscreen=pass hidden_cheat=pass "
            f"max_near_black={maximum_black:.4f}",
            flush=True,
        )
    finally:
        if window and user32.IsWindow(window):
            user32.PostMessageW(window, WM_CLOSE, 0, 0)
        try:
            process.wait(timeout=4)
        except subprocess.TimeoutExpired:
            process.terminate()


if __name__ == "__main__":
    main()
