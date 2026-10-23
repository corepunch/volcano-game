#!/usr/bin/env python3
"""Build a self-contained macOS app bundle.

    python3 tools/package_app.py --binary build/volcano --assets reference/rescue-team --output build
    python3 tools/package_app.py ... --mission library-rescue      # opens that mission

Everything the game reads is copied into Contents/Resources: the asset
folder (as resources/), content/, missions/ and the project's packs. The
launcher refers only to paths inside the bundle, so the app can be moved or
shared. Retail assets are not redistributable; ship them only where licensed.
"""
import argparse
from pathlib import Path
import plistlib
import re
import shlex
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
IGNORE = shutil.ignore_patterns(".DS_Store", "*.prompt.txt", "design.txt")


def title_of(mission):
    if not mission:
        return "Volcano"
    return " ".join(word.capitalize() for word in mission.split("-"))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mission", default="", help="mission folder name; empty opens the campaign")
    args = parser.parse_args(argv)
    mission = args.mission
    if mission and not (ROOT / "missions" / mission / "mission.xml").is_file():
        parser.error(f"no such mission: missions/{mission}/mission.xml")
    if not args.binary.is_file() or not (args.assets / "texture/lazy").is_dir():
        parser.error("build the game first and pass the retail asset folder")
    title = title_of(mission)
    executable = re.sub(r"[^A-Za-z0-9]", "", title)
    bundle = args.output / f"{title}.app"
    if bundle.exists():
        shutil.rmtree(bundle)
    macos, resources = bundle / "Contents/MacOS", bundle / "Contents/Resources"
    macos.mkdir(parents=True)
    shutil.copy2(args.binary, macos / "volcano")
    shutil.copytree(args.assets, resources / "resources", ignore=IGNORE)
    shutil.copytree(ROOT / "content", resources / "content", ignore=IGNORE)
    shutil.copytree(ROOT / "missions", resources / "missions", ignore=IGNORE)
    for pack in ("particles", "kenney-characters", "volcano-episode"):
        shutil.copytree(ROOT / "assets" / pack, resources / "assets" / pack,
                        ignore=shutil.ignore_patterns(".DS_Store", "source", "blender", "previews", "*.zip"))
    if mission:
        launcher = macos / executable
        target = shlex.quote(f"missions/{mission}/mission.xml")
        launcher.write_text("#!/bin/sh\n"
                            'here="$(cd "$(dirname "$0")" && pwd)"\n'
                            'cd "$here/../Resources" || exit 1\n'
                            f'exec "$here/volcano" --mission {target} "$@"\n')
        launcher.chmod(0o755)
    else:
        executable = "volcano"
    with (bundle / "Contents/Info.plist").open("wb") as output:
        plistlib.dump({
            "CFBundleDisplayName": title, "CFBundleName": title, "CFBundleExecutable": executable,
            "CFBundleIdentifier": f"local.volcano.{executable.lower()}", "CFBundlePackageType": "APPL",
            "CFBundleVersion": "1", "CFBundleShortVersionString": "1.0", "NSHighResolutionCapable": True,
        }, output)
    print(f"Created {bundle}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
