"""Paint and bake a rescue uniform onto Kenney's existing body UVs in Blender.

No clothing geometry is added. The original skin UV map stays available; an
atlas UV layer preserves that layout in the upper 1024 rows of a 1024x1280
image. The bottom strip holds the helmet and shared tool-color swatches.
"""
import json
import math
from pathlib import Path
import bpy


def simple_helmet(rig, collection):
    """One 62-vertex silhouette: twelve sides, three dome rings and a brim."""
    n=12;verts=[];faces=[]
    for ring in range(3):
        phi=ring*math.pi/6
        for j in range(n):
            a=j*2*math.pi/n
            verts.append((.56*math.cos(phi)*math.cos(a),.03+.65*math.cos(phi)*math.sin(a),3.47+.57*math.sin(phi)))
    pole=len(verts);verts.append((0,.03,4.04))
    for ring in range(2):
        for j in range(n):
            k=(j+1)%n;faces.append((ring*n+j,ring*n+k,(ring+1)*n+k,(ring+1)*n+j))
    for j in range(n):faces.append((2*n+j,2*n+(j+1)%n,pole))
    for z in [3.47,3.41]:
        for j in range(n):
            a=j*2*math.pi/n;verts.append((.65*math.cos(a),-.06+.8*math.sin(a),z))
    top=pole+1;bottom=top+n;center=len(verts);verts.append((0,-.06,3.41))
    for j in range(n):
        k=(j+1)%n
        faces.extend([(j,top+j,top+k,k),(top+j,bottom+j,bottom+k,top+k),(bottom+k,bottom+j,center)])
    mesh=bpy.data.meshes.new('Hard hat silhouette');mesh.from_pydata(verts,[],faces);mesh.update()
    obj=bpy.data.objects.new('Hard hat',mesh);collection.objects.link(obj)
    obj.vertex_groups.new(name='Head').add(list(range(len(verts))),1,'REPLACE')
    obj.modifiers.new('Follow Head','ARMATURE').object=rig
    # A plan projection is sufficient for a rotational shell; the lower side
    # band is painted from the same swatch at its edge, with no separate meshes.
    uv=mesh.uv_layers.new(name='RescueAtlas')
    for loop in mesh.loops:
        co=mesh.vertices[loop.vertex_index].co
        uv.data[loop.index].uv=((co.x/.65*.46+.5)*.25,((co.y+.06)/.8*.46+.5)*.2)
    for p in mesh.polygons:
        p.use_smooth=p.center.z>3.48
        if p.center.z<3.425:
            for index in p.loop_indices:uv.data[index].uv=(320/1024,64/1280)
    return obj


class Paint:
    def __init__(self,name,image):
        self.mat=bpy.data.materials.new(name);self.mat.use_nodes=True;self.mat.use_fake_user=True
        self.nodes=self.mat.node_tree.nodes;self.nodes.clear();self.links=self.mat.node_tree.links
        self.out=self.nodes.new('ShaderNodeOutputMaterial')
        self.emit=self.nodes.new('ShaderNodeEmission');self.links.new(self.emit.outputs[0],self.out.inputs['Surface'])
        self.target=self.nodes.new('ShaderNodeTexImage');self.target.name='BAKE TARGET';self.target.image=image
        self.nodes.active=self.target
        geo=self.nodes.new('ShaderNodeNewGeometry');space=self.nodes.new('ShaderNodeVectorTransform')
        space.vector_type='POINT';space.convert_from='WORLD';space.convert_to='OBJECT';self.links.new(geo.outputs['Position'],space.inputs[0])
        split=self.nodes.new('ShaderNodeSeparateXYZ');self.links.new(space.outputs[0],split.inputs[0]);self.x,self.y,self.z=split.outputs
    def input(self,socket,value):
        if hasattr(value,'node'):self.links.new(value,socket)
        else:socket.default_value=value
    def math(self,op,a,b=0):
        node=self.nodes.new('ShaderNodeMath');node.operation=op
        self.input(node.inputs[0],a);self.input(node.inputs[1],b);return node.outputs[0]
    def band(self,value,lo,hi):
        return self.math('MULTIPLY',self.math('GREATER_THAN',value,lo),self.math('LESS_THAN',value,hi))
    def both(self,*masks):
        result=masks[0]
        for mask in masks[1:]:result=self.math('MULTIPLY',result,mask)
        return result
    def mix(self,mask,a,b):
        node=self.nodes.new('ShaderNodeMixRGB');node.blend_type='MIX'
        self.input(node.inputs[0],mask);self.input(node.inputs[1],a);self.input(node.inputs[2],b);return node.outputs[0]
    def finish(self,color):self.input(self.emit.inputs['Color'],color);return self.mat


