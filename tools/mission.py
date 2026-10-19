#!/usr/bin/env python3
"""Check native mission files (missions/NAME/mission.xml).

    python3 tools/mission.py check missions/library-rescue/mission.xml
    python3 tools/mission.py check missions/*/mission.xml

Errors are problems the game would refuse to load; warnings point at likely
design mistakes (disconnected roads, unreachable objects, goal mismatches).
The game's own loader (src/content/mission.c) remains the authority.
"""
import argparse
import math
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
FORMAT = "volcano-mission-1"
LINK_DISTANCE = 50
CROWD_RADIUS = 80
APPROACHED = {"obstacle", "producer", "worker", "pickup", "civilian", "base", "valve", "lot", "service", "animal"}


def load_archetypes(content):
    """id -> class, following extends chains."""
    classes, parents = {}, {}
    for node in ET.parse(content / "archetypes.xml").getroot().iter("archetype"):
        ident = node.get("id")
        if node.get("class"):
            classes[ident] = node.get("class")
        parents[ident] = node.get("extends")
    for ident in parents:
        seen, current = set(), ident
        while current not in classes and parents.get(current) and current not in seen:
            seen.add(current)
            current = parents[current]
        if current in classes:
            classes[ident] = classes[current]
    return classes


def components(points):
    unseen, groups = set(range(len(points))), []
    while unseen:
        pending = [unseen.pop()]
        size = 1
        while pending:
            p = points[pending.pop()]
            near = [j for j in unseen if math.dist(p, points[j]) < LINK_DISTANCE]
            unseen.difference_update(near)
            pending.extend(near)
            size += len(near)
        groups.append(size)
    return sorted(groups, reverse=True)


def check(path, classes):
    errors, warnings = [], []
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as error:
        return [f"not well-formed XML: {error}"], []
    if root.tag != "mission" or root.get("format") != FORMAT:
        return [f'root must be <mission format="{FORMAT}">'], []
    folder = path.parent
    background = root.get("background")
    if bool(background) == bool(root.get("retail-background")):
        errors.append("needs exactly one of background or retail-background")
    elif background and not (folder / background).is_file():
        errors.append(f"background image not found: {background}")
    for tag in ("pack", "sprites"):
        for node in root.iter(tag):
            target = folder / (node.get("path") or "")
            if not (target.is_dir() if tag == "pack" else target.is_file()):
                errors.append(f"<{tag}> path not found: {node.get('path')}")
    local = dict(classes)
    for node in root.iter("archetype"):
        parent = node.get("extends")
        if parent and parent not in local:
            errors.append(f"archetype {node.get('id')} extends unknown {parent}")
        local[node.get("id")] = node.get("class") or local.get(parent)
    settings = root.findall("settings")
    if len(settings) != 1:
        errors.append("needs exactly one <settings>")
        settings = [ET.Element("settings")]
    settings = settings[0]
    points = []
    for node in root.iter("traffic"):
        for pair in (node.text or "").split():
            try:
                x, y = (float(v) for v in pair.split(","))
                points.append((x, y))
            except ValueError:
                errors.append(f"traffic point is not x,y: {pair}")
    objects, ids = [], set()
    for node in root.iter("object"):
        ident, kind = node.get("id"), node.get("type")
        if ident in ids:
            errors.append(f"duplicate object id {ident}")
        ids.add(ident)
        if kind not in local:
            errors.append(f"object {ident}: unknown type {kind}")
            continue
        objects.append((ident, kind, local[kind], float(node.get("x", 0)), float(node.get("y", 0)), node))
    for ident, kind, cls, x, y, node in objects:
        for relation in ("producer", "gate", "service", "support"):
            if node.get(relation) and node.get(relation) not in ids:
                errors.append(f"object {ident}: {relation}={node.get(relation)} names no object")
        if cls in APPROACHED and points:
            gap = min(math.dist((x, y), p) for p in points)
            if gap >= LINK_DISTANCE:
                warnings.append(f"object {ident} ({kind}): nearest traffic point is {gap:.0f} units away; check its approach")
    if not points:
        errors.append("no traffic points")
    sizes = components(points)
    if len(sizes) > 1:
        warnings.append(f"traffic has {len(sizes)} separate networks before obstacles {sizes}; water or offroad networks may be intentional")
    bases = [(x, y) for _, _, cls, x, y, _ in objects if cls == "base"]
    workers = [(x, y, node) for _, _, cls, x, y, node in objects if cls == "worker"]
    if not bases:
        errors.append("no headquarters (a base archetype such as hq)")
    if not workers:
        errors.append("no workers")
    recruits = sum(1 for x, y, node in workers if bases and node.get("transport", "foot") == "foot" and
                   math.dist((x, y), bases[0]) >= CROWD_RADIUS and settings.get("crew-at-hq") != "true")
    if workers and recruits == len(workers):
        warnings.append(f"every worker starts stranded; place one within {CROWD_RADIUS} units of headquarters")
    people = sum(1 for _, _, cls, *_ in objects if cls == "civilian") + recruits
    goal = int(settings.get("people", 0))
    if goal != people:
        warnings.append(f"people goal is {goal}; the mission has {people} civilians and stranded recruits")
    if int(settings.get("basins", 0)) > sum(1 for _, _, cls, *_ in objects if cls == "valve"):
        warnings.append("basin goal exceeds the number of valves")
    return errors, warnings


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    sub = commands.add_parser("check", help="validate missions and report design warnings")
    sub.add_argument("missions", type=Path, nargs="+")
    sub.add_argument("--content", type=Path, default=ROOT / "content")
    args = parser.parse_args(argv)
    classes = load_archetypes(args.content)
    failed = False
    for path in args.missions:
        errors, warnings = check(path, classes)
        failed |= bool(errors)
        print(f"{path}: {'INVALID' if errors else 'ok'} ({len(errors)} errors, {len(warnings)} warnings)")
        for message in errors:
            print("  error:", message)
        for message in warnings:
            print("  warning:", message)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
