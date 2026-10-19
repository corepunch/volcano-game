#!/usr/bin/env python3
"""Lossless Rescue Team 5 v301 level source compiler (Python standard library)."""
import argparse
import collections
import json
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
FORMAT = "volcano-level-1"
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
ALIASES = {name: code for code, name in TYPES.items()}
MISSION_KEYS = {"food", "wood", "money", "fuel", "rescue_goal", "basin_goal", "allow_cafe"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def uint(value, where):
    require(type(value) is int and 0 <= value <= 0xffffffff, f"{where}: expected uint32")
    return value


def number(value, where):
    require(type(value) in (int, float), f"{where}: expected finite number")
    try:
        require(math.isfinite(value), f"{where}: expected finite number")
        packed = struct.pack("<f", value)
    except (OverflowError, struct.error) as error:
        raise ValueError(f"{where}: outside float32 range") from error
    require(math.isfinite(struct.unpack("<f", packed)[0]), f"{where}: outside float32 range")
    return packed


def keys(obj, allowed, where):
    require(isinstance(obj, dict), f"{where}: expected object")
    require(not (set(obj) - set(allowed) - {"_comment"}), f"{where}: unknown fields {sorted(set(obj) - set(allowed) - {'_comment'})}")


def text_bytes(value, capacity, where):
    require(isinstance(value, str), f"{where}: expected string")
    data = value.encode("utf-8", errors="surrogateescape")
    require(b"\0" not in data and len(data) < capacity, f"{where}: NUL or string exceeds {capacity - 1} bytes")
    return struct.pack("<H", len(data)) + data


def mission_values(mission):
    keys(mission, MISSION_KEYS, "mission")
    require(MISSION_KEYS <= mission.keys(), "mission: supply every documented mission field")
    for key in MISSION_KEYS - {"allow_cafe"}:
        value = uint(mission[key], f"mission.{key}")
        require(value <= 1000000, f"mission.{key}: maximum is 1000000")
    require(type(mission["allow_cafe"]) is bool, "mission.allow_cafe: expected boolean")
    return "version=1\n" + "".join(f"{k}={int(mission[k])}\n" for k in sorted(MISSION_KEYS))


def encode(source):
    keys(source, {"$schema", "format", "version", "background", "max_id", "objects", "mission"}, "source")
    require(source.get("format") == FORMAT, f"format must be {FORMAT!r}")
    require(type(source.get("version", 301)) is int and source.get("version", 301) == 301, "only binary version 301 is supported")
    objects = source.get("objects")
    require(isinstance(objects, list) and len(objects) <= MAX_OBJECTS, "objects: expected array, maximum 4096")
    ids = [uint(o.get("id"), f"objects[{i}].id") if isinstance(o, dict) else uint(None, f"objects[{i}].id") for i, o in enumerate(objects)]
    require(len(ids) == len(set(ids)), "objects: duplicate IDs")
    maximum = uint(source.get("max_id", max(ids, default=0)), "max_id")
    output = bytearray(struct.pack("<I", 301))
    output += text_bytes(source.get("background"), 128, "background")
    if source["background"].startswith("@"):
        name = source["background"][1:]
        require(bool(name) and "/" not in name and "\\" not in name and name not in {".", ".."}, "custom background must be @FILENAME beside the source")
        require(Path(name).suffix.lower() in {".png", ".jpg", ".jpeg"}, "custom background must be a PNG or JPEG")
    output += struct.pack("<II", maximum, len(objects))
    total_points = 0
    for i, obj in enumerate(objects):
        where = f"objects[{i}] (id {ids[i]})"
        code = obj.get("type")
        if isinstance(code, str):
            code = ALIASES.get(code)
        code = uint(code, where + ".type")
        layer = uint(obj.get("layer", 0), where + ".layer")
        output += struct.pack("<II", code, ids[i])
        common = {"type", "id", "layer"}
        if code == 18:
            keys(obj, common | {"points"}, where)
            points = obj.get("points")
            require(isinstance(points, list), where + ".points: expected array")
            total_points += len(points)
            require(total_points <= MAX_POINTS, "maximum 4096 traffic points across all records")
            output += struct.pack("<I", len(points))
            for j, point in enumerate(points):
                require(isinstance(point, list) and len(point) == 2, f"{where}.points[{j}]: expected [x, y]")
                output += number(point[0], where) + number(point[1], where)
            output += struct.pack("<I", layer)
        else:
            keys(obj, common | {"name", "params", "extra", "link_id", "x", "y", "rotation", "scale_x", "scale_y"} | ({"auxiliary"} if code == 6 else set()), where)
            output += struct.pack("<I", layer)
            for key, capacity in (("name", 512), ("params", 4096), ("extra", 4096)):
                output += text_bytes(obj.get(key, ""), capacity, where + "." + key)
            output += struct.pack("<I", uint(obj.get("link_id", 0), where + ".link_id"))
            for key in ("x", "y", "rotation", "scale_x", "scale_y"):
                output += number(obj.get(key, 1 if key.startswith("scale_") else 0), where + "." + key)
            if code == 6:
                point = obj.get("auxiliary", [obj.get("x", 0), obj.get("y", 0)])
                require(isinstance(point, list) and len(point) == 2, where + ".auxiliary: expected [x, y]")
                output += number(point[0], where) + number(point[1], where)
    if "mission" in source:
        mission_values(source["mission"])
    return bytes(output)


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
    source = {"format": FORMAT, "version": 301, "background": reader.string(128), "max_id": reader.integer(), "objects": []}
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


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def parse_rules(content):
    require(len(content.encode("utf-8")) < 1023 and "\0" not in content, "invalid or oversized mission rules")
    pairs = []
    for line in content.splitlines():
        if not line:
            continue
        require("=" in line, "invalid mission rule")
        pairs.append(line.split("=", 1))
    values = unique_pairs(pairs)
    require(set(values) == MISSION_KEYS | {"version"} and values["version"] == "1", "unsupported mission rules")
    require(all(v.isascii() and v.isdecimal() for v in values.values()), "mission values must be unsigned decimal integers")
    require(values["allow_cafe"] in {"0", "1"}, "allow_cafe must be 0 or 1")
    mission = {k: bool(int(values[k])) if k == "allow_cafe" else int(values[k]) for k in MISSION_KEYS}
    mission_values(mission)
    return mission


def load_source(path):
    source = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_pairs)
    encode(source)
    return source


