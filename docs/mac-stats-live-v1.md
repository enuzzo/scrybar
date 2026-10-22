# Live Mac Stats network extension

Shared by ScryBar Companion 0.3.10, the ScryBar strip and Meguru. This is an
additive extension of protocol v1, delivered to `POST /api/mac-stats` only when
the display advertises `mac-stats` in `GET /api/companion` (or the established
legacy ScryBar fallback applies).

## Wire contract

Existing fields remain unchanged: optional `cpuTempC`, `gpuTempC`,
`cpuUsagePct`, `gpuUsagePct`; required `ramUsedGB`, `ramTotalGB`, `diskUsedGB`,
`diskTotalGB`; ISO8601 `updatedAt`.

All new fields are optional. JSON absence/null means unknown; numeric zero is
a valid measured idle rate. Values are finite and nonnegative. All rates are
**bytes per second**, using decimal display units (1 kB = 1,000 B).

| Field | Type | Meaning |
| --- | --- | --- |
| `netDownBytesPerSec` | number | Aggregate receive rate on active physical network interfaces |
| `netUpBytesPerSec` | number | Aggregate transmit rate on active physical network interfaces |
| `networkApp` | string | Display name of the currently busiest app by download + upload, at most 80 UTF-8 bytes |
| `networkAppDownBytesPerSec` | number | Receive rate attributed to that app |
| `networkAppUpBytesPerSec` | number | Transmit rate attributed to that app |
| `networkAttributionAvailable` | boolean | Whether the local per-process collector has a fresh usable sample |
| `hostName` | string | Mac's local display name, at most 80 UTF-8 bytes |

The Companion samples and submits at approximately 1 Hz. Per-process attribution
uses a separate source from interface counters, so its sum need not equal the
physical totals (VPNs, OS accounting, sample windows and traffic on other
interfaces can differ). Do not infer internet line capacity from current traffic.

`networkApp` and its rates are omitted if no app has positive measured traffic
or attribution is unavailable. Display “Idle” only when attribution is available
with no leader; otherwise show “Apps unavailable” or waiting. The Mac popover
holds the fuller ranked app list locally. No destination IPs, hostnames, URLs,
packet contents, browser tabs or domains are collected for this feature.

Legacy payloads still populate CPU/GPU/RAM/disk with unknown network values.
Receivers must clear missing optional fields on every complete snapshot, reject
invalid numbers/oversized names, use local monotonic freshness, and expire the
whole snapshot after 15 seconds. Unknown and stale values must not become zero.
The Companion's existing independent delivery lanes drop intermediate samples
when a destination is busy, preserving other feeds and displays.

## Visual direction

- Mac menu bar: compact download/upload rates; existing track context remains in
  the original menu item and popover. A second compact item shows both rates;
  Settings can hide it. Clicking either opens the existing popover with network
  rates and ranked apps above the compact hardware statistics.
- ScryBar 640 × 172: CPU/GPU and RAM/disk share the left area; large download/upload
  values and the leading app occupy the right area.
- Meguru 466 × 466: a native circular Mac Stats view, using the same payload and
  explicit waiting/live/stale states. No additional Mac app or collector.

The existing music-only policy and high-resolution artwork protocol remain
unchanged. Publication and physical acceptance are recorded separately from
local builds and renderer evidence.

## Collector implementation

`NetworkStatsProvider` reads 64-bit physical-interface counters using the public
`IFMIB_IFDATA` sysctl and excludes loopback, tunnels, peer-to-peer and bridge
interfaces to avoid counting the same packet at multiple layers. The public
[Apple interface MIB definition](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/net/if_mib.h)
uses `if_data64`; the `getifaddrs` interface data is narrower.

One long-lived `/usr/bin/nettop -P -n -x -c -L 0 -s 1 -t wifi -t wired
-J bytes_in,bytes_out` subprocess returns only process names and byte counters.
A pseudo-terminal flushes each CSV sample promptly and keeps stdin open: using
`/dev/null` on stdin caused nettop's event loop to spin on EOF during native
testing. The corrected collector measured about 0.089 CPU seconds over 8.63 s
(about 1% of one core), with approximately 4.6 MB RSS on this Mac in that short
probe. This is collector overhead, not a benchmark of the whole Companion.

Counters use actual elapsed time from `mach_continuous_time`, which includes
sleep. A counter reset, new interface/process or gap above five seconds requires
a fresh baseline. Attribution expires after 3.5 seconds; the receiving device's
whole-payload expiry remains 15 seconds. Empty complete CSV samples are idle;
malformed blocks are unavailable. Helper processes aggregate under their
outermost app bundle. VPN traffic may appear under the VPN process instead of
the originating app. Unknown values remain absent and never become zero.
