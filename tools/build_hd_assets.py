#!/usr/bin/env python3
"""Pack authored static sprite states at 2x resolution into a separate asset tree.

Requires Pillow. Art and all damage/repair stages come from ImageGen sheets;
only extraction, proportional downsampling and atlas packing happen here.
Original declarations supply unchanged world sizes, pivots, FPS and frame order.
"""
import argparse
from array import array
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess

from PIL import Image, ImageChops, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
DECL = re.compile(r"texinfo\s*=\s*createTextureInfoEx\('([^']+)'\)")
FRAMES = re.compile(r"frames\s*=\s*\{(.*?)\}\s*(addTextureInfoExNodes\(texinfo, )'([^']+)'(, frames, [\d.]+\))", re.S)
FIELD = re.compile(r"\b(dx|dy|u|v|cu|cv|w|h)\s*=\s*([\d.eE+-]+)")


class ShadowAtlas:
    """Pack authored ground footprints without touching the object artwork.

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
        self.image = Image.new("RGBA", (2048, 1024))
        self.x = self.y = self.row_height = 4
        self.rows = defaultdict(list)
        self.suffixes = {}
        self.report = []

    def add(self, name, index, original, sprite_size, suffix):
        entry = self.entries[name, index]
        footprint = entry["footprint"]
        # These are the same integer placements used for the body atlas below.
        fw, fh = round(original["w"] * 2), round(original["h"] * 2)
        bw, bh = sprite_size
        body_left = -original["dx"] + ((fw - bw) // 2) / 2
        body_bottom = -original["h"] + original["dy"] + ((fh - bh) // 2 + bh) / 2
        body_width = bw / 2
        w = max(2, round(body_width * footprint["width"] * 2))
        h = max(2, round(body_width * footprint["height"] * 2))
        center_x = body_left + body_width * (.5 + footprint["offset_x"])
        center_y = body_bottom - body_width * footprint["inset"] + footprint["drop"]
        if self.x + w + 4 > self.image.width:
            self.x, self.y = 4, self.y + self.row_height + 4
            self.row_height = 0
        if self.y + h + 4 > self.image.height:
            raise ValueError("packed shadow atlas is full")
        shadow = self.source.crop(entry["region"]).resize((w, h), Image.Resampling.LANCZOS)
        self.image.alpha_composite(shadow, (self.x, self.y))
        frame = dict(dx=w / 4 - center_x, dy=center_y + h / 4,
                     w=w / 2, h=h / 2, u=self.x / self.image.width,
                     v=1 - (self.y + h) / self.image.height,
                     cu=w / self.image.width, cv=h / self.image.height)
        self.rows[name].append("  { " + ", ".join(f"{k}={v:.10g}" for k, v in frame.items()) + " },")
        self.suffixes[name] = suffix
        self.report.append(dict(animation=name, frame=index, pivot=[frame["dx"], frame["dy"]],
                                world_size=[frame["w"], frame["h"]],
                                atlas_region=[self.x, self.y, w, h]))
        self.x += w + 4
        self.row_height = max(self.row_height, h)

    def save(self, output):
        declarations = []
        for name, rows in self.rows.items():
            declarations.append(f"texinfo = createTextureInfoEx('{name}_ground_shadow')\n"
                                "frames =\n{\n" + "\n".join(rows) + "\n}\n"
                                "addTextureInfoExNodes(texinfo, 'volcano-static-shadows'" +
                                self.suffixes[name] + "\n")
        self.image.save(output / "texture/lazy/volcano-static-shadows.png")
        (output / "texture/lazy/volcano-static-shadows.lua").write_text("\n".join(declarations))
        (output / "hd-static-shadows-report.json").write_text(json.dumps(self.report, indent=2) + "\n")


def extract_regions(path, regions, threshold):
    """Separate touching low-alpha fringes without changing authored colors.

    Recorded regions bound connected alpha cores. A three-pixel fringe retains
    the generated antialiasing; it excludes nearby sprites in the source sheet.
    """
    image = Image.open(path).convert("RGBA")
    width, height = image.size
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
    result = {}
    for region in regions:
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
    for entry in manifest["frames"]:
        name, index = entry["animation"], entry["frame"]
        if index in selected[name]:
            raise ValueError(f"duplicate frame: {name}[{index}]")
        selected[name][index] = entry
        sheets[entry["sheet"]].append(entry["region"])
    extracted = {
        sheet: extract_regions(art / sheet, regions, manifest["alpha_core_threshold"])
        for sheet, regions in sheets.items()
    }
    shadows = ShadowAtlas(art, {(name, index) for name, states in selected.items() for index in states})
    atlas = Image.new("RGBA", (2048, 1024))
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
                w, h = round(frame["w"] * 2), round(frame["h"] * 2)
                ratio = min((w - 2) / sprite.width, (h - 2) / sprite.height)
                if ratio > 1:
                    raise ValueError(f"source artwork too small for true 2x: {name}[{index}]")
                sprite = sprite.resize((max(1, round(sprite.width * ratio)),
                                        max(1, round(sprite.height * ratio))), Image.Resampling.LANCZOS)
                shadows.add(name, index, frame, sprite.size, table[4])
                if cursor_x + w + 4 > atlas.width:
                    cursor_x, cursor_y = 4, cursor_y + row_height + 4
                    row_height = 0
                if cursor_y + h + 4 > atlas.height:
                    raise ValueError("packed atlas is full")
                atlas.alpha_composite(sprite, (cursor_x + (w - sprite.width) // 2,
                                               cursor_y + (h - sprite.height) // 2))
                packed = dict(frame, u=cursor_x / atlas.width,
                              v=1 - (cursor_y + h) / atlas.height,
                              cu=w / atlas.width, cv=h / atlas.height)
                rows.append("  { " + ", ".join(f"{k}={v:.10g}" for k, v in packed.items()) + " },")
                report.append(dict(animation=name, frame=index,
                                   original_size=[frame["w"], frame["h"]],
                                   texture_size=[w, h],
                                   pivot=[frame["dx"], frame["dy"]],
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
    for name, content in replacements.items():
        (folder / name).write_bytes(content)
    atlas.save(folder / "volcano-static.png")
    shadows.save(output)
    (output / "hd-static-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"HD assets: {len(selected)} static sprites, {len(report)} authored states at exact 2x texture resolution.")
    print(f"Ground shadows: {len(shadows.report)} authored footprints, with matching state order and FPS.")
    print(f"World sizes, pivots, frame order and all other declarations preserved. Output: {output}")


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
