# Display performance and Mac Stats — 23 October 2026

This records the r310–r313 optimization stage. The later
[r316 temporal-graph follow-up](2026-10-23-network-scroll.md) supersedes its graph
continuity verdict: screenshot sequences exposed a further time-binning defect
that single-frame inspection did not reveal.

This work improves completed page transitions and removes redundant LVGL work.
It does **not** establish a universal 60 FPS guarantee. A matched rapid-switch
test still runs at approximately the original rate. The owner reports a large
improvement when swiping normally. The **r313 stage was installed and boot-verified**;
the network cadence and peak-padding correction passed native and live-framebuffer
review. The owner later confirmed its static appearance, while screenshot
sequences exposed the additional temporal defect documented in the follow-up.

## Measurements and comparison limits

`flush_hz` counts completed display transfers over the whole measurement window,
including preparation and idle time. `ACTIVE fps` counts intervals between
presentations during page motion; preparation is reported separately. Neither
metric measures the panel's internal scanout rate. Histogram P95 values are
upper bounds, not exact percentile timings.

| Firmware and workload | Active presentation FPS | Whole-window transfers/s | Median preparation | Mean display transfer | Result |
|---|---:|---:|---:|---:|---|
| r309, HOME/INFO requested every 300 ms | Not available | 14.0 | Not available | 16.369 ms | Original fully covered 30-second window |
| r310, completed-transition benchmark, 12 s | 40.5 | 30.8 | About 88 ms | 20.363 ms | Cached drawing exposed a transfer wake-up delay |
| r311, warm completed-transition benchmark, 30 s | 58.8 | 44.3 | 87.679 ms | 12.880 ms | Explicit ISR yield removes the SDK wake-up delay |
| r312, warm completed-transition benchmark, 30 s | 58.8 | 44.1 | About 89.5 ms | 12.796 ms | Staged capture experiment has no demonstrated preparation benefit; rejected |
| r312, matched requests every 300 ms | 13.5–14.1 | 13.5–14.0 | Not comparable | 15.473–15.560 ms | No demonstrated gain over the original rapid workload |
| r313, final warm completed-transition benchmark, 30 s | **59.1** | **44.7** | **86.242 ms** | **12.881 ms** | Installed; 83 transitions completed, live graph inspected |

The r309 rapid workload and the completed-transition benchmark are different
tests. The rapid workload requests another page before preparation plus the
250 ms animation can finish, repeatedly interrupting the cache and falling back
to LVGL. Its fully covered r312 windows remain at 13.5–14 transfers/s, versus
r309's 14. A claim of a threefold improvement under identical conditions would
therefore be unsupported.

The completed-transition benchmark waits for each transition to finish. Its
58.8 active FPS describes the motion after preparation; the 44.3 whole-window
rate includes the roughly 88 ms preparation cost. Cold/initial behavior is less
predictable: the first post-install r311 12-second run recorded 42.7 active FPS,
28.6 whole-window transfers/s and 147.695 ms median preparation. Boot/background
load was not isolated, so the warm result is not a cold-boot guarantee. The
warm r311 histogram's P95 upper bound was 25 ms, with a 26.711 ms maximum interval.

The final run covered 30.316 seconds and 1,355 completed transfers; 1,272
active intervals yielded 59.1 FPS. P95 was at most 25 ms in the histogram and
the maximum interval was 27.051 ms. No touch-owned transition occurred in the
timed workload. The preparation maximum was 102.134 ms.

[Versioned sanitized summary](2026-10-23-measurements.json). Full local diagnostic
files below are ignored captures, retained in this working checkout:

- [r309 matched baseline](../../screenshots/performance-r310/baseline/transition-baseline.json)
- [r310 first completed-transition run](../../screenshots/performance-r310/hardware/first-benchmark.json)
- [r311 first run](../../screenshots/performance-r311/hardware/first-benchmark.json) and [warm 30-second run](../../screenshots/performance-r311/hardware/warm-benchmark-30s.json)
- [r312 warm run](../../screenshots/performance-r312/hardware/warm-benchmark-30s.json) and [matched 300 ms workload](../../screenshots/performance-r312/hardware/comparable-300ms/transition-300ms.json)

## Implementation and costs