def canonical(source):
    return decode(encode(source))


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


def warnings(source):
    raw = canonical(source)
    objects = raw["objects"]
    ids = {obj["id"] for obj in objects}
    points = [p for obj in objects if obj["type"] == 18 for p in obj["points"]]
    result = []
    if not points:
        result.append("No traffic points: gameplay cannot initialize.")
    if not any(o["type"] == 104 for o in objects):
        result.append("No on-foot workers: a mission needs at least one.")
    if not any(o["type"] == 110 for o in objects):
        result.append("No headquarters: mission rules need a base.")
    if raw["max_id"] < max(ids, default=0):
        result.append("max_id is below an object ID; omit it when authoring a new scene.")
    components = graph_components(points)
    if len(components) > 1:
        result.append(f"Traffic has {len(components)} components before barriers (sizes {sorted(map(len, components), reverse=True)}); separate water/offroad networks may be intentional.")
    for obj in objects:
        link = obj.get("link_id", 0)
        if link and obj["type"] in {3, 108} and link not in ids:
            result.append(f"Object {obj['id']}: link_id {link} has no matching object; inspect its type-specific meaning.")
        if obj["type"] in {101, 102, 104, 108, 109, 110, 128, 146, 154} and points:
            gap = min(math.dist((obj["x"], obj["y"]), p) for p in points)
            if gap >= 50:
                result.append(f"Object {obj['id']}: nearest traffic point is {gap:.1f} units away; verify its approach.")
    mission = source.get("mission")
    if mission:
        supported = {2, 3, 4, 5, 6, 18, 101, 102, 104, 105, 108, 109, 110, 113, 114, 128, 146, 151, 152, 153, 154}
        other = sorted({o["type"] for o in objects} - supported)
        if other:
            result.append(f"Mission contains types outside the fully supported authoring set: {other}; some are cosmetic or ignored.")
        bases = [o for o in objects if o["type"] == 110]
        recruits = sum(bool(bases) and math.dist((o["x"], o["y"]), (bases[0]["x"], bases[0]["y"])) >= 80 for o in objects if o["type"] == 104)
        people = sum(o["type"] == 109 for o in objects) + recruits
        if mission["rescue_goal"] != people:
            result.append(f"rescue_goal is {mission['rescue_goal']}; scene contains {people} civilians plus stranded recruits in total.")
        if mission["basin_goal"] > sum(o["type"] == 128 for o in objects):
            result.append("basin_goal exceeds the number of valves.")
        if sum(o["type"] == 104 for o in objects) == recruits:
            result.append("Every worker is stranded; place a starting worker within 80 units of the first base.")
        for obj in objects:
            if obj["type"] in {104, 105, 109} and obj.get("extra") in {"heli", "roof", "boat", "sick"}:
                result.append(f"Object {obj['id']}: {obj['extra']} rescue behavior is not implemented for custom missions; use on-foot or supported ATV recruitment.")
        if sum(o["type"] == 104 for o in objects) > 64 or sum(o["type"] == 109 for o in objects) > 64:
            result.append("Runtime limit exceeded: maximum 64 workers and 64 civilian evacuees.")
        if sum(o["type"] in {113, 153} for o in objects) > 16:
            result.append("Runtime limit exceeded: maximum 16 vehicles.")
    return result


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


