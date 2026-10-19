#!/usr/bin/env python3
"""Print mission.xml snippets: sampled <traffic> roads and sprite-aware <object> anchors.

    layout.py route --screen --points '[[165,450],[450,450]]' --points '[[450,450],[455,350]]'
    layout.py anchor --assets reference/rescue-team --sprite burger0_br \\
        --center 330.25 327.25 --screen --compare-sprite burger0 --id 30 --type cafe

Output is XML ready to paste into a mission; measurements are XML comments.
Sprites come from retail declarations (texture/lazy/*.lua) and pack sheets
(sprites/*.sprites.xml) under each --assets folder, then from --sprites files
such as a mission's sprites.xml (later sources win). Never edits its inputs.
"""
import argparse
import json
import math
from pathlib import Path
import re
import xml.etree.ElementTree as ET

WIDTH, HEIGHT = 1366, 768
MAX_POINTS = 4096
DECL = re.compile(r"texinfo\s*=\s*createTextureInfoEx\(\s*(['\"])(.*?)\1\s*\)")
NUMBER = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?"
FIELD = re.compile(r"\b(dx|dy|w|h)\s*=\s*(" + NUMBER + r")\b")


def number(value):
    """World units with at most two decimals, as the project's missions use."""
    text = f"{value:.2f}".rstrip("0").rstrip(".")
    return "0" if text in ("-0", "") else text


def pair(value):
    if (not isinstance(value, (list, tuple)) or len(value) != 2 or
            any(type(v) not in (int, float) or not math.isfinite(v) for v in value)):
        raise ValueError("coordinates must be two finite numbers")
    return tuple(value)


def world(point, screen):
    x, y = pair(point)
    return x, HEIGHT - y if screen else y


