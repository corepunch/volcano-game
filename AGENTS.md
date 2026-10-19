## Workflow

Always commit each completed step before starting the next step. Do not accumulate
uncommitted changes across steps.

## Icon generation

Never draw icons separately. Generate every icon in a set together as a single
strip or atlas in one ImageGen call. When visual states are needed, draw normal,
selected, pressed, hover, and disabled states together in that same atlas. Use the
authored state artwork directly; do not derive visual states through script
post-processing.
