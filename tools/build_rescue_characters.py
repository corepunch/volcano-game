"""Author Kenney-based rescue characters inside Blender (run through blender_mcp.py).

Kenney's separate FBX clips have different rest poses. Retarget through the
included Targeting Pose, then bake ordinary editable Blender actions. New work
motions are authored on that rig; all gear follows weighted bones.
"""
import bpy
import json
import math
from pathlib import Path
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[1] if '__file__' in globals() else Path('/Users/igor/Developer/volcano-sdl2')
ART = ROOT / 'assets/kenney-characters'
SOURCE = ART / 'source/survivors'
OUT = ART / 'blender'
OUT.mkdir(parents=True, exist_ok=True)

# This script runs only in our isolated Blender MCP process.
for obj in list(bpy.data.objects):
    bpy.data.objects.remove(obj, do_unlink=True)
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)
bpy.ops.import_scene.fbx(filepath=str(SOURCE / 'Model/characterMedium.fbx'))
rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
body = next(o for o in bpy.data.objects if o.type == 'MESH')
rig.name = 'RescueWorker_Rig'
body.name = 'RescueWorker_Body'
rig.animation_data_create()


def assign(action):
    rig.animation_data.action = action
    if action.slots:
        rig.animation_data.action_slot = action.slots[0]


def key_pose(frame):
    for pb in rig.pose.bones:
        pb.rotation_mode = 'QUATERNION'
        pb.keyframe_insert('location', frame=frame, group=pb.name)
        pb.keyframe_insert('rotation_quaternion', frame=frame, group=pb.name)
        pb.keyframe_insert('scale', frame=frame, group=pb.name)


def new_action(name, frames):
    rig.animation_data.action = None
    action = bpy.data.actions.new('Rescue_' + name)
    action.use_fake_user = True
    rig.animation_data.action = action
    action['frames'] = frames
    action['fps'] = 24
    action['loop'] = name not in ['Jump', 'Gather', 'Sink']
    return action


# Retarget each clip using its own exported targeting pose, rather than copying
# local Euler/quaternion values between incompatible FBX rest poses.
for clip, count in [('idle', 32), ('run', 16), ('jump', 12)]:
    before = set(bpy.data.objects)
    before_actions = set(bpy.data.actions)
    bpy.ops.import_scene.fbx(filepath=str(SOURCE / f'Animations/{clip}.fbx'))
    imported = set(bpy.data.objects) - before
    source_rig = next(o for o in imported if o.type == 'ARMATURE')
    targeting = source_rig.animation_data.action
    source_rig.animation_data.action = targeting
    source_rig.animation_data.action_slot = targeting.slots[0]
    bpy.context.scene.frame_set(1)
    correction = {b.name: source_rig.pose.bones[b.name].matrix.inverted() @ b.matrix_local for b in rig.data.bones}
    source_action = max(set(bpy.data.actions) - before_actions, key=lambda a: a.frame_range[1] - a.frame_range[0])
    source_rig.animation_data.action = source_action
    source_rig.animation_data.action_slot = source_action.slots[0]
    baked = []
    for frame in range(1, count + 1):
        bpy.context.scene.frame_set(frame)
        baked.append({b.name: source_rig.pose.bones[b.name].matrix.copy() @ correction[b.name] for b in rig.data.bones})
    action = new_action({'idle':'KenneyIdle', 'run':'Run', 'jump':'Jump'}[clip], count)
    action['origin'] = 'Retargeted from Kenney ' + clip + '.fbx through its Targeting Pose'
    for frame, matrices in enumerate(baked, 1):
        for pb in rig.pose.bones:
            parent = matrices[pb.parent.name] if pb.parent else Matrix.Identity(4)
            pb.matrix_basis = pb.bone.convert_local_to_pose(matrices[pb.name], pb.bone.matrix_local,
                parent_matrix=parent, parent_matrix_local=pb.parent.bone.matrix_local if pb.parent else Matrix.Identity(4), invert=True)
        key_pose(frame)
    for obj in imported:
        bpy.data.objects.remove(obj, do_unlink=True)
    for a in set(bpy.data.actions) - before_actions:
        if a != action:
            bpy.data.actions.remove(a)

assign(bpy.data.actions['Rescue_KenneyIdle'])
bpy.context.scene.frame_set(1)
base = {pb.name: pb.matrix_basis.copy() for pb in rig.pose.bones}
rig.animation_data.action = None


def reset_pose():
    for pb in rig.pose.bones:
        pb.matrix_basis = base[pb.name].copy()
    bpy.context.view_layer.update()


def rotate_world(name, axis, angle):
    pb = rig.pose.bones[name]
    mat = pb.matrix.copy()
    rotation = Quaternion(Vector(axis), angle).to_matrix().to_4x4()
    orientation = rotation @ mat
    orientation.translation = mat.translation
    pb.matrix = orientation
    bpy.context.view_layer.update()


def aim(name, direction):
    pb = rig.pose.bones[name]
    q = (pb.tail-pb.head).normalized().rotation_difference(Vector(direction).normalized())
    m = q.to_matrix().to_4x4() @ pb.matrix
    m.translation = pb.head.copy()
    pb.matrix = m
    bpy.context.view_layer.update()


def arm(side, upper, lower):
    aim(side+'Arm', upper)
    aim(side+'ForeArm', lower)


def tool_grip(direction):
    # Tool handles run along the hand's local Z axis. Explicitly orient the
    # wrist, rather than inheriting an arbitrary twist from the idle FBX.
    pb=rig.pose.bones['RightHand'];z=Vector(direction).normalized()
    x=Vector((1,0,0));y=z.cross(x).normalized();x=y.cross(z).normalized()
    mat=Matrix((x,y,z)).transposed().to_4x4();mat.translation=pb.head.copy()
    pb.matrix=mat;bpy.context.view_layer.update()


def root_drop(amount, forward=0):
    pb = rig.pose.bones['HipsCtrl']
    m = pb.matrix.copy(); m.translation += Vector((0, -forward, -amount)); pb.matrix = m
    bpy.context.view_layer.update()


def kneel():
    root_drop(.58)
    aim('RightUpLeg', (-.1, -.75, -.55)); aim('RightLeg', (0, .55, -.8))
    aim('LeftUpLeg', (.08, -.55, -.85)); aim('LeftLeg', (0, .8, -.2))


# All actions use complete baked bone tracks, with an identical final pose at
# the excluded loop endpoint. Linear keys prevent tool overshoot at contact.
counts = dict(Stand=32, Look=32, Carry=16, Chop=24, Gather=24, Hammer=24,
              Repair=24, Pickaxe=24, Valve=32, Call=32, Pet=24, Sit=32,
              Ill=32, Alert=32, Sink=32)
