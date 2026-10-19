#!/usr/bin/env python3
"""Verify every retail building frame is actually packed and keeps its timing."""
import argparse
import json
from pathlib import Path
import re

from PIL import Image

from build_hd_assets import DECL, FIELD, FRAMES, ROOT, building_inventory


def declarations(path):
    raw = path.read_bytes()
    source = raw.decode("utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig")
    result = {}
    names = list(DECL.finditer(source))
    for i, name in enumerate(names):
        end = names[i + 1].start() if i + 1 < len(names) else len(source)
        table = FRAMES.search(source[name.end():end])
        if table is None:
            raise ValueError(f"missing frame table: {name[1]}")
        result[name[1]] = (table[3], table[4],
                          [{k: float(v) for k, v in FIELD.findall(row)}
                           for row in re.findall(r"\{([^{}]+)\}", table[1])])
    return result


def check(assets, output):
    inventory = building_inventory(assets)
    frames = json.loads((output / "hd-static-report.json").read_text())
    report = {(e["animation"], e["frame"]): e for e in frames}
    if len(report) != len(frames):
        raise ValueError("duplicate packed frame records")
    atlas = Image.open(output / "texture/lazy/volcano-static.png").convert("RGBA")
    checked = 0
    for filename in sorted({e["declaration"] for e in inventory}):
        original = declarations(assets / "texture/lazy" / filename)
        packed = declarations(output / "texture/lazy" / filename)
        if original.keys() != packed.keys():
            raise ValueError(f"building declarations changed: {filename}")
        for name, (_, timing, states) in original.items():
            texture, packed_timing, replacements = packed[name]
            if texture != "volcano-static" or packed_timing != timing or len(states) != len(replacements):
                raise ValueError(f"retail fallback, timing or frame-count mismatch: {name}")
            for index, (old, new) in enumerate(zip(states, replacements)):
                entry = report.get((name, index))
                if entry is None or entry["original_size"] != [old["w"], old["h"]]:
                    raise ValueError(f"missing or resized building body: {name}[{index}]")
                if not entry["source_pivot"] and entry["body_pivot"] != [old["dx"], old["dy"]]:
                    raise ValueError(f"retail pivot changed: {name}[{index}]")
                x, y, w, h = entry["atlas_region"]
                if (w != round(new["w"] * 2) or h != round(new["h"] * 2) or
                        abs(new["u"] * atlas.width - x) > .001 or
                        abs((1-new["v"]-new["cv"]) * atlas.height - y) > .001):
                    raise ValueError(f"incorrect 2x texture coordinates: {name}[{index}]")
                if x < 0 or y < 0 or x+w > atlas.width or y+h > atlas.height:
                    raise ValueError(f"building frame outside atlas: {name}[{index}]")
                alpha = atlas.crop((x, y, x+w, y+h)).getchannel("A")
                if alpha.getbbox() is None or alpha.getextrema()[0] != 0:
                    raise ValueError(f"empty or opaque rectangular sprite: {name}[{index}]")
                checked += 1
    print(f"Building parity verified: {len(inventory)} animations, {checked} authored frames, "
          "original timing and body bounds, 2x coordinates, transparent artwork, no retail fallbacks.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, default=ROOT / "reference/rescue-team")
    parser.add_argument("--output", type=Path, default=ROOT / "build/hd-assets")
    args = parser.parse_args()
    try:
        check(args.assets, args.output)
    except (ValueError, OSError, KeyError) as error:
        parser.error(str(error))
