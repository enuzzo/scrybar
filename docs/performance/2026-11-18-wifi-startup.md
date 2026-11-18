# First settings connection after a hardware reset

On the original USB-powered ScryBar, the initial settings GET timed out
during TCP connect in all six r326 trials. A second GET, issued as soon as
the serial console announced the settings service, returned HTTP 200 with
the full page in every trial. The listener announcement followed IP assignment
by 4–25 ms.

## Why the connection was late

In every r326 trial the driver reported an expired association on the first
configured network. Arduino-ESP32 3.3.6 initiated a same-network retry, while
the firmware immediately advanced to the second credential. Two unavailable
networks were tried before the board returned to the original network and
obtained its address. The host's pending connect then hit its ten-second
deadline before it sent an HTTP request. A fresh connection after readiness
already worked on r326. [Arduino-ESP32 station event handling](https://github.com/espressif/arduino-esp32/blob/3.3.6/libraries/WiFi/src/STA.cpp#L131-L165)

r327 allows the driver to finish an association retry during the active
attempt. A voluntary leave during that attempt also keeps its existing deadline.
Other failures, including an absent access point, retain credential rotation.
The ten-second station attempt deadline still advances to the next credential.

The firmware now checks that the TCP listener is open before marking the
service ready or advertising it over mDNS. If binding or listening fails, it
releases the descriptor and retries after one second. Native tests induced a
listener failure and recovery. The hardware traces above showed a delay before
the IP address was available.

## Before and after on the same network

| USB reset trial | r326 service ready (s) | r327 service ready (s) | r327 initial GET start to full page (s) |
| --- | ---: | ---: | ---: |
| 1 | 13.92 | 8.29 | 2.09 |
| 2 | 13.92 | 9.44 | 3.24 |
| 3 | 13.92 | 9.45 | 3.27 |
| 4 | 13.90 | 9.48 | 3.18 |
| 5 | 13.91 | 9.41 | 3.26 |
| 6 | 13.91 | 9.40 | 3.16 |

Median time to the service announcement fell from 13.92 s to
9.43 s. All six initial r327 GETs completed, as did all six
post-readiness control GETs. The preferred network was used in every final
trial. The expired association still occurred on 5 of six final boots and
recovered within the original attempt.

## Test method

For each sample, reset the board over USB without changing flash or credentials.
The host's monotonic clock started when the serial port reopened after the
hardware reset. The board stayed powered throughout. Track the firmware banner,
station attempts, disconnect reasons, address acquisition and listener announcement.
At the first acknowledged serial status command, open a TCP socket to the
known numeric LAN address and request the settings document with a ten-second
connect timeout. Independently repeat the GET at the listener announcement.
Record connect, header and body completion separately. Check status, declared
length and firmware identity. The final column measures receipt of the initial response body. No DNS resolver was involved in the timed requests.

These timings describe this access point, DHCP service, client and device.
They do not establish the cause of the initial association expiry at the radio.

## Checks after installation

The application flash hash matched the local device build. The partition table,
settings storage and OTA selection data were identical before and after flashing.
The settings API returned the preserved configuration. All nine theme previews
passed, and 40 consecutive reads of each of the settings page, JavaScript,
stylesheet and a theme preview completed on the connected device (160 total).
Two 12-second Home/Info runs during Wi-Fi scans recorded 60.0 and 59.9 FPS.
