"""Add an exaggerated rescue-game run to the retargeted Kenney clip in Blender.

Call once on the freshly retargeted Run, before building Carry. Geometry,
textures, stride timing and the original leg poses remain unchanged.
"""
import math

import bpy
from mathutils import Quaternion, Vector


def assign(rig, action):
    rig.animation_data.action = action
    rig.animation_data.action_slot = action.slots[0]
    # Evaluate the action switch before applying a manually sampled pose.
    # Otherwise the first depsgraph update can replace it with the old keys.
    bpy.context.view_layer.update()


def rotate(rig, name, axis, angle):
    bone = rig.pose.bones[name]
    matrix = bone.matrix.copy()
    turned = Quaternion(Vector(axis), angle).to_matrix().to_4x4() @ matrix
    turned.translation = matrix.translation
    bone.matrix = turned
    bpy.context.view_layer.update()


def key_pose(rig, frame, previous):
    for bone in rig.pose.bones:
        bone.rotation_mode = 'QUATERNION'
        if bone.name in previous:
            bone.rotation_quaternion.make_compatible(previous[bone.name])
        previous[bone.name] = bone.rotation_quaternion.copy()
        for path in ('location', 'rotation_quaternion', 'scale'):
            bone.keyframe_insert(path, frame=frame, group=bone.name)


def linear(action):
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:
                        key.interpolation = 'LINEAR'


def reduce_head_swing(rig, factor=.5):
    """Scale neck/head angular excursions, including inherited torso motion.

    Apply once to the full-strength run. Keep the average facing direction,
    neck attachment, torso, arms and bouncing stride intact.
    """
    action = bpy.data.actions['Rescue_Run']
    count = int(action['frames'])
    assign(rig, action)
    names = ('Neck', 'Head')
    rotations = {name: [] for name in names}
    for frame in range(1, count + 1):
        bpy.context.scene.frame_set(frame)
        for name in names:
            q = rig.pose.bones[name].matrix.to_quaternion()
            if rotations[name]:
                q.make_compatible(rotations[name][0])
            rotations[name].append(q)
    means = {name: Quaternion(tuple(sum(q[i] for q in values)/count
                                   for i in range(4))).normalized()
             for name, values in rotations.items()}
    previous = {}
    for frame in range(1, count + 2):
        bpy.context.scene.frame_set(frame)
        for name in names:
            bone = rig.pose.bones[name]
            q = means[name].slerp(rotations[name][(frame-1) % count], factor)
            matrix = q.to_matrix().to_4x4()
            matrix.translation = bone.head.copy()
            bone.matrix = matrix
            bpy.context.view_layer.update()
            if name in previous:
                bone.rotation_quaternion.make_compatible(previous[name])
            previous[name] = bone.rotation_quaternion.copy()
            bone.keyframe_insert('rotation_quaternion', frame=frame, group=name)
    linear(action)
    action['head_swing_scale'] = factor


def enhance_run(rig):
    action = bpy.data.actions['Rescue_Run']
    count = int(action['frames'])
    assign(rig, action)
    # Sample everything before writing keys: the additive motion must never
    # feed back into later poses. Frame count + 1 repeats the first pose.
    source = []
    for frame in range(1, count + 1):
        bpy.context.scene.frame_set(frame)
        source.append({b.name: b.matrix_basis.copy() for b in rig.pose.bones})
    previous = {}
    for frame in range(1, count + 2):
        t = (frame - 1) * 2 * math.pi / count
        for bone in rig.pose.bones:
            bone.matrix_basis = source[(frame - 1) % count][bone.name]
        bpy.context.view_layer.update()
        # Two compressions and two airborne crests per cycle. The source
        # plants its feet at frames 3/11 and reaches flight at frames 7/15.
        lift = .34 * (.5 - .5 * math.cos(2 * t - math.pi / 2))
        root = rig.pose.bones['HipsCtrl']
        matrix = root.matrix.copy()
        matrix.translation += Vector((.055 * math.sin(t), -.055 * math.sin(2*t), lift))
        root.matrix = matrix
        bpy.context.view_layer.update()
        # Rotation, rather than scaling or stretching the neck, makes the
        # large Kenney head travel visibly back and forth above the shoulders.
        rotate(rig, 'Spine', (1, 0, 0), .10 + .16 * math.sin(2*t - .2))
        rotate(rig, 'Spine', (0, 1, 0), .055 * math.sin(t))
        rotate(rig, 'Chest', (0, 0, 1), .13 * math.sin(t + .15))
        rotate(rig, 'Chest', (1, 0, 0), -.035 + .07 * math.sin(2*t - .55))
        rotate(rig, 'Neck', (1, 0, 0), -.06 + .06 * math.sin(2*t - .9))
        rotate(rig, 'Head', (1, 0, 0), -.10 + .16 * math.sin(2*t - 1.1))
        rotate(rig, 'Head', (0, 0, 1), -.055 * math.sin(t + .15))
        for side, sign in [('Left', 1), ('Right', -1)]:
            rotate(rig, side + 'Arm', (1, 0, 0), sign * .24 * math.sin(t + .1))
            rotate(rig, side + 'ForeArm', (1, 0, 0), .10 * math.sin(2*t - .4))
        key_pose(rig, frame, previous)
    linear(action)
    action['origin'] = ('Kenney run retargeted through its Targeting Pose; '
                        'authored stride bounce, torso recoil, half-strength head follow-through and arm drive')
    reduce_head_swing(rig)
    return action


def carry_arms(rig):
    """Keep the carrying pose in the moving chest's coordinate system."""
    chest = rig.pose.bones['Chest']
    turn = chest.matrix.to_quaternion() @ chest.bone.matrix_local.to_quaternion().inverted()
    for side, upper, lower in [
        ('Right', (-.15, -.3, -1), (.1, -.45, .8)),
        ('Left', (.12, -.2, -1), (-.1, -.7, .15)),
    ]:
        for name, direction in [(side+'Arm', upper), (side+'ForeArm', lower)]:
            bone = rig.pose.bones[name]
            q = (bone.tail-bone.head).normalized().rotation_difference((turn @ Vector(direction)).normalized())
            matrix = q.to_matrix().to_4x4() @ bone.matrix
            matrix.translation = bone.head.copy()
            bone.matrix = matrix
            bpy.context.view_layer.update()


def rebuild_carry(rig):
    """Update an existing textured scene without rebuilding unrelated actions."""
    run = bpy.data.actions['Rescue_Run']
    carry = bpy.data.actions['Rescue_Carry']
    count = int(run['frames'])
    poses = []
    assign(rig, run)
    for frame in range(1, count + 1):
        bpy.context.scene.frame_set(frame)
        poses.append({b.name: b.matrix_basis.copy() for b in rig.pose.bones})
    assign(rig, carry)
    previous = {}
    for frame in range(1, count + 2):
        for bone in rig.pose.bones:
            bone.matrix_basis = poses[(frame - 1) % count][bone.name]
        bpy.context.view_layer.update()
        carry_arms(rig)
        key_pose(rig, frame, previous)
    linear(carry)
    carry['origin'] = 'Authored rescue carrying run with stride bounce, half-strength head follow-through and chest-relative sack grip'
    return carry
