# Valkyria overlay media placeholders

The folders are intentionally empty in Git. Place media files locally:

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

PNG and WAV media are ignored by Git. The clean build copies them from this
folder into the staged test bundle. The runtime packager copies them from
that tested bundle into the downloadable archive, with file hashes in
`MANIFEST.json`. The source archive contains only code and these instructions.

Verify provenance and permission to redistribute game-derived art/audio
before publishing any binary release that includes these files.
