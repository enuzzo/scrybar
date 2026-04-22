// Embedded DOOM1 IWAD blob.
//
// The .wad file is *not* checked into git (copyright); when DOOM is built
// it must exist at the default SCRYBAR_WAD_PATH location or at a path
// provided via -DSCRYBAR_WAD_PATH="…" in build_opt.h.
//
// When DOOM_SPIKE_ENABLED=0 (CI web-flasher firmware, minimal builds)
// the WAD is *not* incbin'd — we still export the symbols as empty
// placeholders so any linker references from stubbed-out C++ resolve.
// This avoids requiring a valid WAD file on every build environment.

.section .rodata
.align 4
.global scrybar_doom1_wad_start
.global scrybar_doom1_wad_end

#ifndef DOOM_SPIKE_ENABLED
#  define DOOM_SPIKE_ENABLED 1
#endif

#ifndef SCRYBAR_WAD_PATH
#  define SCRYBAR_WAD_PATH "src/doom/doom1.wad"
#endif

scrybar_doom1_wad_start:
#if DOOM_SPIKE_ENABLED
  .incbin SCRYBAR_WAD_PATH
#endif
scrybar_doom1_wad_end:
