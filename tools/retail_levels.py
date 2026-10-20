#!/usr/bin/env python3
"""Inspect original Rescue Team 5 level binaries (version 301).

The game imports retail levels at runtime (src/content/retail_import.c); this
tool only reads them for study. Missions of our own are XML (tools/mission.py).

    python3 tools/retail_levels.py dump reference/rescue-team/level/level015.lvl build/level015.xml
    python3 tools/retail_levels.py analyze reference/rescue-team/level build/level-analysis.json
"""
import argparse
import collections
import json
import math
from pathlib import Path
import struct
import sys
from xml.sax.saxutils import quoteattr

MAX_OBJECTS = MAX_POINTS = 4096
TYPES = {
    2: "scenery", 3: "marker", 4: "background", 5: "effect", 6: "ambient",
    13: "unknown_marker", 18: "traffic", 101: "building", 102: "barrier",
    104: "worker", 105: "animal", 108: "resource", 109: "civilian", 110: "base",
    111: "helicopter", 112: "helipad", 113: "boat", 114: "pier", 116: "doctor",
    117: "hospital", 118: "station", 119: "train", 121: "thief", 122: "road_entry",
    123: "police", 124: "police_station", 125: "firefighter", 126: "fire_station",
    128: "valve", 132: "scenario", 134: "medicine", 135: "campaign_node",
    138: "campaign_bridge", 140: "bonfire", 141: "air_delivery", 142: "delivery_place",
    144: "generator", 146: "lot", 147: "boost", 148: "campaign_tile",
    149: "scenario_marker", 150: "runway_light", 151: "pollution", 152: "offroad_region",
    153: "atv", 154: "garage",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


class Reader:
    def __init__(self, data):
        require(len(data) <= 64 * 1024 * 1024, "binary exceeds runtime's 64 MiB limit")
        self.data, self.pos = data, 0

    def read(self, n):
        require(self.pos + n <= len(self.data), f"byte {self.pos}: truncated binary")
        value = self.data[self.pos:self.pos + n]
        self.pos += n
        return value

    def integer(self):
        return struct.unpack("<I", self.read(4))[0]

    def scalar(self):
        value = struct.unpack("<f", self.read(4))[0]
        require(math.isfinite(value), f"byte {self.pos}: non-finite float")
        return value

    def string(self, capacity):
        n = struct.unpack("<H", self.read(2))[0]
        require(n < capacity, f"byte {self.pos}: string exceeds runtime capacity")
        data = self.read(n)
        require(b"\0" not in data, f"byte {self.pos}: embedded NUL")
        return data.decode("utf-8", errors="surrogateescape")


def decode(data):
    reader = Reader(data)
    require(reader.integer() == 301, "only binary version 301 is supported")
    source = {"version": 301, "background": reader.string(128), "max_id": reader.integer(), "objects": []}
    count = reader.integer()
    require(count <= MAX_OBJECTS, "maximum 4096 objects")
    total_points = 0
    for _ in range(count):
        obj = {"type": reader.integer(), "id": reader.integer()}
        if obj["type"] == 18:
            count_points = reader.integer()
            total_points += count_points
            require(total_points <= MAX_POINTS, "maximum 4096 traffic points")
            obj["points"] = [[reader.scalar(), reader.scalar()] for _ in range(count_points)]
            obj["layer"] = reader.integer()
        else:
            obj["layer"] = reader.integer()
            for key, capacity in (("name", 512), ("params", 4096), ("extra", 4096)):
                obj[key] = reader.string(capacity)
            obj["link_id"] = reader.integer()
            for key in ("x", "y", "rotation", "scale_x", "scale_y"):
                obj[key] = reader.scalar()
            if obj["type"] == 6:
                obj["auxiliary"] = [reader.scalar(), reader.scalar()]
        source["objects"].append(obj)
    require(reader.pos == len(data), f"byte {reader.pos}: unsupported trailing data")
    return source


def encode(source):
    """Re-encode decoded records; used to prove the reader is lossless."""
    def text(value):
        data = value.encode("utf-8", errors="surrogateescape")
        return struct.pack("<H", len(data)) + data
    out = bytearray(struct.pack("<I", 301)) + text(source["background"])
    out += struct.pack("<II", source["max_id"], len(source["objects"]))
    for obj in source["objects"]:
        out += struct.pack("<II", obj["type"], obj["id"])
        if obj["type"] == 18:
            out += struct.pack("<I", len(obj["points"]))
            for x, y in obj["points"]:
                out += struct.pack("<ff", x, y)
            out += struct.pack("<I", obj["layer"])
            continue
        out += struct.pack("<I", obj["layer"]) + text(obj["name"]) + text(obj["params"]) + text(obj["extra"])
        out += struct.pack("<I", obj["link_id"])
        for key in ("x", "y", "rotation", "scale_x", "scale_y"):
            out += struct.pack("<f", obj[key])
        if obj["type"] == 6:
            out += struct.pack("<ff", *obj["auxiliary"])
    return bytes(out)


def dump(source):
    """Lossless XML record dump of a retail binary, for study and diffs."""
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             f'<retail-level version="301" background={quoteattr(source["background"])} max-id="{source["max_id"]}">']
    for obj in source["objects"]:
        kind = TYPES.get(obj["type"], "unknown")
        if obj["type"] == 18:
            points = " ".join(f"{x:.9g},{y:.9g}" for x, y in obj["points"])
            lines.append(f'  <traffic id="{obj["id"]}" layer="{obj["layer"]}">{points}</traffic>')
            continue
        attrs = [("type", obj["type"]), ("kind", kind), ("id", obj["id"]), ("layer", obj["layer"])]
        attrs += [(k, obj[k]) for k in ("name", "params", "extra") if obj[k]]
        if obj["link_id"]:
            attrs.append(("link", obj["link_id"]))
        attrs += [(k.replace("_", "-"), f"{obj[k]:.9g}") for k in ("x", "y", "rotation", "scale_x", "scale_y")]
        if obj["type"] == 6:
            attrs.append(("auxiliary", ",".join(f"{v:.9g}" for v in obj["auxiliary"])))
        lines.append("  <record" + "".join(f" {k}={quoteattr(str(v))}" for k, v in attrs) + "/>")
    lines.append("</retail-level>")
    return "\n".join(lines) + "\n"


