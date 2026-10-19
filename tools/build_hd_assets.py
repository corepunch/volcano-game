#!/usr/bin/env python3
"""Pack authored static sprite states at 2x resolution into a separate asset tree.

Requires Pillow. Art and all damage/repair stages come from ImageGen sheets.
Each authored shadow is combined with its body during packing, so runtime uses
one sprite. Padding preserves body placement and separate interaction bounds.
"""
import argparse
from array import array
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess

from PIL import Image, ImageChops, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
DECL = re.compile(r"texinfo\s*=\s*createTextureInfoEx\('([^']+)'\)")
FRAMES = re.compile(r"frames\s*=\s*\{(.*?)\}\s*(addTextureInfoExNodes\(texinfo, )'([^']+)'(, frames, [\d.]+\))", re.S)
FIELD = re.compile(r"\b(dx|dy|u|v|cu|cv|w|h)\s*=\s*([\d.eE+-]+)")


def anchored_frame(original, pivot, source_box, sprite_size):
    """Map an authored sheet-space pivot through trimming and 2x packing.

    Sheet coordinates use a top-left origin; runtime dx/dy use bottom-left
    world units. Shadow padding is applied afterwards by BakedShadows.
    """
    if pivot is None:
        return original
    left, top, right, bottom = source_box
    if (not isinstance(pivot, list) or len(pivot) != 2 or
            any(type(v) not in (int, float) or not math.isfinite(v) for v in pivot) or
            not (left <= pivot[0] <= right and top <= pivot[1] <= bottom)):
        raise ValueError("sprite pivot must be a finite point inside its source crop")
    bw, bh = sprite_size
    fw, fh = round(original["w"] * 2), round(original["h"] * 2)
    x = (fw - bw) // 2 + (pivot[0] - left) * bw / (right - left)
    y = (fh - bh) // 2 + (pivot[1] - top) * bh / (bottom - top)
    return dict(original, dx=x / 2, dy=(fh - y) / 2)


class BakedShadows:
    """Combine each authored ground footprint with its existing object state.

    Placement uses the final visible body rectangle, rather than the retail
    pivot alone: tall buildings and tiny supplies have different bottom edges.
    Each damage/repair state has its own source art and placement record.
    """

    def __init__(self, art, required):
        manifest = json.loads((art / "shadows.json").read_text())
        if manifest["format"] != "volcano-static-shadows-1":
            raise ValueError("expected authored static ground shadows")
        path = art / manifest["sheet"]
        if hashlib.sha256(path.read_bytes()).hexdigest() != manifest["sha256"]:
            raise ValueError("shadow artwork changed without updating its manifest")
        self.source = Image.open(path).convert("RGBA")
        if list(self.source.size) != manifest["sheet_size"]:
            raise ValueError("shadow sheet dimensions changed")
        self.entries = {}
        for entry in manifest["frames"]:
            key = entry["animation"], entry["frame"]
            if key in self.entries:
                raise ValueError(f"duplicate authored shadow: {key}")
            left, top, right, bottom = entry["region"]
            if not (0 <= left < right <= self.source.width and
                    0 <= top < bottom <= self.source.height):
                raise ValueError(f"invalid shadow source region: {key}")
            self.entries[key] = entry
        if set(self.entries) != required:
            raise ValueError("every HD object state must have exactly one authored shadow")
    def compose(self, name, index, original, sprite):
        entry = self.entries[name, index]
        footprint = entry["footprint"]
        fw, fh = round(original["w"] * 2), round(original["h"] * 2)
        bw, bh = sprite.size
        body_x, body_y = (fw - bw) // 2, (fh - bh) // 2
        w = max(2, round(bw * footprint["width"]))
        h = max(2, round(bw * footprint["height"]))
        center_x = body_x + bw * (.5 + footprint["offset_x"])
        center_y = body_y + bh - bw * footprint["inset"] + footprint["drop"] * 2
        sx, sy = round(center_x - w / 2), round(center_y - h / 2)
        shadow = self.source.crop(entry["region"]).resize((w, h), Image.Resampling.LANCZOS)
        # Bake the same 45-degree northwest projection formerly drawn at runtime.
        # No damage/repair artwork is synthesized: each state has authored inputs.
        cast = 0 if name == "broken_road0" else round(max(3, (original["h"] - original["dy"]) * .5) * 2)
        if cast:
            shadow = shadow.transform((w + cast, h + cast), Image.Transform.AFFINE,
                                      (1, -cast / (h + cast), 0, 0, h / (h + cast), 0),
                                      Image.Resampling.BICUBIC)
        shadow.putalpha(shadow.getchannel("A").point(lambda alpha: round(alpha * .55)))
        left, top = min(0, sx - cast), min(0, sy - cast)
        right, bottom = max(fw, sx + w), max(fh, sy + h)
        combined = Image.new("RGBA", (right - left, bottom - top))
        combined.alpha_composite(shadow, (sx - cast - left, sy - cast - top))
        combined.alpha_composite(sprite, (body_x - left, body_y - top))
        frame = dict(original, dx=original["dx"] - left / 2,
                     dy=original["dy"] + (bottom - fh) / 2,
                     w=combined.width / 2, h=combined.height / 2,
                     body_dx=original["dx"], body_dy=original["dy"],
                     body_w=original["w"], body_h=original["h"])
        return combined, frame, [-left, -top]


