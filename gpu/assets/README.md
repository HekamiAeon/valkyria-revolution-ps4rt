# Valkyria overlay media

Only the reviewed overlay files listed below are tracked in Git:

- `gpu/assets/valkyria_overlay.png`: portrait for the **inside** of the
  translucent wine-red menu. Max 4096x4096 PNG.
- `gpu/assets/sounds/navigate.wav`: scrolling.
- `gpu/assets/sounds/confirm.wav`: Cross/confirm and open.
- `gpu/assets/sounds/cancel.wav`: Circle/cancel.
- `gpu/assets/sounds/open.wav`: optional open cue; if missing, synthetic fallback.

The already supplied Valkyria recordings can be renamed:
`Valkyria_Scroll.wav` → `navigate.wav`,
`Valkyria_Confirm.wav` → `confirm.wav`,
`Valkyria_Cancel.wav` → `cancel.wav`.

The clean build copies these files from its committed-source snapshot into
the staged test bundle. The runtime packager copies them from that verified
bundle into the player archive, with hashes in `MANIFEST.json`. The sanitized
source archive contains the tracked media as well. Other local artwork and
audio are not exported.

Verify provenance and permission to redistribute game-derived art/audio
before publishing any binary release that includes these files.
