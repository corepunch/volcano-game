#!/usr/bin/env python3
"""Print mission.xml snippets: sampled <traffic> roads and <object> placements.

    layout.py route --screen --points '[[165,450],[450,450]]' --points '[[450,450],[455,350]]'
    layout.py place --screen --center 330.25 327.25 --id 30 --type cafe

An object's x,y is the centre of its base, and the engine centres the sprite
on it, so placing a building is measuring its clearing's centre. Output is
XML ready to paste into a mission. Never edits its inputs.
"""
import argparse
import json
import math

WIDTH, HEIGHT = 1366, 768
MAX_POINTS = 4096


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


def place(args):
    if bool(args.type) != (args.id is not None):
        raise ValueError("--id and --type go together")
    if args.id is not None and args.id < 1:
        raise ValueError("--id must be positive")
    x, y = world(args.center, args.screen)
    position = f'x="{number(x)}" y="{number(y)}"'
    return f'<object id="{args.id}" type="{args.type}" {position}/>' if args.type else position


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
    placement = commands.add_parser("place", help="an <object> standing on a measured clearing centre")
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
        result = place(args) if args.command == "place" else route(args)
    except (ValueError, OverflowError) as error:
        parser.error(str(error))
    print(result)


if __name__ == "__main__":
    main()
