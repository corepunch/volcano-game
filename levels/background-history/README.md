# Background authoring history

These are the preserved inputs and exact prompts used to create and revise
Ancient Echoes and Twisted Tracks on 2026-10-19. The current game loads the PNGs
in the parent `levels/` directory; these archived inputs are for authoring.
See the [production guide](../../docs/creating-backgrounds.md) for the workflow.

| Stage | Input | Exact prompt | Output |
| --- | --- | --- | --- |
| Ancient Echoes: initial generation | Text only | [Initial prompt](ancient-echoes-v1.prompt.txt) | [Original image](ancient-echoes-v1.png) |
| Ancient Echoes: expanded ruins and rough tracks | Original image | [Edit prompt](../ancient-echoes.prompt.txt) | [Current image](../ancient-echoes.png) |
| Twisted Tracks: initial generation | Text only | [Initial prompt](twisted-tracks-v1.prompt.txt) | [Original image](twisted-tracks-v1.png) |
| Twisted Tracks: expanded wreck and rough tracks | Original image | [Expansion prompt](twisted-tracks-expanded.prompt.txt) | [Intermediate image](twisted-tracks-expanded.png) |
| Twisted Tracks: clear access through the right side | Intermediate image | [Access refinement prompt](twisted-tracks-access.prompt.txt) | [Current image](../twisted-tracks.png) |

Each row was one built-in `image_gen.imagegen` call. Generation had no image
input; edits referenced the indicated input PNG. All used
`transparent_background=false`. The top-level
[`twisted-tracks.prompt.txt`](../twisted-tracks.prompt.txt) combines its two edit
prompts as a readable record; they were executed as separate calls, in order.

[`manifest.json`](manifest.json) records the input-stage relationships,
repository-relative paths, PNG dimensions, SHA-256 hashes and original commits.
The first images/prompts were recovered byte-for-byte from their initial level
commits. The intermediate coaster image was copied unchanged from its saved
generation output. No model identifier or random seed was exposed in the
recorded results, so those fields are null. Regeneration can vary; the preserved
PNG files are the authoritative artwork used for these versions of the maps.

The source commits also preserve the initial mission layouts. To inspect them:

```sh
git show ad9c0c9:levels/ancient-echoes.level.json
git show a6745fb:levels/twisted-tracks.level.json
```

Current placements and routes are in
[`ancient-echoes.level.json`](../ancient-echoes.level.json) and
[`twisted-tracks.level.json`](../twisted-tracks.level.json). Measure any newly
generated output again before reusing those coordinates. Update the archive and
manifest when adding later image stages; do not silently change a recorded stage.