def extract_regions(path, regions, threshold, crop_regions=()):
    """Separate touching low-alpha fringes without changing authored colors.

    Recorded regions normally bound connected alpha cores. A three-pixel fringe
    retains antialiasing. Explicit rect crops retain detached authored props.
    """
    image = Image.open(path).convert("RGBA")
    width, height = image.size
    result, core_regions = {}, []
    for region in regions:
        key = tuple(region)
        if key not in crop_regions:
            core_regions.append(region)
            continue
        left, top, right, bottom = key
        if not (0 <= left < right <= width and 0 <= top < bottom <= height):
            raise ValueError(f"invalid explicit source crop: {path.name} {key}")
        sprite = image.crop(key)
        sprite.info["source_box"] = key
        result[key] = sprite
    if not core_regions:
        return result
    alpha = image.getchannel("A")
    pixels = alpha.tobytes()
    labels = array("i", [0]) * (width * height)
    components = {}
    component = 0
    for start, value in enumerate(pixels):
        if value < threshold or labels[start]:
            continue
        component += 1
        labels[start] = component
        stack = [start]
        x0 = x1 = start % width
        y0 = y1 = start // width
        while stack:
            index = stack.pop()
            x, y = index % width, index // width
            x0, x1 = min(x0, x), max(x1, x)
            y0, y1 = min(y0, y), max(y1, y)
            neighbors = []
            if x:
                neighbors.append(index - 1)
            if x + 1 < width:
                neighbors.append(index + 1)
            if y:
                neighbors.append(index - width)
            if y + 1 < height:
                neighbors.append(index + width)
            for neighbor in neighbors:
                if not labels[neighbor] and pixels[neighbor] >= threshold:
                    labels[neighbor] = component
                    stack.append(neighbor)
        components[(x0, y0, x1 + 1, y1 + 1)] = component
    for region in core_regions:
        key = tuple(region)
        if key not in components:
            raise ValueError(f"authored alpha region changed: {path.name} {key}")
        selected = components[key]
        x0, y0, x1, y1 = key
        box = max(0, x0 - 3), max(0, y0 - 3), min(width, x1 + 3), min(height, y1 + 3)
        left, top, right, bottom = box
        mask = Image.new("L", (right - left, bottom - top))
        mask.putdata([
            255 if labels[y * width + x] == selected else 0
            for y in range(top, bottom) for x in range(left, right)
        ])
        mask = mask.filter(ImageFilter.MaxFilter(7))
        sprite = image.crop(box)
        sprite.putalpha(ImageChops.multiply(alpha.crop(box), mask))
        sprite.info["source_box"] = box
        result[key] = sprite
    return result


