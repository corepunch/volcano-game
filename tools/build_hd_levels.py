#!/usr/bin/env python3
"""Compile both custom missions with their authored 2x Retina backgrounds."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MISSIONS = ("emerald-falls", "forest-mayday")


def png_size(path):
    with path.open("rb") as image:
        header = image.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"not a PNG image: {path}")
    return struct.unpack(">II", header[16:24])


def build(output):
    plans = []
    for slug in MISSIONS:
        source = ROOT / "levels" / (slug + ".level.json")
        background = ROOT / "levels" / (slug + "@2x.png")
        original = ROOT / "levels" / (slug + ".png")
        if json.loads(source.read_text())["background"] != "@" + original.name:
            raise ValueError(f"unexpected background reference in {source}")
        expected = tuple(dimension * 2 for dimension in png_size(original))
        if png_size(background) != expected:
            raise ValueError(f"{background.name} must be exactly {expected[0]}x{expected[1]}")
        plans.append((slug, source, background, original.name, expected))
    output.mkdir(parents=True, exist_ok=True)
    for slug, source, background, filename, dimensions in plans:
        subprocess.run([sys.executable, str(ROOT / "tools/level_source.py"),
                        "compile", str(source), str(output / (slug + ".lvl"))], check=True)
        # The binary's existing background filename stays intact. Only the HD
        # output folder receives the new image; gameplay data remains identical.
        shutil.copy2(background, output / filename)
        print(f"HD level: {slug}, {dimensions[0]}x{dimensions[1]} background; unchanged world coordinates.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/hd-levels")
    args = parser.parse_args()
    try:
        build(args.output.resolve())
    except (OSError, ValueError, KeyError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
