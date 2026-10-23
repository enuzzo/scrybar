# Network graph chronology — 23 October 2026

This follow-up replaces the r313 graph-continuity verdict. The owner noticed
that the network trace moved irregularly and retained interrupted crests.
Sixteen actual-device screenshots at nominal two-second intervals confirmed
that inspecting single frames had missed temporal behavior.

## Reproduced cause

The old history rounded receipts into bins relative to the current clock.
Advancing only the clock could make stored samples collide and discard a peak.
With 55 unchanged known samples using the measured delivery cadence, the
reported peak changed from **1,000 to 514.406** at a 710 ms phase and returned to
1,000 at 840 ms. No new receipt or sample expiry caused that change. It also
changed the shared scale, visibly deforming the rest of the curve.

Rendering was checked independently: r313's partial updates matched full
redraws in 104 cases, and a full-scale continuous trace painted every content
column. The main fault was history reconstruction rather than top clipping.

## r316 implementation

The graph uses native LVGL XY polylines. Each raw rate retains its real local
receipt time, represented in 10 ms units over the same 60-second viewport.
Clock updates every 200 ms translate points by approximately one screen pixel;
they do not rewrite past rates or their peak. The scale can still change when
a real larger sample arrives or an old maximum expires.

The outer page-update guard also admits those Stats deadlines, instead of
waiting for the wall-clock second or a new API payload. Other pages retain their
existing cadence, and the waiting screen does not repeatedly attempt updates.
On a scroll-only deadline, the isolated network helper updates the chart instead
of rerunning hardware rows, sparklines and their styles. An intermediate device
trial exposed that unnecessary full-page work (about 50 ms rendering per frame);
the final path is measured separately below.

Known endpoints connect at their actual spacing, including skipped intermediate
deliveries. A late delivery alone does not prove an unknown measurement.
Explicit unknown values break the affected channel. A freshness-length outage
breaks continuity; pauses longer than 15 seconds clear the old history.
Measured zero remains zero. There is no smoothing average or fabricated rate.

LVGL 8 scatter can omit an isolated known indicator between unknown points.
A chart-local draw callback preserves that marker without joining the unknown
intervals. All seven themes show the expected 3 × 3 pixel indicator.

`NETGRAPH` exposes bounded sample ages, rates and validity flags on explicit
serial request. It prints no host, application, scope or destination identifiers
and adds no periodic logging. Live screenshot sequences use this diagnostic to
distinguish receipt timing from explicit unavailable values.

`SNAP` now advances its byte counter by the result of `Serial.write`, retries
partial/zero writes, and reports a two-second progress stall without emitting a
false completed END. Capture clients verify both payload size and framing.

## Verification

- Strict native tests cover real spacing, narrow peaks, zero/unknown values,
  delivery skips, freshness boundaries, host/scope changes, capacity and rollover.
- A 100-phase clock-only sweep preserves every raw rate, validity flag and peak;
  all X positions undergo the same translation before expiry.
- 49 native temporal renders across seven themes pass the same invariant and
  label bounds. Their sequence was visually inspected.
- All 98 final actual-LVGL transition comparisons match, including the new
  isolated-point callback and independent X arrays for both rendered pages.
- The compiled static RAM increase is **528 bytes**. Temporary history-window
  storage grows by approximately 0.8 KiB; retained raw-history storage is unchanged.
- Chart-only updates match full Stats output pixel-for-pixel across seven themes
  and five ticks. Copied pixels fall from 476,800 to 89,490 per five ticks
  (81.2% less); this is a native dirty-area comparison, not a whole-device CPU claim.
- Extracted native checks cover the orchestrator cadence, waiting-screen guard,
  partial and zero serial writes, progress timeout, rollover and exact payload.

The [fixed-history native sequence](../design/2026-10-23-mac-stats/network-raw-time-sequence.png)
shows the same observations at seven clock phases. Its [numeric verification](../design/2026-10-23-mac-stats/network-temporal-verification.json)
records the temporal invariant and marker checks.

The [chart-only verification](../design/2026-10-23-mac-stats/network-chart-only-verification.json)
is reproducible with `tools/render_mac_stats_native.py --check-chart-only`.

## Hardware evidence

**r316 is installed and boot-verified on ScryBar.** The app image is 6,758,448
bytes, SHA-256 `081154f26dd869d97605387361052ba603c667fd3ecdac1da6aa4903311da8b6`.
All 196 firmware source hashes match the compiled source manifest. The installed
app's flash hash was verified; the write touched only the app partition.

The [16 before captures](../design/2026-10-23-mac-stats/device-r313-sequence.png)
and [16 final captures](../design/2026-10-23-mac-stats/device-r316-sequence.png)
were visually compared. In the final sequence, known crests remain connected
and past observations move coherently to the left. The automatic scale changes
from 6.2 to 5.6 MB/s when the old real maximum expires, as intended. Every accepted
capture contains the complete 220,160-byte framebuffer. One initial short
transfer (82,780 bytes) was rejected and retried; it is excluded from the 16.
Requests targeted two-second intervals, but actual timing is irregular and is
retained in the [measurement record](2026-10-23-network-scroll-measurements.json).

The nearby `NETGRAPH` diagnostics contain no explicit unknown-channel samples
inside these viewports. Receipt intervals reach 8,308 ms during intrusive serial
capture. The final graph connects these known endpoints; the previous 1.75-second
heuristic would have introduced false gaps. The diagnostic dump precedes each
snapshot, so its ages are not claimed to be simultaneous with the captured frame.

Quiet live Stats profiling ran separately, with no screenshot or graph dump in
the measurement window. The representative full 30-second windows show:

| Metric | r315 full-page scroll updates | r316 chart-only scroll updates |
| --- | ---: | ---: |
| Mean render time | 49.945 ms | 23.559 ms |
| Copied pixels | 19,167,360 | 7,868,658 |
| Transfers | 201 | 201 |
| Mean physical transfer time | 15.773 ms | 15.821 ms |

Mean render work falls by 52.8% and copied pixels by 58.9% in these live windows.
Input traffic differs between windows; these are representative runtime profiles,
not a controlled CPU benchmark. Physical full-frame SPI cost remains unchanged.

The final 30.369-second HOME/INFO regression run records **58.3 fps over 1,259
active intervals**, 44.2 transfers/s over the whole window, and 12.839 ms mean
transfer time. The active-interval P95 upper bound is 25 ms. One interval reaches
260.933 ms, so this is not evidence of a sustained locked 60 fps. The display was
returned to Stats after the run. Native equivalence checks and framebuffer review
passed; final optical panel acceptance of r316 remains separate from the owner's
positive physical feedback on r313.

Screenshot requests temporarily block serial/display servicing. Their spacing
therefore tests rendered chronology, not uninterrupted display FPS. Timing
measurements run separately from screenshot and diagnostic requests.

Builds and private rollback images stay outside Dropbox. Installation writes
only the app partition and retains configuration, partition table and filesystem.
No public release, commit or push is part of this local update.
