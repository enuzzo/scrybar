# Companion 0.3.14 (50): centered menu-bar network rates

Date: 2026-10-22. Presentation-only correction to the shared Mac Companion.

The owner reported that download/upload were visibly too high relative to the
other menu-bar items. The two-line attributed status-button title is replaced
with one template image. Its visible Core Text glyph bounds are vertically
centered as a block within a 22-point image; the native status button centers
the image. Font size, row spacing, item width, click handling, tooltip and live
accessible value are retained. Template tint follows system appearance and
selection. This does not change the collectors, music provider or device feed.

| Check | Result |
| --- | --- |
| Offscreen native drawing | Four rate combinations at 1× and 2×: visible top/bottom margins differ by at most one pixel, with no vertical clipping |
| Visual preview | Offscreen Retina rendering inspected on a light background; this is not installed menu-bar evidence |
| Universal build | Release build for x86_64 and arm64 passed |
| DMG | Checksum, strict ad-hoc signature, readonly mounted version 0.3.14 (50) and executable parity passed |
| Installed Mac runtime | Owner-confirmed normal quit, build 50 installed in Applications with build 49 preserved, exact hash/signature verified; one background launch, no panel opened; single app/collector process observed |
| Installed menu-bar appearance | Pending owner's visual confirmation: available Cua captures omit the menubar, so offscreen bounds are not treated as installed visual acceptance |
| Actual ScryBar music | Six of six samples match system title/artist/album; elapsed 117–133 seconds, receive age 221–555 ms and 150-pixel artwork present |
| Meguru joint feed | Three of three samples receiving Now Playing, MacStats and Bambu together with 512-pixel artwork ready; Internet preference retained |
| Publication | Source/package uploaded as draft; downloaded asset/source hashes verified; public release waits for the visual alignment result |

DMG SHA-256:
`d77cab3be2d673c9c9794216c2316d442796f90ee4e32eeff95df586735710b3`.
Universal executable SHA-256:
`018a142400fa11b8984c93b3ba034f7a2998fd4edef8cce60f33e2a576514978`.

The app is ad-hoc signed and not notarized. This correction requires no firmware
update, serial access or new dependency installation. No desktop panel was opened
to prepare or check it. The published 0.3.13 package remains unchanged. The source
snapshot for the draft is `e89bb0437f1c062e4c3989b1bad54b6da56364d9`; this record
adds installed checks after packaging without replacing its source ZIP.