for name, count in counts.items():
    action = new_action(name, count)
    action['origin'] = 'Authored rescue work motion on the Kenney character rig'
    for frame in range(1, count+2):
        p = (frame-1)/count
        t = p*2*math.pi
        reset_pose()
        if name == 'Carry':
            assign(bpy.data.actions['Rescue_Run'])
            bpy.context.scene.frame_set(1+(frame-1)%16)
            pose = {b.name:b.matrix_basis.copy() for b in rig.pose.bones}
            assign(action)
            for b in rig.pose.bones: b.matrix_basis=pose[b.name]
            bpy.context.view_layer.update()
            arm('Right', (-.15,-.3,-1), (.1,-.45,.8))
            arm('Left', (.12,-.2,-1), (-.1,-.7,.15))
        elif name in ['Stand','Look']:
            rotate_world('Chest',(1,0,0),.015*math.sin(t))
            rotate_world('Head',(0,0,1),(.35 if name=='Look' else .06)*math.sin(t))
        elif name in ['Hammer','Repair']:
            # Anticipation overhead, a quick downstroke, then a short recovery.
            hit = max(0, math.sin(t))**.65
            rotate_world('Spine',(1,0,0),.1+.16*hit)
            arm('Right', (-.25,-.7,.15-.9*hit), (.05,-.55,.95-1.5*hit))
            arm('Left', (.05,-.55,-.6), (-.25,-.9,.1))
            rotate_world('Head',(1,0,0),.12)
            if name=='Repair':
                rotate_world('Chest',(1,0,0),-.16)
                arm('Left', (.05,-.7,-.3), (-.3,-.8,.45))
            tool_grip((0,-.2-.8*hit,1-1.5*hit))
        elif name in ['Chop','Pickaxe']:
            swing = (.5-.5*math.cos(t))
            rotate_world('Spine',(1,0,0),-.1+.6*swing)
            rotate_world('Chest',(0,0,1),.1*math.sin(t))
            arm('Right', (-.1,-.35,.7-1.5*swing), (.05,-.65,.85-1.3*swing))
            arm('Left', (.12,-.45,.55-1.4*swing), (-.6,-.65,.8-.9*swing))
            rotate_world('Head',(1,0,0),.2*swing)
            tool_grip((0,-.12-.8*swing,1-1.8*swing))
        elif name=='Gather':
            bend=math.sin(math.pi*p)**2
            rotate_world('Spine',(1,0,0),.75*bend)
            rotate_world('Chest',(1,0,0),.3*bend)
            root_drop(.25*bend)
            arm('Right', (-.08,-.25,-1), (.1,-.35,-.9+.5*bend))
            arm('Left', (.08,-.25,-1), (-.1,-.35,-.9+.5*bend))
            rotate_world('Head',(1,0,0),.15*bend)
        elif name=='Valve':
            root_drop(.25)
            rotate_world('Spine',(1,0,0),.35)
            arm('Right', (-.1,-.7,-.6), (.3*math.cos(t),-.9,.3*math.sin(t)))
            arm('Left', (.1,-.7,-.6), (-.3*math.cos(t),-.9,-.3*math.sin(t)))
        elif name in ['Pet','Call']:
            kneel()
            rotate_world('Spine',(1,0,0),.22)
            if name=='Pet':
                arm('Right', (-.12,-.8,-.4), (.05,-.8,-.3+.2*math.sin(t)))
                arm('Left', (.2,-.1,-1), (-.1,-.6,-.4))
                rotate_world('Head',(1,0,0),.35)
            else:
                arm('Right', (-.25,-.35,-.6), (.3,-.25,1))
                arm('Left', (.2,-.25,-.8), (-.1,-.5,.4))
                rotate_world('Head',(0,0,1),.16*math.sin(t))
                tool_grip((0,-.15,1))
        elif name in ['Sit','Ill','Alert']:
            root_drop(1.04)
            for side,sign in [('Left',1),('Right',-1)]:
                aim(side+'UpLeg',(sign*.2,-1,-.1));aim(side+'Leg',(sign*.1,-1,-.15));aim(side+'Foot',(0,-1,.05))
            rotate_world('Spine',(1,0,0),.22 if name=='Sit' else .45)
            arm('Right', (-.1,-.25,-1), (.3,-.75,.15))
            arm('Left', (.1,-.25,-1), (-.3,-.75,.15))
            if name=='Ill':
                arm('Right', (-.25,-.25,-1), (.65,-.2,.65))
                rotate_world('Head',(1,0,0),.35+.04*math.sin(t))
            if name=='Alert':
                arm('Right',(-.6,-.1,.8),(.05,-.15,1))
                rotate_world('RightForeArm',(0,1,0),.3*math.sin(t))
        elif name=='Sink':
            root_drop(.7+.06*math.sin(t))
            arm('Right',(-.8,-.4,-.15),(.2,-.7,.5))
            arm('Left',(.8,-.4,-.15),(-.2,-.7,.5))
        key_pose(frame)
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:key.interpolation='LINEAR'

# Materials and the original face texture are editable and packed in the blend.
def material(name, color, metal=0):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
    shader=m.node_tree.nodes.get('Principled BSDF');shader.inputs['Base Color'].default_value=(*color,1)
    shader.inputs['Roughness'].default_value=.62;shader.inputs['Metallic'].default_value=metal
    return m

