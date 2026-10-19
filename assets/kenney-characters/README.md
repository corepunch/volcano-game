# Kenney rescue characters

The source is [Kenney Animated Characters Survivors](https://kenney.nl/assets/animated-characters-survivors), released under CC0. The original download, its license, skins, FBX files and SHA-256 provenance are retained in `source/`. The safety clothing, equipment and additional work animations are authored in this project.

Open `blender/rescue-characters.blend` in Blender 5.2 or newer. It contains the worker, male and female civilian meshes, packed textures, an orthographic sprite camera and editable, baked `Rescue_*` actions. The selected action automatically shows its equipment. The rig's `character` custom property selects `0` worker, `1` male or `2` female.

The uniform is painted into `textures/rescue-worker.png`: vest, reflective tape, pockets, zipper, belt, buckle, sleeves, gloves and boots. This single atlas also supplies the hard hat and tool colors. Kenney's body remains exactly **804 vertices / 1,604 triangles**, with its original positions, topology, skin weights and `UVMap` preserved. A second UV layer fits that layout into the shared atlas. There are no added clothing meshes. The helmet is one **62-vertex / 120-triangle** silhouette with painted ridge and sticker details. The standing worker now has 1,724 triangles instead of 3,516. `textures/model-check.json` records the comparison against the original FBX.

| Action | Game animation names | Equipment / behavior |
| --- | --- | --- |
| Stand / Look | `worker_wait0..2`, civilian `*_idle0..2` | Breathing and looking around |
| Run | `worker_run0..7`, civilian `*_run0..7` | Bouncy in-place run with torso recoil, head follow-through and arm drive |
| Carry | `worker_bag0..7` | Same bouncing stride with a chest-relative supply sack grip |
| Chop | `worker_wood0..7` | Two-handed axe swing |
| Gather | `worker_gather0..7` | Bend and reach for supplies |
| Hammer | `worker_srepair0..7` | Hammer strike at a low work surface |
| Repair | `worker_repair0..7` | Hammer at a higher work surface |
| Pickaxe | `worker_pick0..7` | Two-handed pickaxe swing |
| Valve | `worker_tap0..7` | Turn a valve with both hands |
| Call | `worker_call0..7` | Crouch and use a hand radio |
| Pet | `worker_pet0..7` | Kneel and reach toward an animal |
| Sit | `worker_sit0..1` | Seated waiting pose |
| Ill / Alert | `*_ill`, `*_ill_alert` | Seated distress and signaling |
| Sink | `*_sink` | Bob in a life ring |
| Jump | Available in the Blender file and sprite atlas | Retargeted Kenney jump; no current game trigger |

`Rescue_KenneyIdle` retains the retargeted original idle separately. `actions.json` lists all eighteen actions. `retail-worker-actions.json` inventories the original worker's frame counts and frame rates. Ill, Alert and Sink also have male and female civilian renders; both original civilian color variants currently map to the same male/female skin.

The sixteen-frame run adds two distinct compressions and airborne crests per cycle. Hips rise and fall by 0.38 model units; the head follows the torso with delayed neck/head rotation. Neck and head angular swing is reduced to 25% of the original around the average facing direction, including inherited torso rotation; the body bounce and stride remain unchanged. Shoulder counter-rotation and stronger arm swings make the whole silhouette move. Carry inherits this motion with its arms held relative to the chest. Both actions include a matching loop endpoint at frame 17. `previews/dynamic-run.gif` shows side and three-quarter views of both clips. `tools/animate_rescue_run.py` applies these edits after retargeting, without changing the original mesh or texture.

The scheduler selects axe work for trees, pickaxe work for rocks, hammering for construction and repair, valve turning, animal petting and calling to a survivor. Standing, running, gathering and carrying retain their usual roles. Sitting and distress states replace their original resource names for campaign scenes; Jump is available for future use. This changes job presentation without altering costs, durations or completion rules.

## Use in the game

`make hd-assets` installs the character atlases after building the HD static art. Existing `make run-emerald-falls-hd`, `make run-forest-mayday-hd` and `make hd-app` targets use them. `make character-assets` creates `build/kenney-assets` with the new characters over the retail environment. The source resource directory is preserved.

The sprite installer retains each original animation's frame count and FPS. It resamples the baked source poses to those frames; Transparent 128×160 renders (160×160 for wide tool swings) supply 2× texture resolution. Packing rejects empty, clipped or unlit frames. It trims alpha and computes pivots from the projected world origin, so changing poses and eight-direction turns stay attached to the same ground point. Directions follow the game's convention: E, NE, N, NW, W, SW, S, SE. Retail shadow sprites remain in use.

## Render or rebuild through Blender MCP

Follow the [character rendering guide](../../docs/rendering-characters.md) for
setup, saved-model rendering, rebuilding from FBX sources, partial re-renders,
previewing, cache recovery and game verification.

To render the existing model, start a dedicated background Blender MCP process
with the saved scene loaded:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background \
  assets/kenney-characters/blender/rescue-characters.blend --online-mode \
  --command blender_mcp --host 127.0.0.1 --port 9877
```

Then, in another terminal at the repository root:

```sh
python3 tools/blender_mcp.py tools/render_rescue_characters.py
python3 tools/pack_rescue_characters.py --pack
make test-hd hd-app
```

`tools/build_rescue_characters.py` is a separate authoring step: it recreates the
scene from source and overwrites the saved model and baked uniform. Use it when
rebuilding from the authoring scripts, not merely to re-render saved edits.
Normal game builds use the committed sprite atlases and do not require Blender.
