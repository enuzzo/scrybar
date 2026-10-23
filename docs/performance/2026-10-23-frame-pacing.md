# HTTP isolation and frame pacing — 23 October 2026

This follow-up addresses the r316 warm-run outlier and the owner's request for
stable 60 fps without lowering visual quality. It leaves 640 × 172 RGB565,
fonts, antialiasing, chart detail and the 40 MHz QSPI link unchanged.

## Attribution before changing scheduling

The r316 30-second run contained a 260,933 µs active interval, while its longest
display handler was only 18,735 µs and its longest transfer 16,568 µs. The pause
therefore occurred outside the display service. A 50 ms clamp in the LVGL clock
then discarded roughly 211 ms of real elapsed time, extending the animation
instead of advancing to its correct wall-clock position.

r317 adds bounded, in-memory stage counters without changing the scheduling or
HTTP path. Three 30-second actual-device trials with ordinary live Companion
traffic produced:

| Trial | Active fps | Longest active interval | Longest HTTP stage | Longest display stage |
| --- | ---: | ---: | ---: | ---: |
| 1 | 58.9 | 26.527 ms | 11.264 ms | 20.331 ms |
| 2 | 14.2 | 10,083.914 ms | 10,065.514 ms | 18.271 ms |
| 3 | 12.3 | 6,926.181 ms | 6,907.320 ms | 21.545 ms |

These are real stalls, not synthetic delayed-client trials. HTTP attribution is
now direct. The installed Arduino 3.3.6 WebServer parses requests synchronously,
and NetworkClient::write can spend ten one-second select retries waiting for a
receiver. Both previously ran on the visual main loop. The exact transport
operation responsible for each individual prior stall was not separately traced.
Runtime also confirms that the visual loop is core 1, priority 1, sharing that
core/priority with the existing background feed worker.

## Implemented scheduling and HTTP isolation

- A dedicated core-0 HTTP worker parses requests and sends responses. All route
  callbacks, model mutations and LVGL operations retain the original main-thread
  owner; the worker waits while a callback runs.
- Callbacks capture one known-length response in bounded PSRAM. The worker checks
  Content-Length before sending. Direct nonblocking socket writes count partial
  progress and stop a connection after two seconds without progress.
- Fast Mac Stats receipt callbacks may run between frame deadlines. Page
  generation, artwork decoding, Wi-Fi scans and configuration writes wait for a
  transition handoff. An unclaimed request waiting 1.2 seconds returns 503 with
  Retry-After: 1, instead of reusing invalid arguments or blocking presentation.
- Cached presentation uses absolute 16,667 µs slots; late polling cannot shift
  the scheduling epoch. Actual DMA presentations remain separately counted from
  scheduling slots, including slots with unchanged pixels.
- Cached slides evaluate the exact LVGL8 ease-out curve against wall-clock time without running unrelated widget timers. Native comparisons cover58,968 positions, including reverse, partial/cancel paths and rollover. Normal LVGL timers resume at handoff.
- LVGL consumes the complete elapsed wall-clock delta, including long pauses and
  timer rollover. Equal timestamps add zero, rather than inventing a millisecond.
- Invisible widget refreshes, optional logs, radio recovery and scheduled feed
  housekeeping wait for handoff. Their deadlines are not consumed while deferred.
  The visual owner temporarily runs at priority 2 during cached motion and restores
  its original priority afterward. DMA waits continue yielding normally. The last2ms before a frame deadline are reserved for presentation, so a new roughly1ms touch read cannot push that frame past its slot; input resumes after the transfer.

The HTTP adapter intentionally supports the existing known-length send/send_P
routes. Streaming, chunked output, raw socket use and SSE are outside that
contract; unsupported or incomplete captured responses fail explicitly. If the
worker cannot be created, the legacy path is serviced only outside cached motion.

## Validation

Strict native tests cover the clock, absolute deadlines, overrun accounting,
rollover, histogram percentiles and stage attribution. HTTP adapter tests cover
worker/main ownership, cancellation races, ordered repeated requests, fast/deferred
callbacks, binary payload integrity, partial/zero/EAGAIN writes, disconnects,
allocation/capacity failure and unsupported output. ASan/UBSan and ThreadSanitizer
passes supplement the normal compiler checks.

Final hardware installation, performance trials and slow-client checks are
recorded below. These tests measure completed framebuffer transfers, not optical
panel scanout. Incoming HTTP parsing still has the Arduino
server's single-client behavior, but its waiting time no longer belongs to the UI
thread. Explicit configuration operations and Wi-Fi scans remain distinct from
routine animation performance.

## Installed r320 results

The ScryBar identity was verified before an app-only write at0x10000; esptool verified the flash hash. The bootloader, partition table, NVS and filesystem were retained. The compiled app is6,763,888bytes, SHA-256 `5477fa8f7d860bc5a26c6f38ab34a5fa7302c19d68e065b0e9af8fbe5656d236`;198 source hashes matched the frozen build before installation. Credentials and binaries remain in the private external build directory.

Two30-second HOME/INFO trials used ordinary live Companion traffic. A third added delayed HTTP headers and a slow receiver; no USB framebuffer captures ran during profiling.

| r320 trial | Active fps | P95 interval upper bound | P99 interval upper bound | Longest interval | Missed periods | Longest active HTTP stage |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Normal1 |60.1 |17.75ms |18.75ms |20.562ms |0 |1.561ms |
| Normal2 |60.0 |18.00ms |19.50ms |20.150ms |0 |1.505ms |
| HTTP stress |60.0 |18.00ms |19.50ms |20.535ms |0 |1.994ms |

The active interval counts are1,245 /1,260 /1,245. P95/P99 use250µs histogram bins, not exact order statistics. Maximum frame-start lateness was807 /37 /1,459µs. The aggregate frame rates count completed DMA transfers during cached motion. They exclude cache preparation, stationary time and the initial/final redraw, so they do not mean every part of a page change runs at60fps. Whole-window presentation was44.1–44.6Hz with those costs included. Cache preparation remains roughly85–105ms in this run.

All five400ms delayed-header requests returned200. The root HTML request returned all47,850 response bytes after a3-second read pause. HTTP worker counters reported no allocation, response, write or busy failure, and at least1,440bytes of worker stack remained. Global callback maxima include expensive idle operations; they are not the active-frame HTTP-stage values above. The slow receiver test did not establish forced socket backpressure on hardware; native tests force partial writes, EAGAIN and the no-progress deadline.

The r318 deadline experiment still skipped6–14 periods per trial. r319 corrected completion-slot accounting and reached59.9fps with zero missed periods; r320 reserved the imminent deadline window and reached the values above. No rendering detail, font antialiasing, native resolution or link clock was reduced.

The owner physically tested swipe, cancellation and a held drag on r320 and replied **“Ora è continua”**. This is the user's physical acceptance of the interaction, separate from native pixel comparisons and DMA timing. No optical high-speed-camera claim is made. The longest observed individual interval remains20.562ms, so this is nominal60fps with small jitter, not a guarantee that every panel refresh takes exactly16.667ms.

Reproducible structured measurements: [frame-pacing measurements](2026-10-23-frame-pacing-measurements.json). Raw sanitized diagnostics remain in `screenshots/performance-r320/hardware/normal-and-http-stress.json` (ignored local evidence). Later design changes require a final firmware regression run before inheriting this result.

## Regression after the visual and WebUI changes

The final native palette, copy, layout and Control Surface changes were installed
as r321, then r322 added persistent-brightness metadata. Both app-only writes
verified the flash hash and retained the bootloader, partitions, NVS and filesystem.
The native display paths are byte-equivalent between these versions, as documented
in [the r322 comparison](../design/2026-10-23-display-quality/r322-equivalence.md).

Each version completed one additional 30-second HOME/INFO hardware trial with live
Companion traffic, five delayed-header requests and a receiver paused for three
seconds. No framebuffer captures ran during profiling.

| Firmware | Active fps | P95 upper bound | P99 upper bound | Longest interval | Missed periods | Whole-window presentations |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| r321 | 60.1 | 17.75 ms | 19.00 ms | 20.039 ms | 0 | 44.3 Hz |
| r322 | 60.0 | 18.00 ms | 19.50 ms | 20.779 ms | 0 | 44.3 Hz |

The final r322 app is 6,782,480 bytes, SHA-256
`45f37e04dcf69504a84424438bffadb879b8261cee9eea501bb599da819010b7`.
Its `.ino` SHA-256 is
`10b0ac004e8942950c44db75d08fd33ec1b31a78509d1f42f2d0ebd285209dba`.
All 198 frozen source hashes matched before installation. The HTTP worker reported
zero busy, allocation, response and write failures; its response capacity remained
sufficient for the 66.7 kB Control HTML, with at least 1,440 bytes of worker stack
remaining. These counters are cumulative, while the timing table covers only the
stated trial.

The r322 brightness test used the actual HTTP endpoints and browser. An unsaved
live preview remained visibly dirty after a fresh GET. Saved levels survived a
hardware reset; the original levels were then restored and confirmed after a
second reset. The configuration returned by `/api/config` remained identical.
Private configuration snapshots and reset logs stay outside the repository.

Structured results are included in `post_design_hardware_regressions` in the
[measurement file](2026-10-23-frame-pacing-measurements.json). The same limits as
r320 apply: these are completed DMA transfers during active motion, not optical
scanout or a guarantee that every interval is exactly 16.667 ms. User acceptance
of r320 touch behavior is not relabelled as a new physical acceptance of r322.

## Final installed Companion regression

With Companion 0.3.15 (53) installed and its native artwork reader active, the
unchanged r322 completed one final 30.246-second HOME/INFO trial. It included the
same delayed-header and slow-receiver workload, plus natural metadata/artwork
traffic. No framebuffer capture ran during this trial.

| Active intervals | Active fps | P95 upper bound | P99 upper bound | Longest interval | Missed periods | Whole-window presentations |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1,170 | 60.0 | 18.00 ms | 19.00 ms | 20.443 ms | 0 | 41.3 Hz |

The pacer counted 1,248 slots with maximum start lateness of 1,441 microseconds.
All five delayed-header requests and the receiver paused for three seconds
returned HTTP 200. Worker counters reported zero busy, allocation, response or
write failures, and at least 1,440 bytes of stack remained. These are cumulative
worker counters; expensive idle callbacks are not active-animation stage timing.
The lower whole-window rate includes cache preparation and handoffs, which remain
outside the active-motion fps metric. No graphical quality setting changed.

Separate read-only checks on the installed build53 found fresh, playing, in-sync
metadata and a received 150 x 150 cover in all four samples. A private ScryBar
framebuffer from build52 was visually inspected and showed the matching current
cover; build53 changes only reader termination, not capture or image transport.
Meguru's production metadata and artwork endpoints acknowledged the current-track
probe; visible physical Meguru pixels remain a separate owner observation.
Structured results are stored under `final_companion_hardware_regression` in the
measurement file.