orange=material('Rescue orange',(.95,.19,.025));navy=material('Workwear navy',(.025,.065,.11))
white=material('Reflective silver',(.8,.87,.84),.12);lime=material('Reflective yellow edge',(.78,.9,.04))
black=material('Boots and gloves',(.025,.032,.035));steel=material('Tool steel',(.36,.45,.5),.5)
wood=material('Tool ash handle',(.45,.22,.075));canvas=material('Supply sack canvas',(.58,.43,.21))
red=material('Rescue red',(.75,.035,.025));skin=bpy.data.materials.new('Kenney original skin');skin.use_nodes=True
image=bpy.data.images.load(str(SOURCE/'Skins/survivorMaleB.png'),check_existing=True)
tex=skin.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image;tex.interpolation='Closest'
skin.node_tree.links.new(tex.outputs['Color'],skin.node_tree.nodes.get('Principled BSDF').inputs['Base Color'])
skin.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.8
body.data.materials.clear()
for m in [skin,orange,navy,black]:body.data.materials.append(m)
for polygon in body.data.polygons:
    scores={}
    for vi in polygon.vertices:
        for g in body.data.vertices[vi].groups:
            n=body.vertex_groups[g.group].name;scores[n]=scores.get(n,0)+g.weight
    group=max(scores,key=scores.get) if scores else ''
    polygon.material_index=0 if group=='Head' else 3 if any(s in group for s in ['Hand','Foot','Toes']) else 2 if any(s in group for s in ['Leg','Hips']) else 1

worker_parts=[body]
props={}

def weight(obj,bone,prop=None):
    obj.vertex_groups.new(name=bone).add(list(range(len(obj.data.vertices))),1,'REPLACE')
    mod=obj.modifiers.new('Follow '+bone,'ARMATURE');mod.object=rig
    worker_parts.append(obj)
    if prop: props.setdefault(prop,[]).append(obj)
    return obj


def cube(name,loc,scale,mat,bone='Chest',prop=None,bevel=.035):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.scale=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    # Store geometry in armature rest coordinates so modifiers animate naturally.
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=False)
    o.data.materials.append(mat)
    if bevel:
        mod=o.modifiers.new('Soft edges','BEVEL');mod.width=bevel;mod.segments=2
        o.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
    return weight(o,bone,prop)


def mesh_obj(name,verts,faces,mat,bone,prop=None):
    me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(o);o.data.materials.append(mat)
    return weight(o,bone,prop)

# Hard hat: a rounded dome with a broad brim and central reinforcing ridge.
verts=[];faces=[];segments=24;rings=8
for ring in range(rings+1):
    phi=(ring/rings)*math.pi/2
    for j in range(segments):
        a=j/segments*2*math.pi;verts.append((.56*math.cos(phi)*math.cos(a),.03+.65*math.cos(phi)*math.sin(a),3.47+.57*math.sin(phi)))
for k in range(rings):
    for j in range(segments):
        n=(j+1)%segments;faces.append((k*segments+j,k*segments+n,(k+1)*segments+n,(k+1)*segments+j))
helmet=mesh_obj('Hard hat dome',verts,faces,orange,'Head')
for p in helmet.data.polygons:p.use_smooth=True
bpy.ops.mesh.primitive_cylinder_add(vertices=32,radius=1,depth=.065,location=(0,-.06,3.48));brim=bpy.context.object;brim.name='Hard hat brim';brim.scale=(.65,.8,1);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);brim.data.materials.append(orange);weight(brim,'Head')
cube('Hard hat ridge',(0,.03,3.93),(.085,.6,.11),orange,'Head')
# Vest bands sit over the torso on both sides, following the same torso bones.
for y in [-.292,.228]:
    for x in [-.18,.18]:
        cube('Vest yellow edging',(x,y,2.16),(.12,.03,.6),lime,'Chest',bevel=.008)
        cube('Vest reflective stripe',(x,y+(-.018 if y<0 else .018),2.16),(.065,.018,.6),white,'Chest',bevel=.005)
    cube('Vest waist band',(0,y,1.92),(.58,.035,.1),white,'Spine',bevel=.008)