def retail_frames(folder, names, found):
    """Constant frame tables from original texture/lazy/*.lua declarations (data, not code)."""
    retail = {}
    for path in sorted(folder.glob("*.lua")):
        data = path.read_bytes()
        source = data.decode("utf-16" if data.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig")
        declarations = list(DECL.finditer(source))
        for index, declaration in enumerate(declarations):
            name = declaration.group(2)
            if name not in names:
                continue
            if name in retail:
                raise ValueError(f"duplicate sprite declaration: {name}")
            end = declarations[index + 1].start() if index + 1 < len(declarations) else len(source)
            section = source[declaration.end():end]
            block = re.search(r"\bframes\s*=\s*\{(.*?)\}\s*addTextureInfoExNodes\s*\(", section, re.S)
            if not block:
                raise ValueError(f"unrecognized constant frame table for {name}; inspect {path}")
            frames = []
            for row in re.finditer(r"\{([^{}]*)\}", block.group(1)):
                fields = {key: float(value) for key, value in FIELD.findall(row.group(1))}
                if (set(fields) != {"dx", "dy", "w", "h"} or
                        not all(math.isfinite(v) for v in fields.values()) or
                        fields["w"] <= 0 or fields["h"] <= 0):
                    raise ValueError(f"invalid constant frame in {name}")
                frames.append(fields)
            if not frames:
                raise ValueError(f"no frames for {name}")
            retail[name] = (str(path), frames)
    found.update(retail)


def floats(node, key, count, default=None):
    text = node.get(key)
    if text is None:
        return default
    values = [float(v) for v in text.split(",")]
    if len(values) != count or not all(math.isfinite(v) for v in values):
        raise ValueError(f"{key}={text!r} needs {count} numbers")
    return values


def sheet_frames(path, names, found):
    """Frames from a native sprite sheet, following src/assets/assets.c."""
    root = ET.parse(path).getroot()
    if root.tag != "sprites":
        raise ValueError(f"{path}: expected <sprites> root")
    sheet_scale = float(root.get("scale", 1))
    for sprite in root.iter("sprite"):
        name = sprite.get("name")
        if name not in names:
            continue
        scale = float(sprite.get("scale", sheet_scale))
        frames = []
        for node in list(sprite.iter("frame")) or [sprite]:
            rect = floats(node, "rect", 4)
            if rect is None:
                raise ValueError(f"{path}: sprite {name} has no rect")
            w, h = floats(node, "size", 2, [rect[2] * scale, rect[3] * scale])
            px, py = floats(node, "pivot", 2, [0.5, 1])
            dx, dy = floats(node, "anchor", 2, [w * px, h * (1 - py)])
            for _ in range(int(node.get("repeat", 1))):
                frames.append({"dx": dx, "dy": dy, "w": w, "h": h})
        found[name] = (str(path), frames)


def load_frames(args, names):
    found = {}
    for folder in args.assets:
        lazy = folder / "texture/lazy"
        if lazy.is_dir():
            retail_frames(lazy, names, found)
        for sheet in sorted((folder / "sprites").glob("*.sprites.xml")):
            sheet_frames(sheet, names, found)
    for sheet in args.sprites:
        sheet_frames(sheet, names, found)
    missing = set(names) - found.keys()
    if missing:
        raise ValueError("unknown sprite(s): " + ", ".join(sorted(missing)))
    return found


def anchor(args):
    if not args.assets and not args.sprites:
        raise ValueError("pass --assets FOLDER and/or --sprites SHEET.xml")
    if bool(args.type) != (args.id is not None):
        raise ValueError("--id and --type go together")
    if args.id is not None and args.id < 1:
        raise ValueError("--id must be positive")
    names = list(dict.fromkeys([args.sprite] + args.compare_sprite))
    atlas = load_frames(args, names)
    cx, cy = world(args.center, args.screen)
    frames = atlas[args.sprite][1]
    if not 0 <= args.frame < len(frames):
        raise ValueError(f"frame must be in 0..{len(frames)-1}")
    chosen = frames[args.frame]
    x, y = cx - chosen["w"] / 2 + chosen["dx"], cy - chosen["h"] / 2 + chosen["dy"]
    lines = [f"<!-- clearing centre world ({number(cx)},{number(cy)}); "
             f"position screen ({number(x)},{number(HEIGHT - y)}) -->"]
    for name in names:
        path, rows = atlas[name]
        for index, f in enumerate(rows):
            centre = (x + f["w"] / 2 - f["dx"], y + f["h"] / 2 - f["dy"])
            rect = (x - f["dx"], HEIGHT - y - f["h"] + f["dy"], f["w"], f["h"])
            lines.append(f"<!-- {name} frame {index} ({path}): w={number(f['w'])} h={number(f['h'])} "
                         f"dx={number(f['dx'])} dy={number(f['dy'])}; visible centre world "
                         f"({number(centre[0])},{number(centre[1])}); screen rect "
                         f"{','.join(number(v) for v in rect)} -->")
    position = f'x="{number(x)}" y="{number(y)}"'
    lines.append(f'<object id="{args.id}" type="{args.type}" {position}/>' if args.type else position)
    return "\n".join(lines)


def route(args):
    if not math.isfinite(args.spacing) or not 1 <= args.spacing < 50:
        raise ValueError("spacing must be at least 1 and strictly below 50")
    points, seen = [], set()
    for vertices in args.points:
        if not isinstance(vertices, list) or len(vertices) < 2:
            raise ValueError("each --points value must be a JSON polyline with at least two vertices")
        vertices = [world(p, args.screen) for p in vertices]
        for a, b in zip(vertices, vertices[1:]):
            steps = max(1, math.ceil(math.dist(a, b) / args.spacing))
            if steps >= MAX_POINTS:
                raise ValueError(f"one segment exceeds the engine's {MAX_POINTS}-point total limit")
            for index in range(steps + 1):
                p = tuple(number(a[c] + (b[c] - a[c]) * index / steps) for c in (0, 1))
                if p not in seen:
                    points.append(",".join(p))
                    seen.add(p)
                if len(points) > MAX_POINTS:
                    raise ValueError(f"route exceeds the engine's {MAX_POINTS}-point total limit")
    return f"<traffic>{' '.join(points)}</traffic>"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    placement = commands.add_parser("anchor", help="anchor a sprite's visible centre on a measured clearing")
    placement.add_argument("--assets", type=Path, action="append", default=[],
                           help="retail or pack folder (texture/lazy/*.lua, sprites/*.sprites.xml); repeatable")
    placement.add_argument("--sprites", type=Path, action="append", default=[],
                           help="extra sprite sheet, e.g. missions/NAME/sprites.xml; repeatable")
    placement.add_argument("--sprite", required=True)
    placement.add_argument("--compare-sprite", action="append", default=[])
    placement.add_argument("--frame", type=int, default=0)
    placement.add_argument("--center", type=float, nargs=2, required=True)
    placement.add_argument("--screen", action="store_true", help="center uses top-left logical canvas coordinates")
    placement.add_argument("--id", type=int, help="object id for the printed <object>")
    placement.add_argument("--type", help="archetype for the printed <object>; without it only x/y attributes print")
    traffic = commands.add_parser("route", help="sample polylines into a <traffic> element")
    traffic.add_argument("--points", type=json.loads, action="append", required=True)
    traffic.add_argument("--spacing", type=float, default=30)
    traffic.add_argument("--screen", action="store_true", help="vertices use top-left logical canvas coordinates")
    args = parser.parse_args()
    try:
        result = anchor(args) if args.command == "anchor" else route(args)
    except (ValueError, OSError, UnicodeError, OverflowError, ET.ParseError) as error:
        parser.error(str(error))
    print(result)


if __name__ == "__main__":
    main()
