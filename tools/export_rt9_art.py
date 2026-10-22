#!/usr/bin/env python3
"""Organize decoded RT9 textures, sprite samples and pixel-size inventories."""
import csv
import html
import json
from pathlib import Path
import re
import shutil
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'reference/rescue-team-9/export'
OUT = ROOT / 'reference/rescue-team-9/art'


def safe(name):
    return re.sub(r'[^\w.-]', '_', name)


def inventory(name, rows):
    with (OUT / name).open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main():
    manifest = json.loads((SOURCE / 'manifest.json').read_text())
    for folder in ['backgrounds', 'atlases', 'sprites', 'thumbs']:
        (OUT / folder).mkdir(parents=True, exist_ok=True)
    cards = {name: [] for name in ['backgrounds', 'atlases', 'sprites']}
    texture_rows, frame_rows = [], []
    for name, texture in sorted(manifest['textures'].items()):
        category = 'backgrounds' if name.startswith('levelback') else 'atlases'
        path = f'{category}/{safe(name)}.png'
        shutil.copyfile(SOURCE / texture['file'], OUT / path)
        texture_rows.append({'name': name, 'width': texture['width'], 'height': texture['height'], 'category': category, 'file': path})
        cards[category].append((name, texture['width'], texture['height'], path))
    for atlas_name, atlas in manifest['atlases'].items():
        texture = manifest['textures'].get(atlas_name)
        if not texture:
            continue
        with Image.open(SOURCE / texture['file']) as sheet:
            for obj in atlas['objects']:
                if obj.get('composite') or any(x in obj['name'] for x in ['$bounds', '$hotzone', '$shadow']):
                    continue
                sequences = [s for s in obj.get('sequences', []) if s.get('frames')]
                if not sequences:
                    continue
                representative = next((s for s in sequences if s['name'].startswith('idle')), sequences[0])
                sample_path = None
                sample_size = None
                for seq in sequences:
                    for index, frame in enumerate(seq['frames']):
                        width = round(frame['cu'] * sheet.width)
                        height = round(frame['cv'] * sheet.height)
                        rotated = bool(frame.get('r'))
                        pixel_width, pixel_height = (height, width) if rotated else (width, height)
                        frame_rows.append({'object': obj['name'], 'sequence': seq['name'], 'frame': index,
                                           'width_px': pixel_width, 'height_px': pixel_height,
                                           'world_width': frame['w'], 'world_height': frame['h'],
                                           'rotated_in_atlas': rotated, 'atlas': atlas_name})
                        if seq is representative and index == 0 and width > 0 and height > 0:
                            x = round(frame['u'] * sheet.width)
                            y = round((1 - frame['v'] - frame['cv']) * sheet.height)
                            image = sheet.crop((x, y, x + width, y + height))
                            if rotated:
                                image = image.transpose(Image.Transpose.ROTATE_90)
                            sample_path = f'sprites/{safe(atlas_name)}--{safe(obj["name"])}.png'
                            image.save(OUT / sample_path)
                            sample_size = image.size
                if sample_path:
                    cards['sprites'].append((obj['name'], *sample_size, sample_path))
    inventory('texture-sizes.csv', texture_rows)
    inventory('sprite-frame-sizes.csv', frame_rows)
    sections = []
    for category, entries in cards.items():
        items = []
        for i, (name, width, height, path) in enumerate(entries):
            with Image.open(OUT / path) as image:
                image.thumbnail((280, 180))
                thumb = f'thumbs/{category}-{i}.png'
                image.save(OUT / thumb)
            items.append(f'<a href="{path}"><img loading="lazy" src="{thumb}"><b>{html.escape(name)}</b><span>{width} × {height} pixels</span></a>')
        sections.append(f'<section id="{category}"><h2>{category.title()} ({len(entries)})</h2><div class="grid">'+''.join(items)+'</div></section>')
    (OUT / 'index.html').write_text('''<!doctype html><meta charset="utf-8"><title>RT9 exported art</title>
<style>body{font:15px system-ui;background:#17232b;color:#edf4f7;margin:24px}a{color:inherit}nav{display:flex;gap:24px;position:sticky;top:0;background:#17232b;padding:16px}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:16px}.grid a{display:flex;flex-direction:column;align-items:center;justify-content:end;gap:8px;padding:12px;background:#293b46;text-decoration:none;overflow-wrap:anywhere}.grid img{max-width:100%;background:repeating-conic-gradient(#66717a 0% 25%,#46515a 0% 50%) 0/16px 16px}span{color:#b7cbd6}</style>
<h1>Rescue Team 9 · exported art</h1><p>Original decoded PNG textures. Click any image for its full-resolution file. Sprite samples show one authored frame per non-composite object; all frame dimensions are in the CSV.</p>
<nav><a href="#backgrounds">Backgrounds</a><a href="#atlases">Texture atlases</a><a href="#sprites">Sprite samples</a><a href="texture-sizes.csv">Texture sizes CSV</a><a href="sprite-frame-sizes.csv">All sprite frame sizes CSV</a></nav>'''+''.join(sections))
    # A compact visual overview for quick inspection outside the browser.
    entries = cards['backgrounds'][:24]
    contact = Image.new('RGB', (1200, 6 * 190), '#17232b')
    draw = ImageDraw.Draw(contact)
    for i, (name, width, height, path) in enumerate(entries):
        with Image.open(OUT / path) as image:
            image.thumbnail((290, 158))
            x, y = (i % 4) * 300, (i // 4) * 190
            contact.paste(image, (x, y))
            draw.text((x + 4, y + 160), f'{name}  {width}x{height}', fill='white')
    contact.save(OUT / 'background-contact-sheet.jpg')
    print(json.dumps({'textures': len(texture_rows), 'sample_sprites': len(cards['sprites']), 'sprite_frames': len(frame_rows), 'gallery': str(OUT / 'index.html')}, indent=2))


if __name__ == '__main__':
    main()