def bake_uniform(rig,body,helmet,worker_parts,art,palette):
    atlas_path=Path(art)/'textures/rescue-worker.png';atlas_path.parent.mkdir(parents=True,exist_ok=True)
    old=bpy.data.images.get('Rescue worker atlas')
    if old:bpy.data.images.remove(old)
    image=bpy.data.images.new('Rescue worker atlas',width=1024,height=1280,alpha=False)
    image.colorspace_settings.name='sRGB'
    original=body.data.uv_layers.active
    atlas_uv=body.data.uv_layers.new(name='RescueAtlas',do_init=True)
    for src,dst in zip(original.data,atlas_uv.data):dst.uv=(src.uv.x,.2+.8*src.uv.y)
    body.data.uv_layers.active=atlas_uv;atlas_uv.active_render=True
    orange=palette['orange'];navy=palette['navy'];silver=palette['silver'];lime=palette['lime'];black=palette['black']
    white=(.78,.82,.8,1);seam=(.5,.075,.012,1)
    p=Paint('Uniform paint / vest',image);ax=p.math('ABSOLUTE',p.x)
    color=orange
    pocket=p.both(p.band(ax,.075,.285),p.band(p.z,1.99,2.17),p.math('LESS_THAN',p.y,-.12))
    color=p.mix(pocket,color,seam)
    pocket_inner=p.both(p.band(ax,.088,.272),p.band(p.z,2.005,2.155),p.math('LESS_THAN',p.y,-.12))
    color=p.mix(pocket_inner,color,orange)
    edges=p.both(p.band(ax,.12,.24),p.band(p.z,1.73,2.48))
    tape=p.both(p.band(ax,.15,.21),p.band(p.z,1.73,2.48))
    color=p.mix(edges,color,lime);color=p.mix(tape,color,silver)
    color=p.mix(p.band(p.z,1.79,1.89),color,lime);color=p.mix(p.band(p.z,1.815,1.865),color,silver)
    zipper=p.both(p.band(ax,0,.012),p.band(p.z,1.6,2.38),p.math('LESS_THAN',p.y,-.12))
    vest=p.finish(p.mix(zipper,color,seam))
    p=Paint('Uniform paint / sleeves',image);ax=p.math('ABSOLUTE',p.x)
    sleeves=p.finish(p.mix(p.math('GREATER_THAN',ax,1.20),white,black))
    p=Paint('Uniform paint / trousers and belt',image);ax=p.math('ABSOLUTE',p.x)
    color=p.mix(p.band(p.z,.73,.9),navy,(.018,.045,.075,1))
    color=p.mix(p.band(p.z,1.46,1.57),color,black)
    buckle=p.both(p.band(p.z,1.485,1.55),p.band(ax,0,.07),p.math('LESS_THAN',p.y,-.2))
    pants=p.finish(p.mix(buckle,color,silver))
    p=Paint('Uniform paint / gloves and boots',image);boots=p.finish(black)
    p=Paint('Uniform paint / original Kenney face',image)
    tex=p.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(Path(art)/'source/survivors/Skins/survivorMaleB.png'),check_existing=True)
    uv=p.nodes.new('ShaderNodeUVMap');uv.uv_map=original.name;p.links.new(uv.outputs[0],tex.inputs['Vector'])
    face=p.finish(tex.outputs['Color'])
    body.data.materials.clear()
    for mat in [face,vest,sleeves,pants,boots]:body.data.materials.append(mat)
    for poly in body.data.polygons:
        groups={}
        for vi in poly.vertices:
            for g in body.data.vertices[vi].groups:
                name=body.vertex_groups[g.group].name;groups[name]=groups.get(name,0)+g.weight
        group=max(groups,key=groups.get)
        poly.material_index=0 if group=='Head' else 4 if any(s in group for s in ['Hand','Foot','Toes']) else 3 if any(s in group for s in ['Leg','Hips']) else 2 if any(s in group for s in ['Arm','Shoulder']) else 1
    p=Paint('Uniform paint / hard hat',image);ax=p.math('ABSOLUTE',p.x)
    color=p.mix(p.both(p.band(ax,0,.045),p.math('GREATER_THAN',p.z,3.65)),orange,(.78,.13,.018,1))
    # Painted reinforcing ridge and a plain reflective sticker; no raised trim.
    badge=p.both(p.band(ax,0,.14),p.math('LESS_THAN',p.y,-.47),p.band(p.z,3.52,3.72))
    color=p.mix(badge,color,silver);color=p.mix(p.math('LESS_THAN',p.z,3.435),color,(.6,.105,.015,1))
    helmet.data.materials.clear();helmet.data.materials.append(p.finish(color))
    # Bake twelve palette tiles into the same texture for unchanged tool shapes.
    verts=[];faces=[];tile_uv=[];tile_materials=[];swatches={}
    for i,(name,rgba) in enumerate(palette.items()):
        x=256+(i%6)*128;y=(i//6)*128
        swatches[name]=((x+64)/1024,(y+64)/1280)
        j=len(verts);verts.extend([(0,0,-5-i),(1,0,-5-i),(1,1,-5-i),(0,1,-5-i)]);faces.append((j,j+1,j+2,j+3))
        tile_uv.extend([(x/1024,y/1280),((x+128)/1024,y/1280),((x+128)/1024,(y+128)/1280),(x/1024,(y+128)/1280)])
        tile_materials.append(Paint('Uniform paint / palette '+name,image).finish(rgba))
    mesh=bpy.data.meshes.new('Temporary palette bake');mesh.from_pydata(verts,[],faces);mesh.update()
    tiles=bpy.data.objects.new('Temporary palette bake',mesh);bpy.context.collection.objects.link(tiles)
    uv=mesh.uv_layers.new(name='RescueAtlas')
    for loop,coords in zip(uv.data,tile_uv):loop.uv=coords
    for i,mat in enumerate(tile_materials):mesh.materials.append(mat);mesh.polygons[i].material_index=i
    scene=bpy.context.scene;engine=scene.render.engine;samples=scene.cycles.samples
    action=rig.animation_data.action;slot=rig.animation_data.action_slot;pose=rig.data.pose_position
    rig.animation_data.action=None;rig.data.pose_position='REST';rig['character']=0
    scene.render.engine='CYCLES';scene.cycles.samples=1
    scene.render.bake.use_selected_to_active=False;scene.render.bake.use_clear=False;scene.render.bake.margin=4
    bpy.ops.object.select_all(action='DESELECT')
    for obj in [body,helmet,tiles]:obj.hide_set(False);obj.hide_render=False;obj.select_set(True)
    bpy.context.view_layer.objects.active=body;bpy.context.view_layer.update()
    bpy.ops.object.bake(type='EMIT')
    image.filepath_raw=str(atlas_path);image.file_format='PNG';image.save();image.pack()
    bpy.data.objects.remove(tiles,do_unlink=True)
    scene.render.engine=engine;scene.cycles.samples=samples;rig.data.pose_position=pose
    rig.animation_data.action=action
    if slot:rig.animation_data.action_slot=slot
    # One image material for the body, helmet and every nonmetal prop. A second
    # uses the same atlas with metallic response for tool heads.
    def surface(name,metallic=0):
        mat=bpy.data.materials.new(name);mat.use_nodes=True
        bsdf=mat.node_tree.nodes.get('Principled BSDF');bsdf.inputs['Roughness'].default_value=.7;bsdf.inputs['Metallic'].default_value=metallic
        tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image;tex.interpolation='Closest'
        uv=mat.node_tree.nodes.new('ShaderNodeUVMap');uv.uv_map='RescueAtlas';mat.node_tree.links.new(uv.outputs[0],tex.inputs['Vector']);mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
        return mat
    uniform=surface('Rescue uniform / one texture');metal=surface('Rescue tools / same texture',.5)
    for obj in [body,helmet]:
        obj.data.materials.clear();obj.data.materials.append(uniform)
        for poly in obj.data.polygons:poly.material_index=0
    for obj in worker_parts:
        if obj in [body,helmet]:continue
        old_color=tuple(obj.data.materials[0].diffuse_color)
        name=min(palette,key=lambda k:sum((palette[k][i]-old_color[i])**2 for i in range(3)))
        uv=obj.data.uv_layers.get('RescueAtlas') or obj.data.uv_layers.new(name='RescueAtlas')
        for loop in uv.data:loop.uv=swatches[name]
        obj.data.uv_layers.active=uv;uv.active_render=True
        obj.data.materials.clear();obj.data.materials.append(metal if name=='steel' else uniform)
        for poly in obj.data.polygons:poly.material_index=0
    report={'body_vertices':len(body.data.vertices),'body_triangles':sum(len(p.vertices)-2 for p in body.data.polygons),'helmet_vertices':len(helmet.data.vertices),'helmet_triangles':sum(len(p.vertices)-2 for p in helmet.data.polygons),'uniform_mesh_pieces':0,'worker_texture':'textures/rescue-worker.png','texture_size':[1024,1280],'body_uv':'Original Kenney UVMap retained, affine atlas layout in RescueAtlas','worker_materials':2}
    (Path(art)/'textures/uniform.json').write_text(json.dumps(report,indent=2)+'\n')
    return report