def copy_background(source, input_path, output_path):
    if source["background"].startswith("@"):
        name = source["background"][1:]
        require(bool(name) and "/" not in name and "\\" not in name and Path(name).suffix.lower() in {".png", ".jpg", ".jpeg"}, "custom background must be @FILENAME.png or .jpg beside the source")
        origin, target = input_path.parent / name, output_path.parent / name
        data = origin.read_bytes()  # Validate the dependency even when already in place.
        if origin.resolve() != target.resolve():
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name, help_text in (("compile", "JSON source to v301 binary and optional .rules"), ("decompile", "binary to lossless JSON source")):
        sub = commands.add_parser(name, help=help_text)
        sub.add_argument("input", type=Path)
        sub.add_argument("output", type=Path)
    sub = commands.add_parser("check", help="validate source and warn about likely design problems")
    sub.add_argument("input", type=Path)
    sub = commands.add_parser("analyze", help="round-trip and inventory all original binaries")
    sub.add_argument("directory", type=Path)
    sub.add_argument("output", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "compile":
            require(args.output.suffix == ".lvl", "compiled output must use .lvl extension")
            require(args.input.resolve() != args.output.resolve(), "input and output must differ")
            source = load_source(args.input)
            data = encode(source)
            # Validate everything before writing either output. Refuse stale mission rules.
            rules = Path(str(args.output) + ".rules")
            require("mission" in source or not rules.exists(), f"stale {rules}: remove it explicitly before compiling a scene without mission rules")
            copy_background(source, args.input, args.output)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(data)
            if "mission" in source:
                rules.write_text(mission_values(source["mission"]), encoding="utf-8")
            print(f"Wrote {args.output}: {len(data)} bytes, {len(source['objects'])} objects")
        elif args.command == "decompile":
            require(args.input.resolve() != args.output.resolve(), "input and output must differ")
            source = decode(args.input.read_bytes())
            # Sidecars belong to this restoration, never guessed for original levels.
            rules = Path(str(args.input) + ".rules")
            if rules.exists():
                source["mission"] = parse_rules(rules.read_text(encoding="utf-8"))
            copy_background(source, args.input, args.output)
            write_json(args.output, source)
            print(f"Wrote {args.output}")
        elif args.command == "check":
            source = load_source(args.input)
            notes = warnings(source)
            print(f"Valid v301 source: {len(source['objects'])} objects; {len(notes)} design warnings")
            for note in notes:
                print("warning:", note)
        else:
            report = analyze(args.directory)
            write_json(args.output, report)
            print(f"Byte-identical round trip: {report['roundtripped_files']} files; report: {args.output}")
        return 0
    except (ValueError, OSError, UnicodeError, RecursionError) as error:
        print(f"level-source: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
