#!/usr/bin/env python3
"""Bundle the HD assets, executable and both custom missions for macOS."""
import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MISSIONS = {"emerald-falls": "Emerald Falls HD", "forest-mayday": "Forest Mayday HD"}


def bundle_runtime(binary, frameworks, resources):
    """Copy non-system dylibs so the local app does not need Homebrew."""
    frameworks.mkdir(exist_ok=True)
    pending = [binary]
    copied = {}
    while pending:
        target = pending.pop()
        dependencies = subprocess.check_output(["otool", "-L", str(target)], text=True)
        for line in dependencies.splitlines()[1:]:
            dependency = line.strip().split(" (")[0]
            if not dependency.startswith("/") or dependency.startswith(("/usr/lib/", "/System/Library/")):
                continue
            source = Path(dependency)
            if source.name == target.name:
                continue  # dylib's own install name
            destination = frameworks / source.name
            if source.name not in copied:
                shutil.copy2(source, destination)
                destination.chmod(0o755)
                copied[source.name] = destination
                pending.append(destination)
                subprocess.run(["install_name_tool", "-id", "@rpath/" + source.name, str(destination)], check=True)
                if "SDL2" in source.name:
                    license_path = source.parent.parent / "share/licenses/SDL2/LICENSE.txt"
                    if not license_path.is_file():
                        license_path = source.resolve().parent.parent / "LICENSE.txt"
                    shutil.copy2(license_path, resources / "SDL2-LICENSE.txt")
            subprocess.run(["install_name_tool", "-change", dependency,
                            "@executable_path/../Frameworks/" + source.name, str(target)], check=True)
    for target in [*copied.values(), binary]:
        subprocess.run(["codesign", "--force", "--sign", "-", str(target)], check=True)


def metadata(path, title, executable):
    with path.open("wb") as stream:
        plistlib.dump({
            "CFBundleDisplayName": title, "CFBundleName": title,
            "CFBundleExecutable": executable,
            "CFBundleIdentifier": "local.volcano." + title.lower().replace(" ", "-"),
            "CFBundlePackageType": "APPL", "CFBundleVersion": "1",
            "CFBundleShortVersionString": "1.0", "NSHighResolutionCapable": True,
        }, stream)


def package(assets):
    assets = assets.resolve()
    report = assets / "hd-static-report.json"
    binary = ROOT / "build/volcano"
    if not report.is_file() or not binary.is_file():
        raise ValueError("build the executable and HD assets first")
    bundle = ROOT / "build/Volcano HD.app"
    contents = bundle / "Contents"
    macos, resources = contents / "MacOS", contents / "Resources"
    macos.mkdir(parents=True, exist_ok=True)
    resources.mkdir(parents=True, exist_ok=True)
    shutil.copy2(binary, macos / "volcano-bin")
    subprocess.run(["rsync", "-a", "--exclude=.DS_Store", str(assets) + "/", str(resources) + "/"], check=True)
    particles = resources / "texture/particles"
    particles.mkdir(parents=True, exist_ok=True)
    for name in ("log-fire.png", "log-smoke.png", "README.md", "source/Kenney-license.txt"):
        shutil.copy2(ROOT / "assets/particles" / name, particles / Path(name).name)
    bundle_runtime(macos / "volcano-bin", contents / "Frameworks", resources)
    levels = resources / "custom-levels"
    levels.mkdir(exist_ok=True)
    for slug in MISSIONS:
        for suffix in (".lvl", ".lvl.rules", ".png"):
            shutil.copy2(ROOT / "build/hd-levels" / (slug + suffix), levels / (slug + suffix))
    launcher = macos / "volcano"
    launcher.write_text(
        '#!/bin/sh\n'
        'contents=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)\n'
        'exec "$contents/MacOS/volcano-bin" --assets "$contents/Resources" '
        '--level-file "$contents/Resources/custom-levels/emerald-falls.lvl" "$@"\n'
    )
    launcher.chmod(0o755)
    metadata(contents / "Info.plist", "Volcano HD", "volcano")
    subprocess.run(["codesign", "--force", "--sign", "-", str(bundle)], check=True)
    print(f"Created self-contained {bundle} (opens Emerald Falls)")
    # These small launchers refer to the sibling bundle. Both custom missions
    # and their backgrounds live inside that bundle, independent of build/levels.
    for slug, title in MISSIONS.items():
        mini = ROOT / "build" / (title + ".app") / "Contents"
        (mini / "MacOS").mkdir(parents=True, exist_ok=True)
        launch = mini / "MacOS/volcano"
        launch.write_text(
            '#!/bin/sh\n'
            'build_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)\n'
            f'exec "$build_dir/Volcano HD.app/Contents/MacOS/volcano" '
            f'--level-file "$build_dir/Volcano HD.app/Contents/Resources/custom-levels/{slug}.lvl" "$@"\n'
        )
        launch.chmod(0o755)
        metadata(mini / "Info.plist", title, "volcano")
        print(f"Created {mini.parent}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("assets", type=Path)
    args = parser.parse_args()
    try:
        package(args.assets)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
