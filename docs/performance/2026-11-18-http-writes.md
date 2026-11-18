# Incomplete HTTP responses on ScryBar

## Reproduction

On 18 November 2026 the original ESP32-S3 returned just 5,618 body bytes for a
normal settings-page GET on release r325. Python raised `IncompleteRead` and
the device recorded one socket write failure. In a separate 160-request run,
one request timed out before it received headers during startup. That event
did not increment the write-failure counter.

For diagnosis, the same USB-powered board ran the r325 write policy with a
serial trace added. A GET for the Tide Table preview returned 5,744 of the
5,816 expected bytes. The board independently logged exactly that offset,
`EAGAIN` (errno 11), and 2,000 ms since the last successful write. Other visits
to the same preview returned all 5,816 bytes. The original HTTP harness was
used throughout, with normal TCP reads for both settings HTML and flash assets.

## Cause and change

The two-second no-progress deadline closed the socket after a response had
already started. At expiry the sender still had 72 valid bytes to send and the
socket was temporarily unable to accept data. Both the settings page and flash
assets use this writer. The recorded error identifies the firmware decision
that truncated the response. It does not identify what delayed TCP progress on
the LAN.

Release r326 retains the two-second deadline before the first successful write.
After any response bytes have been accepted, including the headers of a direct
flash response, each no-progress wait has a five-second limit. A successful
partial send advances the offset by the exact byte count. A permanent socket
error closes immediately. The HTTP worker waits independently of the display
loop. While waiting, the single HTTP worker can delay other requests.

`PERF` reports the number of stalls that resumed after at least two seconds and
the longest recovered interval in milliseconds. A failed response emits one
`[HTTP][WRITE]` line with a fixed route category, phase, bytes accepted and
expected in that phase, socket errno, elapsed time and final stall duration.
The byte counts describe what the local socket accepted. The client must read
and compare its own byte count to establish delivery.

## Internal RAM at startup

A timeout-only candidate proved that some stalls recover, including waits of
2,560 ms and 3,510 ms. It still dropped a JavaScript response after 5,744 of
7,414 bytes and five seconds of backpressure. During these cold starts the AES
driver also reported allocation failures while RSS and Wiki were fetching.
This message comes from a failed aligned allocation in the
[ESP-IDF 5.5.2 AES DMA implementation](https://github.com/espressif/esp-idf/blob/v5.5.2/components/mbedtls/port/aes/dma/esp_aes_dma_core.c#L235-L280).

The final build uses two 32-row LCD transfer buffers. Each stores 172 pixels
per row at two bytes per pixel. Reducing both from 64 rows releases 22,016 bytes
of DMA-capable RAM throughout operation. The physical panel resolution, frame
cache and feed fetch schedule stay the same. Comparison with the five-second
candidate isolates the buffer size change in the firmware. LAN traffic and feed
contents varied between boots.

## Regression tests

The native server test holds a direct flash body after its header for more than
two seconds, then releases the socket and verifies the exact payload. Other
cases cover a permanent stall before a response starts, a five-second stall
after its header, a disconnect after 1,460 transmitted bytes, and a failed
header followed by Arduino's attempted body write. That last case produces
one failure record. Each browser engine also passed 161 panel checks and nine
brightness-state checks on a local fixture serving the device's HTML.

## Final board results

| Check on release firmware | Observed |
| --- | --- |
| Five startup sessions, 27 ordinary GETs per session | 135 complete responses, zero retries and zero socket write failures |
| AES allocation errors in captured startup serial output | 17 across five earlier 64-row sessions, zero across five final 32-row sessions |
| Two 12-second Home/Info transition trials with live Wi-Fi scanning | 60.1 and 60.0 fps during active motion, zero missed active pacing periods in each |
| A separate 160-request series started at power-up | One connection timeout before headers while the network came up. All 159 responses that started arrived in full |
| Device online, 40 consecutive reads each of HTML, CSS, JavaScript and the preview | 160 complete responses without retries |
| Paused reader | A client paused for 2.8 s after headers and then read the entire settings page |
| Install integrity | Verified the app write hash. Partition table, NVS and OTA metadata were byte-identical across the write. Saved configuration and nine theme previews matched after reboot |

The five final startup trials covered the settings document, configuration JSON,
CSS, JavaScript and Tide Table preview. Counts describe the recorded windows.
The larger RAM margin removed the observed allocation failures in this comparison.
It does not establish that memory pressure caused every individual TCP delay.
The cold connection timeout happened before an HTTP response existed and was
also observed on r325. The frame figures measure software pacing during active
motion. They exclude the time between transitions.

## Repeat on a board

Read the address and port from the Info screen. With the USB serial console
at 115200 baud, issue `PERF` before and after the reads. Run the following
from a host on the same LAN, substituting that board's address for
`SCRYBAR_ORIGIN`. Leave curl retries disabled so that a successful second
attempt cannot hide a partial first response.

```sh
curl --silent --show-error --fail --max-time 12 --output /dev/null \
  --write-out '%{http_code} %{size_download} bytes in %{time_total}s\n' \
  "$SCRYBAR_ORIGIN/ui/theme.webp?theme=tide-table"
```

Compare curl's result with the counters and any `[HTTP][WRITE]` line captured
during the same boot. Repeat with the settings document at `/`. An increase
in `recovered_stalls` means a previously stalled socket accepted more bytes.
A complete client read establishes whether the entire response arrived.
