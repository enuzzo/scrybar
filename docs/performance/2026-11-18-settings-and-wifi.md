# Wi-Fi scanning and settings transfers on ScryBar r325

Measured on the same USB-powered ESP32-S3-Touch-LCD-3.49 on 18 November 2026.
Both builds used the same Wi-Fi network and saved configuration. The production
radio scan API returned a list of networks in all four trials.

## Device responsiveness

Each column shows two independent 12-second Home/Info transition trials.
During each trial the host started one ordinary Wi-Fi scan after about 1.4 s
and sent a read-only USB status command every 250 ms, with at most one outstanding.

| Measurement per trial | r324 | r325 |
| --- | ---: | ---: |
| Longest observed USB round trip (ms) | 2967.0 / 2687.4 | 322.9 / 187.5 |
| Successful USB samples | 33 / 35 | 42 / 44 |
| Wi-Fi scan HTTP round trip (s) | 3.69 / 3.91 | 3.07 / 3.26 |
| Active transition rate (fps) | 60.1 / 60.0 | 60.1 / 60.0 |
| Active frame intervals observed | 330 / 375 | 405 / 495 |
| Missed active pacing periods | 0 / 0 | 0 / 0 |

On r324 the UI callback waited for the radio. On r325 the HTTP worker performs
that wait and sends the same JSON response. Other requests to the single HTTP
worker can still queue during a scan. The software fallback used if the worker
cannot start remains synchronous. USB timings include the host and serial path.
The frame counter covers only active motion. Measuring optical latency would require a separate instrument.

To repeat, wake the screen with `SAVEROFF`, select Home with `VIEW1`, then send
`PERFBENCH 12`. Poll `SAVERSTAT` over the same USB connection while issuing
`GET /api/wifi/scan` over the LAN. Record the longest command round trip and the
final `[PERF][ACTIVE]` and `[PERF][PACE]` lines. Compare firmware on the same board.

## Settings traffic

| Response body | r324 | r325 |
| --- | ---: | ---: |
| First visit, HTML plus main CSS and JavaScript | 60,353 B | 37,963 B |
| Second visit with a populated cache | 60,353 B | 28,089 B |
| CSS and JavaScript transferred on the second visit | Included in HTML | 0 B |

Chromium opened two 390 px pages in the same new headless browser context
against the actual board. Both r325 pages rendered all three settings tabs.
The cache supplied both versioned assets on the second visit. The table excludes
headers and the theme preview on both builds. HTML size varies slightly with
live device information. Body bytes fell by 37% cold and 53% warm.

Five direct GETs per firmware gave median HTML response times of
166.7 ms and 91.1 ms respectively. These samples measure individual HTTP responses.
The r324 benchmark retried one incomplete read. The final r325 benchmark completed on its first attempts.
A separate r325 run completed 120 of 120 requests without retries across HTML, CSS and
JavaScript. One preliminary r325 run also saw a short response. Its cause remains unconfirmed.

## Regression checks

- Verified the app write hash and compared the partition table, NVS and OTA
  data before and after the flash. After reboot, compared saved display,
  timers, network, weather, RSS, branding and word-clock settings against r324.
- Fetched all nine palette samples and both representations of the CSS and
  JavaScript from the board. The bytes matched their sources. Checked the
  versioned cache headers, identity fallback, gzip and the all-encodings-refused
  response using ordinary HTTP request headers. Arduino-ESP32 3.3.6 parses
  these headers with a space after the colon and retains the last repeated
  field line. The checks used the standard browser serialization.
- Chromium and WebKit each exercised the real device HTML on a loopback fixture
  at 320, 390 and 1440 px. The panel suites passed 161 and 161 checks respectively.
  Each engine also passed 9 brightness persistence-state checks with simulated storage.
  Empty, failed and timed-out scans retained the selected SSID and re-enabled
  the scan action.
- Native tests cover worker-owned requests while UI servicing is suspended,
  bounded writes, content negotiation and settings persistence. The firmware
  build checks the packed assets against the editable files.