LVGL's default 30 ms refresh timer is replaced with 16 ms. Guarded text and
local-style setters skip equal values while retaining explicit refreshes,
mutable text, inheritance and state selectors. The word clock also avoids
repeating font fitting when its sentence has not changed. Font assets,
anti-aliasing and rendering quality are retained.

For eligible adjacent transitions, two complete pages are cached in native
panel order. Their horizontal translation becomes contiguous native-row copies
into the existing DMA buffers. This avoids rerendering both widget trees on
every intermediate frame. The two snapshots cost **440,320 bytes of PSRAM**;
preparation is visible in telemetry and is not hidden in the FPS calculation.
Displayed values remain frozen in those snapshots during the gesture and its
settle animation. Models and timers continue, and ownership returns to LVGL
with a fresh redraw. Unsupported or interrupted states use the normal LVGL path.

The panel remains at **40 MHz QSPI, RGB565, 640 × 172**. The existing sequential
driver path and DMA-buffer ownership are retained. There is no bus overclock,
busy-wait replacement or additional internal DMA allocation.

The installed ESP-IDF 5.5.2 SPI LCD backend discards the completion callback's
Boolean return value. Returning a wake-up flag alone consequently does not
reschedule the task waiting on the transfer semaphore. The callback now consumes
that wake-up locally with `portYIELD_FROM_ISR()` and returns false. On Xtensa,
this requests scheduling after interrupt handling completes. Espressif later
fixed the same backend defect in [upstream commit c4e8f1e](https://github.com/espressif/esp-idf/commit/c4e8f1e70973c24579a5aae50c3a316a465547ec).

## Stats design and chart correction

The selected design keeps the four hardware rows and fills the network half
with directional rates, a shared-scale 60-second history, scope/scale context
and the leading application's own rates. Missing data, measured zero, stale
data and partial coverage remain distinct. The scale represents recent observed
traffic, not connection capacity. History originates from received telemetry,
independently of whether the page is visible.

Physical feedback identified broken curves despite the initial visual matrix.
The one-second-bin implementation was subsequently reproduced with 1.1-second
receipts: ordinary delivery cadence can create periodic empty bins. Chart
padding also needed correction. Final r313 connects consecutive known receipts
up to 1.75 seconds apart, eliminating all five artificial holes in the 55-sample
reproduction. Explicit unknowns and longer gaps remain visible; small internal
insets keep peak strokes inside the widget. Measured real receipt intervals were
870–1,277 ms. The final version uses the original r311 capture path; the unhelpful
r312 staging experiment was removed.

The [native before/after proof](../design/2026-10-23-mac-stats/network-continuity-before-after.png)
and [r313 live device framebuffer](../design/2026-10-23-mac-stats/device-r313.png)
were visually inspected. A second capture after a complete quiet minute confirms
normal trace continuity without benchmark traffic; gaps still represent unavailable
samples. Optical panel inspection and the owner's confirmation of the revised graph
are separate from this framebuffer evidence.

## Verification and installation boundaries

- **105 final native LVGL frames:** 15 states across seven themes, using the actual
  firmware functions and Dosis assets; visible layouts were inspected and label
  bounds checked. The original 91-frame matrix missed the cadence defect;
  the final matrix adds ordinary jitter and explicit-unknown cadence fixtures.
- **98 actual LVGL comparisons:** seven themes, two directions and seven offsets;
  all 110,080 pixels per frame matched normal rendering of frozen page pairs.
  This was rerun against the final chart padding and functions. It checks
  composition fidelity, not physical gesture behavior.
- **146,406,400 pixel-reference comparisons:** exhaustive seam positions,
  production DMA strips, alternate clipping sizes, physical mirror mappings,
  invalid requests and guard regions.
- **Setter regression:** 1,000 equal text/color/opacity batches caused 1,000
  native redraws with ordinary setters and zero with guards. Real changes,
  explicit refreshes, owned mutable text and local-selector behavior still passed.

The installation workflow retained private rollback material and verified the
original r309 application and the first 8 MiB. Candidate writes target only the
application at `0x10000`, preserving the partition table, NVS and filesystem.
Private backups, identifying captures and raw device details are not published.
Build products remain outside Dropbox; no public release or website publication
is established by these local builds and device trials.

Final app-only installation: r313, 6,756,608 bytes, flash hash verified.
The source manifest matches the current firmware files. Original rollback data
and the private installation receipt remain outside Dropbox. No commit, push,
public firmware package or Companion rebuild was performed for this update.
