#!/usr/bin/env python3
"""Launch the native build, navigate to Credits, and capture its client area."""

from __future__ import annotations

import argparse
import ctypes
import subprocess
import time
from pathlib import Path

from PIL import Image


user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32


class Rect(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


class BitmapInfoHeader(ctypes.Structure):
    _fields_ = [
        ("biSize", ctypes.c_uint32), ("biWidth", ctypes.c_long),
        ("biHeight", ctypes.c_long), ("biPlanes", ctypes.c_uint16),
        ("biBitCount", ctypes.c_uint16), ("biCompression", ctypes.c_uint32),
        ("biSizeImage", ctypes.c_uint32), ("biXPelsPerMeter", ctypes.c_long),
        ("biYPelsPerMeter", ctypes.c_long), ("biClrUsed", ctypes.c_uint32),
        ("biClrImportant", ctypes.c_uint32),
    ]


class BitmapInfo(ctypes.Structure):
    _fields_ = [("bmiHeader", BitmapInfoHeader), ("bmiColors", ctypes.c_uint32 * 3)]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--screen",
        choices=("title", "credits", "select", "ladder", "kode", "unlock", "fight", "match", "kode-match", "projectile", "cheat", "configuration"),
        default="credits",
    )
    parser.add_argument(
        "--versus-kode",
        default="202202",
        help="six digits to enter when --screen kode-match is selected",
    )
    args = parser.parse_args()

    user32.SetProcessDPIAware()
    process = subprocess.Popen([str(args.executable)])
    window = 0
    deadline = time.monotonic() + 8
    while not window and time.monotonic() < deadline:
        window = user32.FindWindowW("BloodPongNativeWindow", None)
        time.sleep(0.05)
    if not window:
        process.terminate()
        raise SystemExit("Native window did not appear")
    # The HWND is created before the embedded sprite/audio banks finish
    # decoding. Wait until WinMain has shown it and entered normal dispatch so
    # navigation and timing start from the same point as an interactive run.
    ready_deadline = time.monotonic() + 12
    while not user32.IsWindowVisible(window) and time.monotonic() < ready_deadline:
        time.sleep(0.05)
    if not user32.IsWindowVisible(window):
        process.terminate()
        raise SystemExit("Native window did not finish initialization")

    if args.screen == "credits":
        # Title selection begins on One Player; move to KREDITS and activate it.
        for _ in range(3):
            user32.SendMessageW(window, 0x0100, 0x28, 0)  # WM_KEYDOWN / VK_DOWN
        user32.SendMessageW(window, 0x0100, 0x0D, 0)      # WM_KEYDOWN / VK_RETURN
    elif args.screen == "configuration":
        for _ in range(2):
            user32.SendMessageW(window, 0x0100, 0x28, 0)
        user32.SendMessageW(window, 0x0100, 0x0D, 0)
    elif args.screen == "kode-match":
        if len(args.versus_kode) != 6 or not args.versus_kode.isdigit():
            process.terminate()
            raise SystemExit("--versus-kode must contain exactly six digits")
        user32.SendMessageW(window, 0x0100, 0x28, 0)  # TWO PLAYER
        user32.SendMessageW(window, 0x0100, 0x0D, 0)
        user32.SendMessageW(window, 0x0100, 0x0D, 0)  # confirm P1
        user32.SendMessageW(window, 0x0100, 0x0D, 0)  # confirm P2
        for digit in args.versus_kode:
            user32.SendMessageW(window, 0x0100, ord(digit), 0)
        user32.SendMessageW(window, 0x0100, 0x0D, 0)
    elif args.screen in ("select", "ladder", "fight", "match", "projectile", "cheat"):
        user32.SendMessageW(window, 0x0100, 0x0D, 0)
        if args.screen in ("ladder", "fight", "match", "projectile", "cheat"):
            user32.SendMessageW(window, 0x0100, 0x0D, 0)
        if args.screen in ("fight", "match", "projectile", "cheat"):
            user32.SendMessageW(window, 0x0100, 0x0D, 0)
        if args.screen in ("projectile", "cheat"):
            user32.SetForegroundWindow(window)
            user32.keybd_event(0x11, 0, 0, 0)       # Ctrl down
            user32.keybd_event(0x12, 0, 0, 0)       # Alt down
            time.sleep(0.05)
            user32.SendMessageW(window, 0x0104, 0x70, 1 << 29)  # WM_SYSKEYDOWN / F1
            user32.keybd_event(0x12, 0, 2, 0)       # Alt up
            user32.keybd_event(0x11, 0, 2, 0)       # Ctrl up
        if args.screen == "projectile":
            # Leave the menu, wait out the exact intro, then enter Fung Shwei's
            # recovered component-one recipe: A1, A1, A2, A2, Turbo.
            user32.SendMessageW(window, 0x0100, 0x28, 0)
            user32.SendMessageW(window, 0x0100, 0x28, 0)
            user32.SendMessageW(window, 0x0100, 0x0D, 0)
            user32.SendMessageW(window, 0x0100, 0x1B, 0)
            time.sleep(6.2)
            for key in (0x31, 0x31, 0x32, 0x32, 0x35):
                user32.keybd_event(key, 0, 0, 0)
                user32.keybd_event(key, 0, 2, 0)
                time.sleep(0.04)
    elif args.screen in ("kode", "unlock"):
        user32.SendMessageW(window, 0x0100, 0x28, 0)
        user32.SendMessageW(window, 0x0100, 0x0D, 0)
        user32.SendMessageW(window, 0x0100, 0x0D, 0)
        user32.SendMessageW(window, 0x0100, 0x0D, 0)
        if args.screen == "unlock":
            user32.SetForegroundWindow(window)
            user32.keybd_event(0x11, 0, 0, 0)
            user32.keybd_event(0x12, 0, 0, 0)
            time.sleep(0.05)
            user32.SendMessageW(window, 0x0104, 0x70, 1 << 29)
            user32.keybd_event(0x12, 0, 2, 0)
            user32.keybd_event(0x11, 0, 2, 0)
    if args.screen == "fight":
        time.sleep(3.2)
    elif args.screen in ("match", "kode-match"):
        time.sleep(6.2)
    else:
        time.sleep(0.28 if args.screen == "projectile" else 0.12)
    user32.UpdateWindow(window)

    rect = Rect()
    user32.GetClientRect(window, ctypes.byref(rect))
    width, height = rect.right, rect.bottom
    source_dc = user32.GetDC(window)
    memory_dc = gdi32.CreateCompatibleDC(source_dc)
    bitmap = gdi32.CreateCompatibleBitmap(source_dc, width, height)
    old_bitmap = gdi32.SelectObject(memory_dc, bitmap)
    if not user32.PrintWindow(window, memory_dc, 1):  # PW_CLIENTONLY
        raise SystemExit("PrintWindow failed")

    info = BitmapInfo()
    info.bmiHeader.biSize = ctypes.sizeof(BitmapInfoHeader)
    info.bmiHeader.biWidth = width
    info.bmiHeader.biHeight = -height
    info.bmiHeader.biPlanes = 1
    info.bmiHeader.biBitCount = 32
    info.bmiHeader.biCompression = 0
    buffer = ctypes.create_string_buffer(width * height * 4)
    gdi32.GetDIBits(memory_dc, bitmap, 0, height, buffer, ctypes.byref(info), 0)
    image = Image.frombuffer("RGBA", (width, height), buffer, "raw", "BGRA", 0, 1).convert("RGB")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output, optimize=True)

    gdi32.SelectObject(memory_dc, old_bitmap)
    gdi32.DeleteObject(bitmap)
    gdi32.DeleteDC(memory_dc)
    user32.ReleaseDC(window, source_dc)
    user32.PostMessageW(window, 0x0010, 0, 0)  # WM_CLOSE
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.terminate()
    print(f"captured={width}x{height} output={args.output}")


if __name__ == "__main__":
    main()
