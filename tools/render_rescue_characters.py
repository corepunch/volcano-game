"""Render editable rescue actions into eight-direction transparent PNG frames.

Execute inside the isolated Blender MCP session after build_rescue_characters.py.
Set RESCUE_PREVIEW_ONLY=True in the execute namespace for a quick motion review.
The renderer is resumable; a fresh build requires deleting its build/ frame cache.
"""
import bpy
import json
import math
import hashlib
import shutil
import time
from pathlib import Path
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

ROOT=Path(__file__).resolve().parents[1] if '__file__' in globals() else Path(bpy.data.filepath).resolve().parents[3]
ART=ROOT/'assets/kenney-characters'
CACHE=ROOT/'build/kenney-character-frames'
CACHE.mkdir(parents=True,exist_ok=True)
fingerprint=hashlib.sha256((ART/'blender/rescue-characters.blend').read_bytes()+(ROOT/'tools/render_rescue_characters.py').read_bytes()).hexdigest()
stamp=CACHE/'fingerprint.txt'
if stamp.exists() and stamp.read_text()!=fingerprint:
    shutil.rmtree(CACHE)
    CACHE.mkdir(parents=True)
stamp.write_text(fingerprint)
rig=bpy.data.objects['RescueWorker_Rig'];pivot=bpy.data.objects['Direction_Pivot'];scene=bpy.context.scene
scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True
scene.render.resolution_x=128;scene.render.resolution_y=160;scene.render.resolution_percentage=100
scene.render.film_transparent=True
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
scene.render.use_persistent_data=True
# Retain the native sixteen-frame run; slower work motions use twenty-four or
# thirty-two poses. The asset installer resamples these to retail frame counts.
jobs=[]
for name in ['Stand','Look','Run','Carry','Chop','Gather','Hammer','Repair','Pickaxe','Valve','Call','Pet','Sit','Ill','Alert','Sink','Jump']:
    action=bpy.data.actions['Rescue_'+name]
    count=int(action['frames'])
    jobs.append(dict(character='worker',action=name,frames=count,character_id=0))
for character,ident in [('male',1),('female',2)]:
    for name in ['Stand','Look','Run','Ill','Alert','Sink']:
        jobs.append(dict(character=character,action=name,frames=int(bpy.data.actions['Rescue_'+name]['frames']),character_id=ident))
preview=globals().get('RESCUE_PREVIEW_ONLY',False)
manifest={'format':'volcano-kenney-character-renders-1','resolution':[128,160],'texture_scale':2,'directions':['E','NE','N','NW','W','SW','S','SE'],'clips':[]}
started=time.monotonic();rendered=0
for job in jobs:
    width=160 if job['action'] in ['Hammer','Repair','Chop','Pickaxe'] else 128
    height=160
    scene.render.resolution_x=width;scene.render.resolution_y=height
    action=bpy.data.actions['Rescue_'+job['action']]
    rig.animation_data.action=action;rig.animation_data.action_slot=action.slots[0]
    rig['character']=job['character_id']
    folder=CACHE/(job['character']+'-'+job['action']);folder.mkdir(exist_ok=True)
    directions=[7] if preview else range(8)
    frames=[0,job['frames']//4,job['frames']//2,3*job['frames']//4] if preview else range(job['frames'])
    for direction in directions:
        pivot.rotation_euler.z=math.pi/2+direction*math.pi/4
        for index in frames:
            dest=folder/f'{direction}-{index:03}.png'
            scene.frame_set(index+1)
            # A life ring is shared by worker/civilian sink poses. Our civilian
            # skin has no baked gear, so just show the same weighted ring there.
            if dest.is_file():continue
            scene.render.filepath=str(dest)
            bpy.ops.render.render(write_still=True)
            rendered+=1
    anchor=world_to_camera_view(scene,scene.camera,Vector((0,0,0)))
    manifest['clips'].append(dict(character=job['character'],action=job['action'],frames=job['frames'],resolution=[width,height],folder=str(folder.relative_to(ROOT)),anchor=[anchor.x*width,(1-anchor.y)*height]))
    (CACHE/'progress.json').write_text(json.dumps(dict(clip=folder.name,rendered=rendered,seconds=round(time.monotonic()-started,1)))+'\n')
if not preview:
    (ART/'render-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
rig['character']=0;rig.animation_data.action=bpy.data.actions['Rescue_Stand'];rig.animation_data.action_slot=rig.animation_data.action.slots[0]
scene.frame_set(1);pivot.rotation_euler.z=math.pi/4
result={'rendered':rendered,'preview':preview,'seconds':round(time.monotonic()-started,1),'clips':len(jobs)}