def build(assets, output, art):
    assets, output, art = assets.resolve(), output.resolve(), art.resolve()
    if assets == output or assets in output.parents or output in assets.parents:
        raise ValueError("HD output must be separate from the original assets")
    if not (assets / "texture/lazy").is_dir():
        raise ValueError(f"missing original assets: {assets}")
    manifest = json.loads((art / "atlas.json").read_text())
    if manifest["format"] != "volcano-static-atlas-1" or manifest["scale"] != 2:
        raise ValueError("expected the authored 2x static atlas manifest")
    selected = defaultdict(dict)
    sheets = defaultdict(list)
    crop_regions = defaultdict(set)
    for entry in manifest["frames"]:
        name, index = entry["animation"], entry["frame"]
        if index in selected[name]:
            raise ValueError(f"duplicate frame: {name}[{index}]")
        selected[name][index] = entry
        sheets[entry["sheet"]].append(entry["region"])
        extraction = entry.get("extraction", "component")
        if extraction not in ("component", "rect"):
            raise ValueError(f"unknown extraction mode: {extraction}")
        if extraction == "rect":
            crop_regions[entry["sheet"]].add(tuple(entry["region"]))
    extracted = {
        sheet: extract_regions(art / sheet, regions, manifest["alpha_core_threshold"], crop_regions[sheet])
        for sheet, regions in sheets.items()
    }
    shadows = BakedShadows(art, {(name, index) for name, states in selected.items() for index in states})
    atlas = Image.new("RGBA", (2048, 2048))
    cursor_x = cursor_y = row_height = 4
    replacements, report = {}, []
    for path in sorted((assets / "texture/lazy").glob("*.lua")):
        raw = path.read_bytes()
        encoding = "utf-16" if raw.startswith((b"\xff\xfe", b"\xfe\xff")) else "utf-8-sig"
        source = raw.decode(encoding)
        matches = list(DECL.finditer(source))
        edits = []
        for i, declaration in enumerate(matches):
            name = declaration[1]
            if name not in selected:
                continue
            end = matches[i + 1].start() if i + 1 < len(matches) else len(source)
            section = source[declaration.end():end]
            table = FRAMES.search(section)
            if table is None:
                raise ValueError(f"missing frame table: {name}")
            original = [{k: float(v) for k, v in FIELD.findall(row)}
                        for row in re.findall(r"\{([^{}]+)\}", table[1])]
            if set(selected[name]) != set(range(len(original))):
                raise ValueError(f"every original state must be authored: {name}")
            rows = []
            for index, frame in enumerate(original):
                entry = selected[name][index]
                sprite = extracted[entry["sheet"]][tuple(entry["region"])]
                source_box = sprite.info["source_box"]
                w, h = round(frame["w"] * 2), round(frame["h"] * 2)
                ratio = min((w - 2) / sprite.width, (h - 2) / sprite.height)
                if ratio > 1:
                    raise ValueError(f"source artwork too small for true 2x: {name}[{index}]")
                sprite = sprite.resize((max(1, round(sprite.width * ratio)),
                                        max(1, round(sprite.height * ratio))), Image.Resampling.LANCZOS)
                frame = anchored_frame(frame, entry.get("pivot"), source_box, sprite.size)
                sprite, padded, body_offset = shadows.compose(name, index, frame, sprite)
                w, h = sprite.size
                if cursor_x + w + 4 > atlas.width:
                    cursor_x, cursor_y = 4, cursor_y + row_height + 4
                    row_height = 0
                if cursor_y + h + 4 > atlas.height:
                    raise ValueError("packed atlas is full")
                atlas.alpha_composite(sprite, (cursor_x, cursor_y))
                packed = dict(padded, u=cursor_x / atlas.width,
                              v=1 - (cursor_y + h) / atlas.height,
                              cu=w / atlas.width, cv=h / atlas.height)
                rows.append("  { " + ", ".join(f"{k}={v:.10g}" for k, v in packed.items()) + " },")
                report.append(dict(animation=name, frame=index,
                                   original_size=[frame["w"], frame["h"]],
                                   texture_size=[w, h],
                                   pivot=[padded["dx"], padded["dy"]],
                                   body_pivot=[frame["dx"], frame["dy"]],
                                   source_pivot=entry.get("pivot"),
                                   body_offset=body_offset, shadow="baked-northwest-45",
                                   atlas_region=[cursor_x, cursor_y, w, h]))
                cursor_x += w + 4
                row_height = max(row_height, h)
            replacement = "frames =\n{\n" + "\n".join(rows) + "\n}\n" + table[2] + "'volcano-static'" + table[4]
            edits.append((declaration.end() + table.start(), declaration.end() + table.end(), replacement))
        if edits:
            for start, end, replacement in reversed(edits):
                source = source[:start] + replacement + source[end:]
            replacements[path.name] = source.encode(encoding)
    if len(report) != len(manifest["frames"]):
        raise ValueError("some authored frames have no matching original declaration")
    output.mkdir(parents=True, exist_ok=True)
    # Always refresh the declarations before applying replacements: repeated
    # builds and an updated ASSETS path cannot accumulate duplicate sprites.
    subprocess.run(["rsync", "-a", "--exclude=.DS_Store", str(assets) + "/", str(output) + "/"], check=True)
    folder = output / "texture/lazy"
    # Migrate previously built HD trees; there are no shadow-only animations now.
    for stale in (folder / "volcano-static-shadows.lua", folder / "volcano-static-shadows.png",
                  output / "hd-static-shadows-report.json"):
        stale.unlink(missing_ok=True)
    for name, content in replacements.items():
        (folder / name).write_bytes(content)
    atlas.save(folder / "volcano-static.png")
    (output / "hd-static-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"HD assets: {len(selected)} static sprites, {len(report)} authored states at exact 2x texture resolution.")
    print(f"Ground shadows: {len(report)} authored footprints baked into their object frames; no separate layer.")
    print(f"Authored pivots applied to artwork and click bounds; frame order and FPS preserved. Output: {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, default=ROOT / "reference/rescue-team")
    parser.add_argument("--output", type=Path, default=ROOT / "build/hd-assets")
    parser.add_argument("--art", type=Path, default=ROOT / "assets/hd-static")
    args = parser.parse_args()
    try:
        build(args.assets, args.output, args.art)
    except (ValueError, OSError, KeyError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