def graph_components(points):
    unseen, components = set(range(len(points))), []
    while unseen:
        pending = [unseen.pop()]
        group = pending[:]
        while pending:
            p = points[pending.pop()]
            neighbours = [j for j in unseen if math.dist(p, points[j]) < 50]
            unseen.difference_update(neighbours)
            pending.extend(neighbours)
            group.extend(neighbours)
        components.append(group)
    return components


def analyze(directory):
    levels = {}
    for path in sorted(directory.glob("*.lvl")):
        data = path.read_bytes()
        source = decode(data)
        require(encode(source) == data, f"{path}: byte round trip failed")
        levels[path.stem] = source
    require(bool(levels), f"no .lvl files in {directory}")
    counts, samples, parameters = collections.Counter(), {}, collections.defaultdict(collections.Counter)
    rows = []
    for name, source in levels.items():
        points = [p for o in source["objects"] if o["type"] == 18 for p in o["points"]]
        types = collections.Counter(o["type"] for o in source["objects"])
        rows.append({"file": name, "background": source["background"], "objects": len(source["objects"]), "traffic_points": len(points), "traffic_components": sorted(map(len, graph_components(points)), reverse=True), "types": dict(sorted(types.items()))})
        for obj in source["objects"]:
            code = obj["type"]
            counts[code] += 1
            samples.setdefault(code, {"file": name, "object": obj})
            if obj.get("params"):
                parameters[code][obj["params"]] += 1
    order = []
    if "map" in levels:
        nodes = [o for o in levels["map"]["objects"] if o["type"] == 135]
        previous, seen = "", set()
        while True:
            following = [o for o in nodes if o["extra"] == previous]
            if not following:
                break
            require(len(following) == 1 and following[0]["id"] not in seen, "invalid campaign chain")
            node = following[0]
            seen.add(node["id"])
            matching = [n for n, s in levels.items() if n.startswith("level") and s["background"] == node["name"]]
            require(len(matching) == 1, "ambiguous campaign background")
            order.append(matching[0])
            previous = node["name"]
        require(len(seen) == len(nodes), "incomplete campaign chain")
    first_seen = {}
    for position, name in enumerate(order, 1):
        for obj in levels[name]["objects"]:
            first_seen.setdefault(obj["type"], {"campaign": position, "file": name})
    return {"roundtripped_files": len(levels), "campaign_order": order, "first_type_appearance": first_seen, "levels": rows,
            "catalog": [{"type": code, "interpretation": TYPES.get(code, "unknown"), "count": counts[code], "params": dict(parameters[code].most_common()), "example": samples[code]} for code in sorted(counts)]}


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=True, allow_nan=False) + "\n", encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    sub = commands.add_parser("dump", help="binary level to a lossless XML record dump")
    sub.add_argument("input", type=Path)
    sub.add_argument("output", type=Path)
    sub = commands.add_parser("analyze", help="round-trip and inventory all original binaries")
    sub.add_argument("directory", type=Path)
    sub.add_argument("output", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "dump":
            source = decode(args.input.read_bytes())
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(dump(source), encoding="utf-8")
            print(f"Wrote {args.output}: {len(source['objects'])} records")
        else:
            report = analyze(args.directory)
            write_json(args.output, report)
            print(f"Byte-identical round trip: {report['roundtripped_files']} files; report: {args.output}")
        return 0
    except (ValueError, OSError, UnicodeError) as error:
        print(f"retail-levels: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
