#!/usr/bin/env python3
"""Check recorded manual road calipers against the shared logical road standard.

This verifies measured evidence, not semantic road detection. Re-measure changed
paintings: hashes prevent accidentally reusing calipers from an older layout.
"""
import hashlib
import json
import math
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def check():
    dimensions = json.loads((ROOT / "assets/road-dimensions.json").read_text())
    measurements = json.loads((ROOT / "levels/road-width-measurements.json").read_text())
    expected = {p.stem.replace(".level", "") for p in (ROOT / "levels").glob("*.level.json")
                if json.loads(p.read_text())["background"].startswith("@")}
    if {entry["level"] for entry in measurements["images"]} != expected:
        raise ValueError("road calipers must cover every custom-painted level")
    target, tolerance = dimensions["road_clear_width"], dimensions["road_width_tolerance"]
    for entry in measurements["images"]:
        path = ROOT / "levels" / (entry["level"] + ".png")
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != entry["sha256"]:
            raise ValueError(f"re-measure changed artwork: {path.name}")
        width, height = struct.unpack(">II", data[16:24])
        if [width, height] != entry["dimensions"] or len(entry["calipers"]) < 2:
            raise ValueError(f"missing measurement evidence: {path.name}")
        sizes = []
        for caliper in entry["calipers"]:
            (x0, y0), (x1, y1) = caliper["endpoints"]
            if not all(0 <= x < width and 0 <= y < height for x, y in [(x0, y0), (x1, y1)]):
                raise ValueError(f"caliper outside image: {path.name}")
            logical = math.hypot((x1-x0)*1366/width, (y1-y0)*768/height)
            if abs(logical-target) > tolerance:
                raise ValueError(f"{path.name}: {caliper['name']} is {logical:.2f}, expected {target}±{tolerance}")
            sizes.append(f"{logical:.2f}")
        print(f"PASS: {entry['level']} sampled clear widths: {', '.join(sizes)} logical pixels")


if __name__ == "__main__":
    try:
        check()
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit(str(error)) from error
