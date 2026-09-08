#!/usr/bin/env python3
"""Create the standalone Windows executable and archival release bundle."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import zipfile
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def archive_entry(archive: zipfile.ZipFile, name: str, data: bytes) -> None:
    info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    archive.writestr(info, data, compresslevel=9)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--version", default="1.0.0")
    parser.add_argument("--output", type=Path, default=Path("dist"))
    args = parser.parse_args()

    executable = args.executable.resolve()
    if not executable.is_file():
        raise SystemExit(f"release executable not found: {executable}")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)

    stem = f"Blood-Pong-v{args.version}-Windows-x64"
    standalone = output / f"{stem}.exe"
    bundle = output / f"{stem}.zip"
    checksums = output / "SHA256SUMS.txt"
    shutil.copy2(executable, standalone)

    root = Path(__file__).resolve().parent.parent
    with zipfile.ZipFile(bundle, "w") as archive:
        archive_entry(archive, "Blood Pong.exe", standalone.read_bytes())
        archive_entry(archive, "README.md", (root / "README.md").read_bytes())
        archive_entry(archive, "CREDITS.md", (root / "CREDITS.md").read_bytes())
        archive_entry(
            archive, "RELEASE_NOTES.md", (root / "RELEASE_NOTES.md").read_bytes()
        )

    rows = [
        f"{sha256(standalone)}  {standalone.name}",
        f"{sha256(bundle)}  {bundle.name}",
    ]
    checksums.write_text("\n".join(rows) + "\n", encoding="ascii", newline="\n")
    print(f"standalone={standalone}")
    print(f"bundle={bundle}")
    print(f"checksums={checksums}")
    for row in rows:
        print(row)


if __name__ == "__main__":
    main()
