# src/ Compile-Scope Notes

This folder is in the active Arduino sketch compile path.

What belongs here:
- firmware support code used by `scrybar.ino`
- generated LVGL font assets in `src/fonts/`
- active display/touch drivers

What does NOT belong here:
- parked/legacy third-party stacks not used by the running firmware

Audio stack note:
- The legacy codec/microphone stack was moved from `src/audio` to `vendor/audio`.
- Reason: it was being compiled as part of sketch sources, but not linked/used by runtime features.
- If audio features are reintroduced, restore those sources intentionally (see `vendor/audio/README.md`).
