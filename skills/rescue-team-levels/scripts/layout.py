#!/usr/bin/env python3
"""Print route records or atlas-aware placement measurements; never edit inputs."""
import argparse
import json
import math
from pathlib import Path
import re

WIDTH, HEIGHT = 1366, 768
DECL = re.compile(r"texinfo\s*=\s*createTextureInfoEx\(\s*(['\"])(.*?)\1\s*\)")
NUMBER = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?"
FIELD = re.compile(r"\b(dx|dy|w|h)\s*=\s*(" + NUMBER + r")\b")


def pair(value):
    if (not isinstance(value, (list, tuple)) or len(value) != 2 or
            any(type(v) not in (int, float) or not math.isfinite(v) for v in value)):
        raise ValueError("coordinates must be two finite numbers")
    return tuple(value)


def world(point, screen):
    x, y = pair(point)
    return x, HEIGHT - y if screen else y


def atlas_frames(assets, names):
    folder = assets / "texture/lazy"
    if not folder.is_dir():
        raise ValueError(f"missing original sprite directory: {folder}")
    found = {}
    for path in sorted(folder.glob("*.lua")):
        data = path.read_bytes()
        source = data.decode("utf-16" if data.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig")
        declarations = list(DECL.finditer(source))
        for index, declaration in enumerate(declarations):
            name = declaration.group(2)
            if name not in names:
                continue
            if name in found:
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
            found[name] = (str(path), frames)
    missing = set(names) - found.keys()
    if missing:
        raise ValueError("unknown original sprite(s): " + ", ".join(sorted(missing)))
    return found


def anchor(args):
    names = list(dict.fromkeys([args.sprite] + args.compare_sprite))
    atlas = atlas_frames(args.assets, names)
    cx, cy = world(args.center, args.screen)
    frames = atlas[args.sprite][1]
    if not 0 <= args.frame < len(frames):
        raise ValueError(f"frame must be in 0..{len(frames)-1}")
    chosen = frames[args.frame]
    x, y = cx - chosen["w"] / 2 + chosen["dx"], cy - chosen["h"] / 2 + chosen["dy"]
    states = []
    for name in names:
        path, rows = atlas[name]
        for index, f in enumerate(rows):
            states.append({"sprite": name, "frame": index, "source": path, **f,
                           "visible_center_world": [x + f["w"]/2 - f["dx"], y + f["h"]/2 - f["dy"]],
                           "visible_rect_screen": [x - f["dx"], HEIGHT-y-f["h"]+f["dy"], f["w"], f["h"]]})
    return {"clearing_center_world": [cx, cy], "position": {"x": x, "y": y}, "states": states}


def route(args):
    if not math.isfinite(args.spacing) or not 1 <= args.spacing < 50:
        raise ValueError("spacing must be at least 1 and strictly below 50")
    if not 0 <= args.id <= 0xffffffff or not 0 <= args.layer <= 0xffffffff:
        raise ValueError("id and layer must fit uint32")
    points, seen = [], set()
    for vertices in args.points:
        if not isinstance(vertices, list) or len(vertices) < 2:
            raise ValueError("each --points value must be a JSON polyline with at least two vertices")
        vertices = [world(p, args.screen) for p in vertices]
        for a, b in zip(vertices, vertices[1:]):
            steps = max(1, math.ceil(math.dist(a, b) / args.spacing))
            if steps > 4095:
                raise ValueError("one segment exceeds the engine's 4096-point total limit")
            for index in range(steps + 1):
                p = tuple(round(a[c] + (b[c]-a[c])*index/steps, 6) for c in (0, 1))
                if p not in seen:
                    points.append(list(p));seen.add(p)
                if len(points) > 4096:
                    raise ValueError("route exceeds the engine's 4096-point total limit")
    return {"type": "traffic", "id": args.id, "layer": args.layer, "points": points}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    placement = commands.add_parser("anchor", help="measure authored anchors and compare sprite states")
    placement.add_argument("--assets", type=Path, required=True)
    placement.add_argument("--sprite", required=True)
    placement.add_argument("--compare-sprite", action="append", default=[])
    placement.add_argument("--frame", type=int, default=0)
    placement.add_argument("--center", type=float, nargs=2, required=True)
    placement.add_argument("--screen", action="store_true", help="center uses top-left logical canvas coordinates")
    traffic = commands.add_parser("route", help="sample and deduplicate logical-canvas polylines")
    traffic.add_argument("--points", type=json.loads, action="append", required=True)
    traffic.add_argument("--spacing", type=float, default=30)
    traffic.add_argument("--screen", action="store_true", help="vertices use top-left logical canvas coordinates")
    traffic.add_argument("--id", type=int, default=1)
    traffic.add_argument("--layer", type=int, default=4)
    args = parser.parse_args()
    try:
        result = anchor(args) if args.command == "anchor" else route(args)
    except (ValueError, OSError, UnicodeError, OverflowError) as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
