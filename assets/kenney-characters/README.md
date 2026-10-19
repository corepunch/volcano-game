# Kenney rescue characters

The source is [Kenney Animated Characters Survivors](https://kenney.nl/assets/animated-characters-survivors), released under CC0. The original download, its license, skins, FBX files and SHA-256 provenance are retained in `source/`. The safety clothing, equipment and additional work animations are authored in this project.

Open `blender/rescue-characters.blend` in Blender 5.2 or newer. It contains the worker, male and female civilian meshes, packed textures, an orthographic sprite camera and editable, baked `Rescue_*` actions. The selected action automatically shows its equipment. The rig's `character` custom property selects `0` worker, `1` male or `2` female.

| Action | Game animation names | Equipment / behavior |
| --- | --- | --- |
| Stand / Look | `worker_wait0..2`, civilian `*_idle0..2` | Breathing and looking around |
| Run | `worker_run0..7`, civilian `*_run0..7` | Retargeted Kenney run, in place |
| Carry | `worker_bag0..7` | Run with supply sack |
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

The scheduler selects axe work for trees, pickaxe work for rocks, hammering for construction and repair, valve turning, animal petting and calling to a survivor. Standing, running, gathering and carrying retain their usual roles. Sitting and distress states replace their original resource names for campaign scenes; Jump is available for future use. This changes job presentation without altering costs, durations or completion rules.

## Use in the game

`make hd-assets` installs the character atlases after building the HD static art. Existing `make run-emerald-falls-hd`, `make run-forest-mayday-hd` and `make hd-app` targets use them. `make character-assets` creates `build/kenney-assets` with the new characters over the retail environment. The source resource directory is preserved.

The sprite installer retains each original animation's frame count and FPS. It resamples the baked source poses to those frames; Transparent 128×160 renders (160×160 for wide tool swings) supply 2× texture resolution. It trims alpha and computes pivots from the projected world origin, so changing poses and eight-direction turns stay attached to the same ground point. Directions follow the game's convention: E, NE, N, NW, W, SW, S, SE. Retail shadow sprites remain in use.

## Rebuild through Blender MCP

The installed Blender MCP extension provides a null-delimited socket bridge. `tools/blender_mcp.py` talks directly to that extension; it does not require a separate cloud connection. Use a dedicated background Blender process so an open user scene remains untouched:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --online-mode \
  --command blender_mcp --host 127.0.0.1 --port 9877
```

In a second terminal, from the repository root:

```sh
python3 tools/blender_mcp.py tools/build_rescue_characters.py
python3 tools/blender_mcp.py tools/render_rescue_characters.py
python3 tools/pack_rescue_characters.py --pack
make hd-assets
```

The render cache is under `build/kenney-character-frames`. A changed blend file or render script invalidates the cache. A first-time full rebuild after an interrupted preview can be forced by removing that generated cache directory. The saved `sprites/` PNG atlases and catalog make normal game builds independent of Blender.

The FBX clips have different rest poses. The authoring script transfers each clip through its exported Targeting Pose before baking, rather than copying incompatible local quaternion tracks. New work actions are authored on the retargeted rig. Tool and clothing geometry follows weighted bones; there is no frame-by-frame image generation or derived artwork.
