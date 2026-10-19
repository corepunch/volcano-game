#!/usr/bin/env python3
"""Create a local macOS launcher for a compiled custom level."""
import argparse
from pathlib import Path
import plistlib
import re
import shlex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("title")
    parser.add_argument("level", type=Path)
    parser.add_argument("assets", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9 -]*", args.title):
        parser.error("title must use letters, numbers, spaces or hyphens")
    root = Path(__file__).resolve().parent.parent
    level, assets = args.level.resolve(), args.assets.resolve()
    binary = root / "build/volcano"
    if not binary.is_file() or not level.is_file() or not assets.is_dir():
        parser.error("build the game and level first, and supply the asset directory")
    bundle = root / "build" / f"{args.title}.app"
    executable = re.sub(r"[^A-Za-z0-9]", "", args.title)
    macos = bundle / "Contents/MacOS"
    macos.mkdir(parents=True, exist_ok=True)
    launcher = macos / executable
    command = shlex.join([str(binary), "--assets", str(assets), "--level-file", str(level)])
    launcher.write_text(f"#!/bin/sh\ncd {shlex.quote(str(root))}\nexec {command}\n")
    launcher.chmod(0o755)
    with (bundle / "Contents/Info.plist").open("wb") as output:
        plistlib.dump({
            "CFBundleDisplayName": args.title, "CFBundleName": args.title,
            "CFBundleExecutable": executable,
            "CFBundleIdentifier": f"local.volcano.{executable.lower()}",
            "CFBundlePackageType": "APPL", "CFBundleVersion": "1",
            "CFBundleShortVersionString": "1.0", "NSHighResolutionCapable": True,
        }, output)
    print(f"Created {bundle}")


if __name__ == "__main__":
    main()
