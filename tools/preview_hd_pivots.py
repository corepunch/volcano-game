#!/usr/bin/env python3
"""Render every packed HD static frame at 2x with its actual runtime pivot."""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import zipfile

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CELL = (540, 520)
ORIGIN = (270, 300)
FOOTER = 473
PINK = "#ff6ba8"
CYAN = "#80d9ed"


def font(size):
    for name in ("/System/Library/Fonts/Supplemental/Arial.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def card(frame, atlas, highlight):
    image = Image.new("RGBA", CELL, "#252e37")
    draw = ImageDraw.Draw(image)
    for x in range(0, CELL[0], 20):
        draw.line((x, 54, x, FOOTER - 1), fill="#303b46")
    for y in range(60, FOOTER, 20):
        draw.line((0, y, CELL[0], y), fill="#303b46")
    px, py = ORIGIN
    draw.line((0, py, CELL[0], py), fill="#627080")
    draw.line((px, 54, px, 322), fill="#627080")
    width = min(170, max(65, frame["original_size"][0] + 24)) * 2
    height = width * .62
    ring = highlight.resize((round(width), round(height)), Image.Resampling.LANCZOS)
    image.alpha_composite(ring, (round(px-width/2), round(py-height/2)))
    x, y, w, h = frame["atlas_region"]
    if x < 0 or y < 0 or x+w > atlas.width or y+h > atlas.height:
        raise ValueError(f"frame outside packed atlas: {frame['animation']}")
    dx, dy = frame["pivot"]
    left, top = round(px-2*dx), round(py-h+2*dy)
    if left < 0 or top < 54 or left+w > CELL[0] or top+h >= FOOTER:
        raise ValueError(f"preview cell clips {frame['animation']}[{frame['frame']}]")
    image.alpha_composite(atlas.crop((x, y, x+w, y+h)), (left, top))
    draw = ImageDraw.Draw(image)
    bx, by = frame["body_pivot"]
    bw, bh = frame["original_size"]
    draw.rectangle((round(px-2*bx), round(py-2*(bh-by)),
                    round(px+2*(bw-bx)), round(py+2*by)), outline=CYAN)
    # Draw the marker last so the anchor remains visible through the artwork.
    for color, thickness in (("#101820", 5), (PINK, 2)):
        draw.line((px-12, py, px+12, py), fill=color, width=thickness)
        draw.line((px, py-12, px, py+12), fill=color, width=thickness)
        draw.ellipse((px-5, py-5, px+5, py+5), outline=color, width=thickness)
    draw.text((14, 12), f"{frame['animation']}  /  frame {frame['frame']}", font=font(19), fill="white")
    draw.text((14, 35), "Authored pivot" if frame.get("source_pivot") else "Inherited retail pivot", font=font(13), fill="#b8c6d3")
    draw.rectangle((0, FOOTER, CELL[0], CELL[1]), fill="#1c252d")
    draw.text((14, FOOTER + 8), f"Body pivot: ({bx:.2f}, {by:.2f})   Size: {bw:g} x {bh:g}", font=font(14), fill=CYAN)
    draw.text((14, FOOTER + 28), "Bottom-left body coordinates, in world pixels", font=font(12), fill="#b8c6d3")
    return image.convert("RGB")


def sheet(cards, title, path, columns=4):
    header = 88
    rows = math.ceil(len(cards)/columns)
    image = Image.new("RGB", (CELL[0]*columns, header+CELL[1]*rows), "#121b23")
    draw = ImageDraw.Draw(image)
    draw.text((18, 13), title, font=font(27), fill="white")
    draw.text((18, 49), "Pink cross = runtime pivot   |   Cyan = body bounds   |   Green = in-game highlight", font=font(18), fill="#d5e1eb")
    draw.text((18, 72), "All sprites at native 2x resolution; every cross occupies the same ground position within its cell.", font=font(12), fill="#b8c6d3")
    for i, tile in enumerate(cards):
        image.paste(tile, ((i%columns)*CELL[0], header+(i//columns)*CELL[1]))
    image.save(path)


def build(assets, output):
    frames = json.loads((assets/"hd-static-report.json").read_text())
    manifest = json.loads((ROOT/"assets/hd-static/atlas.json").read_text())
    buildings = {e["animation"]: e["category"] for e in
                 json.loads((ROOT/"assets/hd-static/building-parity.json").read_text())["animations"]}
    order = {(e["animation"], e["frame"]): i for i, e in enumerate(manifest["frames"])}
    if len(frames) != len(order) or {(e["animation"], e["frame"]) for e in frames} != set(order):
        raise ValueError("packed report does not cover every authored sprite state")
    frames.sort(key=lambda e: order[e["animation"], e["frame"]])
    atlas = Image.open(assets/"texture/lazy/volcano-static.png").convert("RGBA")
    source = Image.open(assets/"texture/gui/highlight.png").convert("RGBA")
    # Same selected UV rectangle and bottom-left UV convention as game_render.c.
    u, v, cu, cv = .0078125, .140625, .6171875, .421875
    x, y, w, h = round(u*source.width), round((1-v-cv)*source.height), round(cu*source.width), round(cv*source.height)
    highlight = source.crop((x, y, x+w, y+h))
    output.mkdir(parents=True, exist_ok=True)
    groups = defaultdict(list)
    all_cards, paths = [], []
    for frame in frames:
        name = frame["animation"]
        group = ("overlays" if buildings.get(name) == "overlay" else
                 "buildings" if name in buildings else
                 "obstacles" if name.startswith(("bar_", "bridge_", "broken_")) else "supplies")
        tile = card(frame, atlas, highlight)
        path = output/f"{name}-{frame['frame']:02d}.png"
        tile.save(path)
        paths.append(path)
        groups[group].append(tile)
        all_cards.append(tile)
    sheet(all_cards, f"All HD static sprites: {len(frames)} states", output/"all-pivots.png")
    for group, cards in groups.items():
        path = output/f"{group}-pivots.png"
        sheet(cards, f"{group.title()}: {len(cards)} states", path)
        paths.append(path)
    paths.append(output/"all-pivots.png")
    with zipfile.ZipFile(output/"pivot-previews.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for path in paths:
            archive.write(path, path.name)
    print(f"Rendered {len(frames)} states, {len(groups)} category sheets and combined sheet: {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, default=ROOT/"build/hd-assets")
    parser.add_argument("--output", type=Path, default=ROOT/"build/pivot-previews")
    args = parser.parse_args()
    build(args.assets.resolve(), args.output.resolve())
