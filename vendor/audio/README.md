# vendor/audio

This folder stores the parked legacy audio stack:
- `codec_board`
- `esp_codec_dev`

Status:
- Not in active Arduino sketch compile path.
- Kept in-repo for reference and future reactivation.

Why it was moved:
- Under Arduino sketch build rules, sources under `src/` are compiled automatically.
- `src/audio/*` was compiled even when no runtime audio feature used it.
- Moving to `vendor/audio` keeps code available without polluting active build scope.

How to reactivate intentionally:
1. Define concrete runtime requirements (speaker output, mic input, codecs, I2S mode).
2. Move only the needed subset back into an active compile location (for example `src/audio`).
3. Add explicit integration points in `scrybar.ino` and configuration flags in `config.h`.
4. Validate impact on flash/RAM and boot/runtime stability.
5. Document enabled audio paths in `knowledge/project_knowledge.md` and `knowledge/decisions.md`.

Safety rule:
- Do not restore the whole tree blindly; import only what is required by active features.
