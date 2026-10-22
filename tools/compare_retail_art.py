#!/usr/bin/env python3
"""Compare locally owned RT5/RT9 source sprites without resizing their pixels."""
import html
import json
from pathlib import Path
import re
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'reference/rt5-rt9-comparison'
RT5 = ROOT / 'reference/rescue-team/texture/lazy'
RT9 = ROOT / 'reference/rescue-team-9/export'


def crop(sheet, frame):
    x = round(frame['u'] * sheet.width)
    y = round((1 - frame['v'] - frame['cv']) * sheet.height)
    w, h = round(frame['cu'] * sheet.width), round(frame['cv'] * sheet.height)
    image = sheet.crop((x, y, x + w, y + h)).convert('RGBA')
    return image.transpose(Image.Transpose.ROTATE_90) if frame.get('r') else image


def rt5(name):
    for path in RT5.glob('*.lua'):
        text = path.read_text()
        match = re.search(r"createTextureInfoEx\('" + re.escape(name) + r"'\)(.*?)(?=texinfo = createTextureInfoEx|\Z)", text, re.S)
        if not match:
            continue
        body = match[1]
        raw = re.search(r'frames\s*=\s*\{\s*\{([^}]+)', body)[1]
        frame = {k: float(v) for k, v in re.findall(r'(\w+)\s*=\s*([-\d.]+)', raw)}
        texture = re.search(r"addTextureInfoExNodes\(texinfo,\s*'([^']+)'", body)[1]
        with Image.open(RT5 / (texture + '.png')) as sheet:
            return crop(sheet, frame)
    raise ValueError(name)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((RT9 / 'manifest.json').read_text())
    objects = {o['name']: (a['atlas'], o) for a in manifest['atlases'].values() for o in a['objects']}
    cases = [('Worker standing', 'worker_wait0', 'worker$base', 'idle0/0'),
             ('Worker running', 'worker_run0', 'worker$base', 'run0/0'),
             ('Rescue base', 'base0', 'rescuebase$base', 'work0'),
             ('Food building', 'burger0', 'burgerhouse$base', 'work1'),
             ('Sawmill', 'sawmill0', 'sawmill$base', 'work1'),
             ('Gas station', 'gas0', 'gasstation$base', 'work1')]
    records, rows = [], []
    font_path = '/System/Library/Fonts/Supplemental/Arial.ttf'
    font = ImageFont.truetype(font_path, 16)
    small = ImageFont.truetype(font_path, 12)
    board = Image.new('RGB', (820, 100 + len(cases) * 215), '#202c34')
    draw = ImageDraw.Draw(board)
    draw.text((24, 15), 'Original source pixels | RT5 vs RT9 | no sprite resizing', font=font, fill='white')
    draw.text((24, 42), 'Different drawings and poses; RT9 buildings show base artwork only.', font=small, fill='#b7cbd6')
    draw.text((210, 72), 'RT5', font=font, fill='white')
    draw.text((570, 72), 'RT9', font=font, fill='white')
    for index, (label, old_name, new_name, seq_name) in enumerate(cases):
        old = rt5(old_name)
        atlas, obj = objects[new_name]
        frame = next(s for s in obj['sequences'] if s['name'] == seq_name)['frames'][0]
        with Image.open(RT9 / manifest['textures'][atlas]['file']) as sheet:
            new = crop(sheet, frame)
        row_top = 100 + index * 215
        draw.line((24, row_top, 796, row_top), fill='#465862')
        draw.text((24, row_top + 8), label, font=font, fill='white')
        cells = []
        for version, image, name, sequence, cx in [('rt5', old, old_name, 'frame 0', 230), ('rt9', new, new_name, seq_name, 590)]:
            file = f'{index}-{version}.png'
            image.save(OUT / file)
            # Composite at exactly 1:1; neither artwork nor its alpha is resampled.
            board.paste(image, (cx - image.width // 2, row_top + 177 - image.height), image)
            draw.text((cx - 80, row_top + 183), f'{image.width} x {image.height} px', font=font, fill='white')
            cells.append(f'<div><div class="art"><img src="{file}" width="{image.width}" height="{image.height}"></div><b>{image.width} × {image.height} px</b><small>{html.escape(name)} · {html.escape(sequence)}</small></div>')
        records.append({'label': label, 'rt5': list(old.size), 'rt9': list(new.size), 'rt5_sprite': old_name, 'rt9_sprite': new_name, 'rt9_sequence': seq_name})
        rows.append('<section><h2>'+label+'</h2><div class="pair">'+''.join(cells)+'</div></section>')
    board.save(OUT / 'comparison.png')
    (OUT / 'sizes.json').write_text(json.dumps(records, indent=2))
    (OUT / 'index.html').write_text('''<!doctype html><meta charset="utf-8"><title>RT5 vs RT9 · original sprite pixels</title><style>body{background:#202c34;color:#edf4f7;font:16px system-ui;margin:24px;min-width:760px}main{width:760px}h1{font-size:24px}p{color:#b7cbd6}.pair,.head{display:grid;grid-template-columns:380px 380px;text-align:center}.art{height:160px;display:flex;align-items:end;justify-content:center;padding-bottom:8px}.art img{max-width:none;flex:none}section{border-top:1px solid #465862;padding:10px 0}h2{font-size:16px;margin:0}small{display:block;color:#b7cbd6;font-size:12px;margin-top:5px}a{color:#a6dcff}</style><main><h1>RT5 vs RT9 · original sprite pixels</h1><p>One source pixel per CSS pixel at 100% browser zoom. No sprite resizing.<br>Different drawings and poses; RT9 buildings show base artwork without separate overlays.<br>Screen Retina scaling is handled by the browser, so compare source dimensions below.</p><p><a href="comparison.png">Download comparison PNG</a> · <a href="sizes.json">Exact sizes and source names</a></p><div class="head"><b>Rescue Team 5</b><b>Rescue Team 9</b></div>'''+''.join(rows)+'</main>')
    print(json.dumps(records, indent=2))


if __name__ == '__main__':
    main()
