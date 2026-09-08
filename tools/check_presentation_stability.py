#!/usr/bin/env python3
"""Sample the live client surface and fail if a presented frame is blank.

This exercises both windowed and Alt+Enter fullscreen presentation. Unlike the
single-frame visual capture helper, pixels are copied from the desktop DC so an
intermediate live clear cannot be hidden by PrintWindow repaint behavior.
"""

from __future__ import annotations

import argparse
import ctypes
import subprocess
import time
from pathlib import Path


user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32


class Point(ctypes.Structure):
    _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]


class Rect(ctypes.Structure):
    _fields_ = [
        ("left", ctypes.c_long), ("top", ctypes.c_long),
        ("right", ctypes.c_long), ("bottom", ctypes.c_long),
    ]


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


def content_rect(window: int) -> tuple[int, int, int, int]:
    client = Rect()
    user32.GetClientRect(window, ctypes.byref(client))
    origin = Point()
    user32.ClientToScreen(window, ctypes.byref(origin))
    width = client.right
    height = client.bottom
    scaled_width = width
    scaled_height = scaled_width * 480 // 640
    if scaled_height > height:
        scaled_height = height
        scaled_width = scaled_height * 640 // 480
    # Sample inside the source art (48,24)-(592,456), omitting the intentional
    # black border around the original 544x432 presentation.
    left = origin.x + (width - scaled_width) // 2 + scaled_width * 48 // 640
    top = origin.y + (height - scaled_height) // 2 + scaled_height * 24 // 480
    sample_width = max(1, scaled_width * 544 // 640)
    sample_height = max(1, scaled_height * 432 // 480)
    return left, top, sample_width, sample_height


def pure_black_ratio(window: int) -> float:
    left, top, width, height = content_rect(window)
    screen_dc = user32.GetDC(0)
    memory_dc = gdi32.CreateCompatibleDC(screen_dc)
    bitmap = gdi32.CreateCompatibleBitmap(screen_dc, width, height)
    old_bitmap = gdi32.SelectObject(memory_dc, bitmap)
    gdi32.BitBlt(memory_dc, 0, 0, width, height, screen_dc, left, top, 0x00CC0020)
    info = BitmapInfo()
    info.bmiHeader.biSize = ctypes.sizeof(BitmapInfoHeader)
    info.bmiHeader.biWidth = width
    info.bmiHeader.biHeight = -height
    info.bmiHeader.biPlanes = 1
    info.bmiHeader.biBitCount = 32
    buffer = ctypes.create_string_buffer(width * height * 4)
    gdi32.GetDIBits(memory_dc, bitmap, 0, height, buffer, ctypes.byref(info), 0)
    pixels = memoryview(buffer.raw).cast("I")
    ratio = sum((pixel & 0x00FFFFFF) == 0 for pixel in pixels) / len(pixels)
    gdi32.SelectObject(memory_dc, old_bitmap)
    gdi32.DeleteObject(bitmap)
    gdi32.DeleteDC(memory_dc)
    user32.ReleaseDC(0, screen_dc)
    return ratio


def sample_phase(window: int, seconds: float) -> tuple[int, float]:
    samples = 0
    maximum = 0.0
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        maximum = max(maximum, pure_black_ratio(window))
        samples += 1
        time.sleep(0.008)
    return samples, maximum


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--seconds", type=float, default=2.0)
    args = parser.parse_args()

    user32.SetProcessDPIAware()
    process = subprocess.Popen([str(args.executable)])
    window = 0
    deadline = time.monotonic() + 12
    while not window and time.monotonic() < deadline:
        window = user32.FindWindowW("BloodPongNativeWindow", None)
        time.sleep(0.05)
    while window and not user32.IsWindowVisible(window) and time.monotonic() < deadline:
        time.sleep(0.05)
    if not window or not user32.IsWindowVisible(window):
        process.terminate()
        raise SystemExit("Native window did not finish initialization")

    user32.SetForegroundWindow(window)
    user32.SendMessageW(window, 0x0100, 0x0D, 0)  # One player
    user32.SendMessageW(window, 0x0100, 0x0D, 0)  # Accept fighter
    time.sleep(0.5)
    windowed = sample_phase(window, args.seconds)

    user32.SendMessageW(window, 0x0104, 0x0D, 1 << 29)  # Alt+Enter
    time.sleep(0.35)
    fullscreen = sample_phase(window, args.seconds)

    user32.PostMessageW(window, 0x0010, 0, 0)
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.terminate()

    maximum = max(windowed[1], fullscreen[1])
    print(
        f"windowed_samples={windowed[0]} windowed_max_black={windowed[1]:.4f} "
        f"fullscreen_samples={fullscreen[0]} fullscreen_max_black={fullscreen[1]:.4f}"
    )
    if maximum > 0.98:
        raise SystemExit("Detected a nearly all-black presented content frame")


if __name__ == "__main__":
    main()
