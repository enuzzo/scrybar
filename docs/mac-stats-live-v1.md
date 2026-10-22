# Live Mac Stats network extension

Shared by ScryBar Companion 0.3.12 (network scopes since 0.3.11; feed since 0.3.10), the ScryBar strip and Meguru. This is an
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
| `netDownBytesPerSec` | number | Receive rate in the selected destination scope |
| `netUpBytesPerSec` | number | Transmit rate in the selected destination scope |
| `networkApp` | string | Display name of the currently busiest app by download + upload, at most 80 UTF-8 bytes |
| `networkAppDownBytesPerSec` | number | Receive rate attributed to that app |
| `networkAppUpBytesPerSec` | number | Transmit rate attributed to that app |
| `networkAttributionAvailable` | boolean | Whether the local per-process collector has a fresh usable sample |
| `networkScope` | string | `internet`, `local` or `all`; absent in legacy senders |
| `networkCoveragePartial` | boolean | Some sockets lack accounting or a peer needed for classification |
| `hostName` | string | Mac's local display name, at most 80 UTF-8 bytes |

The Companion samples and submits at approximately 1 Hz. Starting with 0.3.11,
totals and app rates come from the same per-socket deltas, filtered before both
presentation and delivery. The Mac selector defaults to **Internet**, persists
its preference and offers **Rete interna** and **Tutto**. Changing scope clears
old displayed/transmitted network values immediately; an in-flight computation
for the old scope is discarded. Existing measured baselines can be reused across
scopes without lifetime-counter spikes. Legacy receivers can ignore both new
fields and still receive the filtered rates and filtered leading app.

Internet excludes private, shared/CGNAT (including Tailscale IPv4), loopback,
link-local, IPv6 ULA (including Tailscale IPv6), multicast and active physical
on-link subnets, including public IPv6 prefixes. Local includes those internal
destinations; All additionally includes sockets whose peer cannot be classified.
The original app's tailnet socket is Local; identified Tailscale encrypted
transport/control sockets are excluded in all scopes to avoid counting the same
traffic again through public peer/DERP addresses. This also excludes Tailscale's
own control traffic. Other VPNs and remote proxies can obscure the ultimate peer;
classification describes the peer exposed by macOS, not a decrypted destination.
Do not infer Internet line capacity from current socket traffic.

Unconnected UDP sockets can have a wildcard peer, and nettop can leave byte
counters empty. Neither is assigned an invented destination or a fake zero.
Known attributable rates remain usable with `networkCoveragePartial=true`;
when there is only unclassifiable active traffic and no positive selected
traffic, or no usable byte counters at all, both rates are absent. Known idle
sockets remain measured zero with partial coverage if other sockets lack counters. All includes measured wildcard
traffic but cannot recover absent byte counters. Short-lived sockets absent
from two consecutive samples are omitted rather than billed lifetime bytes.
These are observable socket rates, not a complete physical-link traffic meter.

`networkApp` and its rates are omitted if no app has positive measured traffic
or attribution is unavailable. Display “Idle” only when attribution is available
with no leader; otherwise show “Apps unavailable” or waiting. The Mac popover
holds the fuller ranked app list locally. Destination IPs are used transiently in RAM for classification and baseline
keys; they are never logged, persisted or delivered. DNS resolution is disabled.
No hostnames, URLs, packet contents, browser tabs or domains are requested.

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

One long-lived `/usr/bin/nettop -n -x -c -L 0 -s 1 -J bytes_in,bytes_out`
subprocess returns socket counters and numeric peers grouped under processes.
Process-summary counters establish ownership only and never enter totals.
Duplicate socket observations are not summed. Repeated headers delimit complete
CSV samples. Unreadable rows are isolated and mark incomplete coverage; unknown
process rows clear ownership so their connections cannot be attributed to the
preceding app. A frame without usable counters cannot masquerade as measured idle.
The child locale is fixed to C. Buffer/socket capacity stays bounded; reaching
either limit requests collector restart. A separate progress clock for usable
frames triggers recovery after five seconds even when raw bytes keep arriving.
Only genuinely empty complete frames or frames with usable counters refresh that
clock; restarts use the existing bounded retry delay. Numeric diagnostics expose
sample age/count/restarts without endpoints or app names.

A pseudo-terminal flushes samples promptly and keeps stdin open: `/dev/null`
on stdin caused nettop's event loop to spin on EOF during native testing.
Counters use actual elapsed time from `mach_continuous_time`, which includes
sleep. A counter reset, new socket or gap above five seconds requires a fresh
baseline. Attribution expires after 3.5 seconds; receiver expiry is 15 seconds.
Helper processes aggregate under their outermost app bundle. Tailscale transport
identification uses executable/bundle paths, including its macOS system extension.

Classification references: the installed macOS `nettop(1)` manual,
[Tailscale address allocation](https://tailscale.com/docs/concepts/ip-and-dns-addresses),
and [IANA IPv4 special-purpose registry](https://www.iana.org/assignments/iana-ipv4-special-registry/).
On-link masks come from active `en<number>` interfaces via numeric `getifaddrs`
addresses; default masks, invalid masks and tunnel prefixes are excluded.