cube('Utility belt',(0,-.01,1.53),(.7,.53,.1),black,'Hips')
cube('Belt buckle',(0,-.3,1.53),(.13,.05,.1),steel,'Hips')

# Build props in right-hand bone coordinates so every tool stays in the grip.
hand=rig.data.bones['RightHand'].matrix_local

def tool_cube(name,loc,scale,mat,kind):
    o=cube(name,(0,0,0),scale,mat,'RightHand',kind)
    transform=hand @ Matrix.Translation(Vector(loc))
    for v in o.data.vertices:v.co=transform @ v.co
    return o

for kind in ['hammer','axe','pickaxe']:
    length=.9 if kind=='hammer' else 1.45
    tool_cube(kind+' handle',(0,.19,.28),( .075,.075,length),wood,kind)
    z=.28+length/2-.08
    if kind=='hammer':tool_cube('Hammer head',(0,.19,z),(.17,.44,.18),steel,kind)
    elif kind=='axe':tool_cube('Axe head',(0,.33,z),(.09,.52,.36),steel,kind)
    else:tool_cube('Pickaxe head',(0,.19,z),(.11,.95,.14),steel,kind)
# A sack high on the back, held by the worker during the carrying run.
bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,location=(.02,.53,2.28));sack=bpy.context.object;sack.name='Supply sack';sack.scale=(.44,.34,.62);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);sack.data.materials.append(canvas);weight(sack,'Chest','sack')
cube('Sack tie',(.02,.48,2.91),(.21,.16,.11),wood,'Chest','sack')
# Hand radio and life ring for the two special rescue states.
tool_cube('Hand radio',(0,.12,.16),(.14,.2,.32),navy,'radio')
tool_cube('Radio antenna',(0,.12,.39),(.035,.035,.22),black,'radio')
bpy.ops.mesh.primitive_torus_add(major_radius=.68,minor_radius=.17,major_segments=32,minor_segments=8,location=(0,0,1.5));ring=bpy.context.object;ring.name='Life ring';bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);ring.data.materials.append(red);weight(ring,'Hips','ring')

# An unmodified civilian using the same mesh, rig and the supplied intact skin.
civilian=body.copy();civilian.data=body.data.copy();civilian.name='Civilian_Male';bpy.context.collection.objects.link(civilian)
civilian.data.materials.clear();civilian.data.materials.append(skin)
for p in civilian.data.polygons:p.material_index=0
civilian.hide_render=True;civilian.hide_set(True)
female=civilian.copy();female.data=civilian.data.copy();female.name='Civilian_Female';bpy.context.collection.objects.link(female)
fskin=skin.copy();fskin.name='Kenney female skin';ftex=next(n for n in fskin.node_tree.nodes if n.type=='TEX_IMAGE');ftex.image=bpy.data.images.load(str(SOURCE/'Skins/survivorFemaleA.png'),check_existing=True);female.data.materials[0]=fskin
female.hide_render=True;female.hide_set(True)

scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True
scene.render.resolution_x=160;scene.render.resolution_y=160;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA';scene.render.film_transparent=True
scene.render.fps=24
scene.world.color=(.35,.35,.35)
scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.68,.76,.86,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.7
scene.view_settings.view_transform='Standard'
scene.view_settings.look='None'
scene.render.use_file_extension=True
bpy.ops.object.empty_add();pivot=bpy.context.object;pivot.name='Direction_Pivot'
rig.parent=pivot
# Meshes use unparented armature modifiers. Parent them to the direction pivot,
# so a single turn rotates the entire assembled character consistently.
for o in worker_parts+[civilian,female]:
    o.parent=pivot;o.matrix_parent_inverse=Matrix.Identity(4)
