#!/usr/bin/env python3
"""Pack Blender-rendered rescue characters and install their sprite declarations.

Pack once after rendering: python3 tools/pack_rescue_characters.py --pack
Install (retail source remains untouched):
  python3 tools/pack_rescue_characters.py --assets build/hd-assets --output build/hd-assets
Only PNG packing, alpha trimming and declaration mapping happen here; all
character artwork and motion comes from the editable Blender scene.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageStat

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'assets/kenney-characters'
DECL=re.compile(r"texinfo\s*=\s*createTextureInfoEx\('([^']+)'\)")
TABLE=re.compile(r"frames\s*=\s*\{(.*?)\}\s*addTextureInfoExNodes\(texinfo,\s*'[^']+',\s*frames,\s*([\d.]+)\)",re.S)


def pack():
    manifest=json.loads((ART/'render-manifest.json').read_text())
    (ART/'sprites').mkdir(exist_ok=True)
    clips=[]
    for clip in manifest['clips']:
        width,height=clip.get('resolution',manifest['resolution'])
        count=clip['frames'];frames=[];sprites=[]
        cursor_x=cursor_y=2;row_height=0;atlas_width=1024
        for direction in range(8):
            row=[]
            for index in range(count):
                path=ROOT/clip['folder']/f'{direction}-{index:03}.png'
                with Image.open(path) as src:image=src.convert('RGBA')
                if image.size!=(width,height):raise ValueError(f'wrong render size: {path}')
                box=image.getchannel('A').getbbox()
                if box is None:raise ValueError(f'empty render: {path}')
                opaque=image.getchannel('A').point(lambda a:255 if a>128 else 0)
                if max(ImageStat.Stat(image.convert('RGB'),opaque).mean)<10:
                    raise ValueError(f'unlit or missing-texture render: {path}')
                if box[0]==0 or box[1]==0 or box[2]==width or box[3]==height:
                    raise ValueError(f'clipped character: {path} {box}')
                left,top,right,bottom=box;scale=manifest['texture_scale'];ax,ay=clip['anchor']
                w,h=right-left,bottom-top
                if cursor_x+w+2>atlas_width:
                    cursor_x=2;cursor_y+=row_height+2;row_height=0
                x,y=cursor_x,cursor_y
                sprites.append((image.crop(box),(x,y)))
                row.append(dict(dx=(ax-left)/scale,dy=(bottom-ay)/scale,
                    w=w/scale,h=h/scale,u=x,v=y,cu=w,cv=h))
                cursor_x+=w+2;row_height=max(row_height,h)
            frames.append(row)
        atlas_height=1 << (cursor_y+row_height+2-1).bit_length()
        atlas=Image.new('RGBA',(atlas_width,atlas_height))
        for sprite,position in sprites:atlas.alpha_composite(sprite,position)
        for row in frames:
            for f in row:
                f['u']/=atlas_width;f['v']=1-(f['v']+f['cv'])/atlas_height
                f['cu']/=atlas_width;f['cv']/=atlas_height
        texture='kenney-'+clip['character']+'-'+clip['action'].lower()
        dest=ART/'sprites'/(texture+'.png');atlas.save(dest,optimize=True)
        clips.append(dict(character=clip['character'],action=clip['action'],count=count,texture=texture,
                          size=list(atlas.size),frames=frames,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
    catalog=dict(format='volcano-kenney-sprites-1',scale=manifest['texture_scale'],clips=clips)
    (ART/'sprites/catalog.json').write_text(json.dumps(catalog,separators=(',',':'))+'\n')
    print(f'Packed {len(clips)} clips into eight-direction transparent PNG atlases.')
    make_previews(manifest)


def make_previews(manifest):
    workers=[c for c in manifest['clips'] if c['character']=='worker']
    sheet=Image.new('RGB',(4*128+170,len(workers)*128),'#e9e5db');draw=ImageDraw.Draw(sheet)
    for row,clip in enumerate(workers):
        draw.text((12,row*128+14),clip['action'],fill='#172f35')
        for col,index in enumerate([0,clip['frames']//4,clip['frames']//2,3*clip['frames']//4]):
            with Image.open(ROOT/clip['folder']/f'7-{index:03}.png') as src:image=src.convert('RGBA')
            image.thumbnail((128,128))
            sheet.paste(image,(170+col*128,row*128),image)
    sheet.save(ART/'previews/worker-actions.jpg',quality=94)
    # Compact in-game-scale directional review of running and hammering.
    sheet=Image.new('RGB',(8*160,3*160),'#e9e5db');draw=ImageDraw.Draw(sheet)
    for row,name in enumerate(['Stand','Run','Hammer']):
        for direction in range(8):
            folder=ROOT/'build/kenney-character-frames'/('worker-'+name)
            with Image.open(folder/f'{direction}-000.png') as src:image=src.convert('RGBA')
            sheet.paste(image,(direction*160+(160-image.width)//2,row*160),image)
            draw.text((direction*160+8,row*160+4),name+' '+manifest['directions'][direction],fill='#172f35')
    sheet.save(ART/'previews/worker-directions.jpg',quality=94)
    names=['Stand','Run','Hammer','Chop','Gather','Carry'];animation=[]
    font_path=Path('/System/Library/Fonts/Supplemental/Arial.ttf')
    font=ImageFont.truetype(str(font_path),17) if font_path.exists() else ImageFont.load_default()
    small=ImageFont.truetype(str(font_path),13) if font_path.exists() else ImageFont.load_default()
    lookup={c['action']:c for c in workers}
    for tick in range(48):
        canvas=Image.new('RGB',(600,426),'#e9e5db');draw=ImageDraw.Draw(canvas)
        draw.text((16,10),'KENNEY / RESCUE WORKER',font=font,fill='#19343c')
        draw.text((16,34),'Blender motions / eight directions / 2x sprites',font=small,fill='#4b6267')
        for j,name in enumerate(names):
            x=(j%3)*200;y=62+(j//3)*182;clip=lookup[name];index=tick%clip['frames']
            with Image.open(ROOT/clip['folder']/f'7-{index:03}.png') as src:image=src.convert('RGBA')
            canvas.paste(image,(x+(200-image.width)//2,y+8),image)
            draw.text((x+14,y+160),name,font=font,fill='#19343c')
        animation.append(canvas)
    palette=animation[0].quantize(colors=192)
    animation=[f.quantize(palette=palette,dither=Image.Dither.NONE) for f in animation]
    animation[0].save(ART/'previews/rescue-worker.gif',save_all=True,append_images=animation[1:],
                      duration=[40,40,50]*16,loop=0,optimize=True)


def mapping(name):
    if name.endswith('_shadow'):return None
    match=re.fullmatch(r'(worker|man1?|wman1?)_(\w+?)(\d?)',name)
    if not match:return None
    family,kind,digit=match.groups();direction=int(digit or 0)
    character='worker' if family=='worker' else 'female' if family.startswith('wman') else 'male'
    actions=dict(wait='Stand',run='Run',bag='Carry',wood='Chop',gather='Gather',
        srepair='Hammer',repair='Repair',pick='Pickaxe',tap='Valve',call='Call',
        pet='Pet',sit='Sit',ill='Ill',ill_alert='Alert',sink='Sink',idle='Stand')
    if kind not in actions:raise ValueError(f'unmapped character animation {name}')
    action=actions[kind]
    if kind in ['wait','idle']:
        action='Look' if direction==1 else 'Stand';direction=6 if direction<2 else 7
    elif kind in ['ill','ill_alert','sink','sit']:direction=7 if direction==0 else 5
    return character,action,direction


def install(assets,output):
    assets=assets.resolve();output=output.resolve()
    if output==ROOT/'reference/rescue-team' or (ROOT/'reference/rescue-team') in output.parents:
        raise ValueError('never overwrite retail reference assets')
    if assets!=output:
        if assets in output.parents or output in assets.parents:raise ValueError('asset trees must not nest')
        output.mkdir(parents=True,exist_ok=True)
        subprocess.run(['rsync','-a','--exclude=.DS_Store',str(assets)+'/',str(output)+'/'],check=True)
    catalog=json.loads((ART/'sprites/catalog.json').read_text())
    for clip in catalog['clips']:
        path=ART/'sprites'/(clip['texture']+'.png')
        if hashlib.sha256(path.read_bytes()).hexdigest()!=clip['sha256']:
            raise ValueError(f'character atlas changed without repacking its catalog: {path}')
    lookup={(c['character'],c['action']):c for c in catalog['clips']}
    folder=output/'texture/lazy';changes=[]
    # Read untouched declarations for repeatable runs, even when the destination
    # already includes our overrides. Shadows and unrelated characters stay intact.
    for original in sorted((ROOT/'reference/rescue-team/texture/lazy').glob('*.lua')):
        if original.name not in ['worker0.lua','man00.lua','man01.lua','man10.lua','man11.lua',
                                 'wman00.lua','wman01.lua','wman10.lua','wman11.lua']:
            continue
        source=original.read_text(encoding='utf-8-sig')
        declarations=list(DECL.finditer(source));edits=[]
        for i,decl in enumerate(declarations):
            name=decl[1];mapped=mapping(name)
            if mapped is None:continue
            character,action,direction=mapped;clip=lookup[character,action]
            end=declarations[i+1].start() if i+1<len(declarations) else len(source)
            table=TABLE.search(source,decl.end(),end)
            if table is None:raise ValueError(f'missing character frames: {name}')
            original_count=len(re.findall(r'\{[^{}]*\}',table[1]));rows=[]
            for j in range(original_count):
                frame=clip['frames'][direction][(j*clip['count'])//original_count]
                rows.append('  { '+', '.join(f'{k}={v:.10g}' for k,v in frame.items())+' },')
            replacement='frames =\n{\n'+'\n'.join(rows)+f"\n}}\naddTextureInfoExNodes(texinfo, '{clip['texture']}', frames, {table[2]})"
            edits.append((table.start(),table.end(),replacement))
            changes.append(dict(animation=name,clip=action,character=character,direction=direction,frames=original_count,fps=float(table[2])))
        if edits:
            for start,end,replacement in reversed(edits):source=source[:start]+replacement+source[end:]
            (folder/original.name).write_text(source)
    for path in (ART/'sprites').glob('*.png'):shutil.copy2(path,folder/path.name)
    shutil.copy2(ART/'source/survivors/License.txt',output/'Kenney-characters-LICENSE.txt')
    shutil.copy2(ART/'source/provenance.json',output/'Kenney-characters-source.json')
    (output/'kenney-characters-report.json').write_text(json.dumps(changes,indent=2)+'\n')
    print(f'Installed {len(changes)} character animations; retail frame counts and FPS preserved. {output}')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pack',action='store_true')
    parser.add_argument('--assets',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.pack:pack()
    if args.assets or args.output:
        if not args.assets or not args.output:parser.error('--assets and --output are required together')
        install(args.assets,args.output)
    elif not args.pack:parser.error('use --pack or --assets/--output')
