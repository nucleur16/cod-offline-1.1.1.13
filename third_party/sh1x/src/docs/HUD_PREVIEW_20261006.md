# CoDOL HUD icon artwork and GIF previews

User-directed image generation created new RC-XD, twin-tube SAM, mortar and
full-body running shepherd sheets from supplied references, in the existing
cyan UAV icon style. The prior spy satellite and original UAV are included for
comparison. These are artwork previews; no game assets or server state changed.

Final sheet copies and built-in ImageGen prompts are stored locally under
`output/imagegen/codol_hud_20261006/v2/`. User-facing copies are in
`C:\Users\Michael\Desktop\codol_imaegs\animated\v2\`.

`tools/hud_gif_preview.py` creates GIFs using Pillow, NumPy and SciPy.
The generated sheets are 1774x887 with approximate 8x4 layouts, so the script
extracts the 32 dominant silhouettes and registers them before previewing,
rather than treating the image as a pixel-exact game atlas. Original images
are unchanged. The source UAV has a regular grid and is cropped directly.

Outputs in `C:\Users\Michael\Desktop\codol_imaegs\animated\gifs\`:

- Six 96x96 GIF canvases with icons rendered at 64x64.
- Six 256x256 enlarged GIFs.
- One 768x550 combined animated comparison with both display scales.
- A still preview and `playback.json` recording source paths and chosen timing.

Verification: every individual GIF contains 32 frames with infinite looping;
the combined comparison contains 192 frames. The rendered comparison was
visually inspected. Speeds are preview choices, not measured C1 animation
rates. Seamless playback and final texture packing still require game testing.

## Loop and gallop revision

User reported the mortar was too fast and the generated loops reset poorly;
the user explicitly confirmed the original UAV loop was already correct.

- `animated/gifs_loopfixed/` contains the corrected preview set. UAV pixels,
  frame count and 60 ms timing are verified identical to the original GIFs.
- Mortar now takes 3.84 seconds per cycle (three times its earlier 1.28 seconds).
- The comparison lasts 3.84 seconds: an exact whole number of every icon's
  cycles, eliminating the arbitrary cut at its previous 7.68-second boundary.
- Generated cycles include last-to-first motion interpolation, with pose time
  allocated by visual change. Silhouette ownership prevents neighboring limbs
  and glow from leaking into extracted frames. OpenCV is now required.
- Dog uses eight new exaggerated gallop key poses generated with built-in
  ImageGen, including stretched flight and tight gathering. The prompt and
  source sheet are in `output/imagegen/codol_hud_20261006/v3/`. Its preview
  has 48 frames over 0.96 seconds; single warped silhouettes avoid ghost paws.
- Each decoded revised GIF's seam difference is below twice its median
  adjacent-frame difference. All encoded durations divide the comparison
  duration exactly. Dog motion contact sheet visually checked.

These checks verify GIF timing and boundary continuity, not native C1 playback.

## Crisp rig revision (superseded for dog and satellite)

The previous optical-flow previews still produced doubled edges and inconsistent
silhouettes. `animated/gifs_crisp/` supersedes those previews. The renderer now
uses fixed source artwork and deterministic periodic part transforms; optical
flow, pose crossfades and temporal averaging have been removed.

- UAV uses the same source frames, rendering and 60 ms frame duration.
- RC-XD holds its viewing angle, rotates the two visible wheel hubs, and bounces.
- SAM barrels gently pitch over a fixed base; satellite panels sway in a stable
  front view instead of morphing between separately generated orientations.
- Mortar points down, with separate upward wind strokes, over the retained
  slow 3.84-second cycle.
- Dog uses a new ImageGen parts sheet, two-segment legs and separate near/far
  foot trajectories for an exaggerated gallop. Its cycle remains 0.96 seconds.
  Parts artwork and both generation prompts are retained in local `v4/` output.
- Broad source bloom is removed before spatial rendering. Every output frame
  starts from an empty canvas. GIFs use a shared palette and disposal mode 2.
- Non-UAV animations also export transparent 8x4 PNG atlases with 64 or 128 pixel
  cells. Atlas timing is recorded separately from the 50 fps GIF preview timing.

Verification: both UAV GIF files are byte-identical to their original previews.
All 13 GIFs decode with looping enabled, the intended total durations, and frame
disposal mode 2 throughout. All 10 new PNG atlases have 32 nonempty cells, full
alpha range and clear cell borders. The rig contact sheets and combined still
were inspected at enlarged and HUD scales. `verification.json` records these
export checks in the local output folder.

`tools/hud_rigs.py` contains the rig implementation. Existing output folders
and original assets remain available. These are illustrative animation previews,
not captures or a claim of native C1 playback validation.

## Whole-dog and satellite rotation redo (current)

The user rejected the assembled dog legs and requested a spinning satellite.
`animated/gifs_redo/` contains the revised set. Built-in ImageGen produced new
complete drawings: 16 full-body canine gallop poses and 32 satellite turntable
views. The dog limb rig has been removed from the renderer; it no longer assembles
or stretches separate legs. The satellite's front/side/rear sequence replaces its
previous sway. Pose order and face registration are explicit in `DrawnCycle`.

Both revised animations play their actual drawings at 60 ms per frame, without
pose blending or optical-flow in-betweens. Dog and satellite cycles remain 0.96
and 1.92 seconds respectively. The dog PNG atlas repeats each of its 16 poses
twice to preserve the 32-cell layout. Original UAV, RC-XD, SAM and mortar exports
are preserved. Local `v5/generation.json` records both built-in generation prompts
and sources, with copies of the source artwork retained in the workspace.

Export checks passed for 13 GIFs and 10 transparent atlases. The eight individual
GIFs for the four unaffected icons are byte-identical to the preceding set.
The enlarged pose contacts and final dog atlas were visually inspected. Dog and
satellite loop-boundary pixel differences are below their respective median
adjacent-pose differences; this is an export check, not in-game verification.