bpy.ops.object.camera_add(location=(0,-10,7.8));camera=bpy.context.object;camera.name='Sprite_Camera';camera.data.type='ORTHO';camera.data.ortho_scale=6.2
camera.rotation_euler=(Vector((0,0,1.75))-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera
for name,loc,power,size in [('Key',(-4,-6,8),650,5),('Fill',(5,-2,5),350,4),('Rim',(1,4,7),450,3)]:
    bpy.ops.object.light_add(type='AREA',location=loc);light=bpy.context.object;light.name=name;light.data.energy=power;light.data.shape='DISK';light.data.size=size;light.rotation_euler=(Vector((0,0,2))-light.location).to_track_quat('-Z','Y').to_euler()

# This metadata controls visibility when rendering or previewing actions.
visibility=dict(Carry=['sack'],Chop=['axe'],Hammer=['hammer'],Repair=['hammer'],Pickaxe=['pickaxe'],Call=['radio'],Sink=['ring'])
for name in counts:
    bpy.data.actions['Rescue_'+name]['props']=json.dumps(visibility.get(name,[]))
for name in ['Run','Jump','KenneyIdle']:
    bpy.data.actions['Rescue_'+name]['props']='[]'
for kind,objects in props.items():
    for o in objects:o['rescue_prop']=kind;o.hide_render=True;o.hide_set(True)
for o in worker_parts:o['rescue_worker_part']=True
civilian['rescue_character']='male';female['rescue_character']='female'
# Gear follows the selected rig action in Blender as well as in our renderer.
# Character is an unanimated selector: 0 worker, 1 male, 2 female.
rig['character']=0
for kind in props:rig['gear_'+kind]=0.0
for a in [a for a in bpy.data.actions if 'frames' in a]:
    assign(a)
    active=json.loads(a.get('props','[]'))
    for kind in props:
        rig['gear_'+kind]=float(kind in active)
        rig.keyframe_insert(data_path='["gear_'+kind+'"]',frame=1,group='Equipment')

def visibility_driver(obj, expression, gear=None):
    obj.hide_set(False)
    for path in ['hide_render','hide_viewport']:
        driver=obj.driver_add(path).driver
        v=driver.variables.new();v.name='character';v.targets[0].id=rig;v.targets[0].data_path='["character"]'
        if gear:
            v=driver.variables.new();v.name='gear';v.targets[0].id=rig;v.targets[0].data_path='["gear_'+gear+'"]'
        driver.expression=expression
for o in worker_parts:
    gear=o.get('rescue_prop')
    expression='gear < 0.5' if gear=='ring' else 'character != 0 or gear < 0.5' if gear else 'character != 0'
    visibility_driver(o,expression,gear)
visibility_driver(civilian,'character != 1')
visibility_driver(female,'character != 2')
rig['source']='Kenney Animated Characters Survivors 1.1 / CC0'
rig['directions']='0=E, 1=NE, 2=N, 3=NW, 4=W, 5=SW, 6=S, 7=SE'
rig['preview_help']='Select a Rescue_* action. Gear follows the action. Custom property character: 0 worker, 1 male, 2 female.'
assign(bpy.data.actions['Rescue_Stand']);scene.frame_start=1;scene.frame_end=32;scene.frame_set(1);pivot.rotation_euler.z=math.pi/4
bpy.context.view_layer.objects.active=rig
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
for area in bpy.context.screen.areas:
    if area.type=='VIEW_3D':
        area.spaces.active.region_3d.view_distance=7
        area.spaces.active.region_3d.view_location=Vector((0,0,2))
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'rescue-characters.blend'))
metadata={'source':'https://kenney.nl/assets/animated-characters-survivors','license':'CC0-1.0','actions':{a.name:{'frames':int(a['frames']),'fps':int(a['fps']),'origin':a.get('origin',''),'props':json.loads(a.get('props','[]'))} for a in bpy.data.actions if 'frames' in a},'directions':['E','NE','N','NW','W','SW','S','SE']}
(ART/'actions.json').write_text(json.dumps(metadata,indent=2)+'\n')
result={'blend':str(OUT/'rescue-characters.blend'),'actions':list(metadata['actions']),'objects':len(bpy.data.objects)}
