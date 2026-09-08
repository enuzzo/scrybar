# Bambu Lab LAN integration

ScryBar monitors a Bambu Lab printer through the macOS companion. Bambu Studio can remain open and connected at the same time; ScryBar does not automate or proxy Bambu Studio itself.

## Architecture

```text
Bambu Lab printer
  └─ MQTT over TLS, LAN port 8883
       └─ ScryBar Companion for macOS
            ├─ event sounds on the Mac
            └─ sanitized HTTP JSON → ScryBar /api/bambu
```

This split intentionally keeps the printer IP, serial number, LAN name, and LAN access code off the ESP32 and out of the repository. The access code is stored in macOS Keychain. The serial and printer name are the stable local identity; the last working IP is only a replaceable cache in the companion preferences.

## Setup

1. Put the Mac, printer, and ScryBar on the same trusted LAN.
2. Open **ScryBar Companion → Settings → Bambu Lab printer**.
3. Select **Search Again**. The companion first uses Bambu SSDP discovery on UDP port 2021. If the printer stays silent, it checks only the Mac's private local `/24` for the Bambu MQTT plus FTPS/camera service fingerprint.
4. If the companion finds more than one printer, select the printer that you want to monitor. Discovery supplies its name, model, IP address, and serial number.
5. On the printer, open its Network/WLAN settings and view the LAN access code. Enable LAN Only Mode or Developer Mode only if the installed firmware requires it, after reading the warning on the printer.
6. Enter the access code, leave event sounds enabled if desired, then choose **Save & Connect**.
7. Keep the companion running in the menu bar. It reconnects automatically after LAN interruptions. If DHCP changes the address, discovery updates the cached IP and reconnects the saved serial automatically.

If discovery does not find the printer, select **Setup Help** in the companion. The same panel includes a new search action and a manual IP/serial fallback. Each printer serial has a separate access-code entry in macOS Keychain.

The companion connects to `mqtts://<printer-ip>:8883` with username `bblp`, subscribes to `device/<serial>/report`, and requests a full status snapshot. The printer's local certificate is not anchored in the macOS public trust store, so certificate-chain verification is disabled for this LAN connection. TLS encryption and the printer access code are still used. Run this only on a trusted local network.

## Displayed print data

| Data | Printer field |
|---|---|
| Connection and print status | `gcode_state` |
| Current stage | `stg_cur` |
| Job / plate name | `subtask_name`, fallback `gcode_file` |
| Progress | `mc_percent` |
| Remaining time | `mc_remaining_time` |
| Current / total layer | `layer_num`, `total_layer_num` |
| Nozzle temperature / target | `nozzle_temper`, `nozzle_target_temper` |
| Bed temperature / target | `bed_temper`, `bed_target_temper` |
| Chamber temperature | `chamber_temper` |
| Error and alert count | `print_error`, `hms` |

Printer reports are partial updates. The companion merges them into one current snapshot before sending it to ScryBar. The display marks the feed offline when no fresh companion update has arrived for two minutes.

## Event sounds

Sounds are emitted once per state transition by the macOS companion, not on the ESP32 speaker:

| Event | Trigger | macOS system sound |
|---|---|---|
| Print started | transition into `RUNNING` | `Glass` |
| Paused, blocked, or faulted | `PAUSE`, `print_error`, or a new HMS alert | `Basso` |
| Print finished | transition into `FINISH` or `COMPLETED` | `Hero` |

The first status received after connecting establishes a baseline and stays silent, avoiding a false start sound when the companion opens during an existing print.

## HTTP payload sent to ScryBar

`POST /api/bambu` accepts a JSON snapshot such as:

```json
{
  "connected": true,
  "status": "RUNNING",
  "stage": "Printing",
  "jobName": "plate_1",
  "progressPercent": 42,
  "remainingMinutes": 68,
  "currentLayer": 57,
  "totalLayers": 133,
  "nozzleTempC": 220,
  "nozzleTargetC": 220,
  "bedTempC": 65,
  "bedTargetC": 65,
  "chamberTempC": 31,
  "gcodeFile": "plate_1.gcode.3mf",
  "printType": "cloud",
  "wifiSignal": "-48dBm",
  "speedLevel": 2,
  "speedPercent": 100,
  "coolingFanPercent": 80,
  "heatbreakFanPercent": 100,
  "auxiliaryFanPercent": 0,
  "chamberFanPercent": 0,
  "filamentSensorState": 1,
  "activeTray": 0,
  "targetTray": 0,
  "amsStatus": 768,
  "activeFilamentLabel": "Bambu PLA Basic",
  "activeFilamentColorHex": "FFFFFFFF",
  "activeFilamentRemainingPercent": 76,
  "amsHumidityPercent": 30,
  "amsTemperatureC": 25,
  "errorCode": 0,
  "hmsCount": 0,
  "updatedAt": "2026-08-31T12:00:00Z"
}
```

It contains no printer password or LAN access code.

The Companion also retains the complete bounded AMS snapshot (up to four units
and four trays per unit), including slot identity, material type/name, RGBA
color, remaining percentage, active slot, humidity and temperature. ScryBar's
`640x172` view keeps the primary print data fixed and rotates the denser LAN
telemetry every five seconds; errors and HMS alerts always take precedence.

Preview/camera frames and cloud-only slicer estimates are intentionally not
invented when the LAN report does not provide them. Their fields remain empty
until a supported source supplies real values.

## Reference and license boundary

The ignored `_references/BambuSphere` checkout was used to study observable protocol behavior and the categories of telemetry exposed by the printer. BambuSphere is distributed under the **Federation Non-Commercial License (FNCL) v1.1**: commercial use is prohibited without separate written permission, redistribution carries notice and change-marking duties, and the license grants no trademark or patent rights.

ScryBar's implementation is an independent rewrite using Apple `Network.framework`; it does not copy BambuSphere source, UI, graphics, sounds, or assets. Keep the reference checkout ignored and do not redistribute it as part of ScryBar.

## On-demand camera (v0.9.3 / r306 / Companion v0.3.4)

Release acceptance: real A1 frames and the r306 header were verified on the
physical ScryBar; the user confirmed light-on and light-off on camera close.
The universal Companion DMG was mounted and verified as v0.3.4 (39), with both
architectures, valid ad-hoc signature and Applications link. All 24 non-clipboard
Swift tests and the host camera-touch regression passed. The macOS clipboard
integration test was excluded because of its earlier graphical-service failure.
Network-loss and simultaneous Mac/device behavior have automated lifecycle
coverage, but have not received a complete physical acceptance run. The dated
checkpoints below preserve the implementation history and earlier pending gates.

The A1/P1 LAN camera is available from **Cam** in ScryBar's Bambu header and
from the video button in the companion's Bambu section. The Mac opens a separate
camera window; Bambu Studio is not needed. The device camera view has only a light action and Back; the Mac retains its
Start/Stop controls. Light control is explicit: opening the camera does not
turn the lamp on.

The companion owns one TLS connection to port 6000 for both viewers. It decodes
bounded JPEG frames, displays the original aspect ratio on the Mac, and sends a
letterboxed 240×135 RGB565 image to the device. No camera credentials are sent
to ScryBar, and no images are saved. X1/H2 RTSPS cameras are not implemented by
this transport. A1's native low camera cadence remains the limiting factor.

ScryBar's `GET /api/bambu/camera` supplies a fresh session ID, active flag and
light revision. The companion polls approximately once per second, then posts
status and only a new image to the same endpoint. The device rejects inactive or
outdated sessions and malformed/wrong-sized images. A missing device expires
its viewer lease after five seconds; a missing companion stops the device view
after ten seconds. Stale frames are hidden after twelve seconds. Back, changing
pages and entering the screensaver close the device viewer. After a connection
error, return with Back and open CAM again.

Closing one viewer preserves the other. Closing the last viewer stops the LAN
stream and requests lamp-off. Light cleanup is saved in companion preferences
before a lamp command is sent, retried while pending, and cleared only after a
matching printer's `lights_report` reports off. An unconfirmed cleanup blocks a
new stream. Normal application quit waits up to five seconds; if the printer
cannot confirm off, the app offers to keep retrying or quit with recovery saved
for next launch. A power loss, forced process kill or unreachable printer cannot
guarantee immediate lamp-off. Printer identity changes are blocked while a
camera session or cleanup is active; changes to the same printer's IP remain
supported.

The protocol implementation is independent and based on the published
[OpenBambuAPI LAN video description](https://github.com/Doridian/OpenBambuAPI/blob/main/video.md).
Light control uses `system.ledctrl` / `chamber_light`, as documented by the
[Home Assistant Bambu integration](https://github.com/greghesp/ha-bambulab/blob/main/custom_components/bambu_lab/pybambu/commands.py).
No BambuSphere code or proprietary plugin is included.

Validation on 2026-09-08: Swift protocol, shared-viewer lifecycle, lease expiry,
cleanup recovery and RGB565 tests pass. Firmware and the macOS app compile.
The unrelated macOS clipboard test requires a graphical service unavailable in
the build sandbox and was excluded after that environment failure. Real A1
frames, touchscreen placement, close/reopen behavior, light readback, network
loss and concurrent viewing still require the updated firmware and companion
running against the printer. No hardware flash or deployment is implied.

Live checkpoint, 2026-09-08: the connected USB device was identified before
flashing by its ScryBar 640×172 framebuffer and live Bambu telemetry. The camera
firmware was uploaded to `/dev/cu.usbmodem83101`; esptool verified the written
hash and reset the board. The post-flash framebuffer shows the Cam button and
the new camera HTTP endpoint responds. The new Debug companion process started,
but its initial Keychain read is waiting for user authorization; camera and lamp
acceptance remain pending until that prompt is resolved.

Follow-up: the user completed Keychain authorization and confirmed that the
camera displayed successfully on ScryBar. Automatic light-off still awaits an
explicit real-printer check. Firmware r303 makes the header action easier to
find: a 96×28 solid Bambu-green button, dark semibold CAM label and video icon;
the touch target matches the wider button. Before the follow-up upload, the
chip MAC was matched to the device identified during the first flash.

Firmware r303 upload completed with hash verification. Its live 640×172
framebuffer was inspected: the larger green CAM button is legible and does not
overlap the stage or status badges. Bambu telemetry resumed (RUNNING, 95%,
layer 75/85). Real lamp-off confirmation remains open.

### Navbar and touch follow-up (r304)

The user reported CAM taps no longer responding after r303. CAM had been using
the generic 350 ms / 12 px tap classifier, including the delayed release scan,
instead of owning its gesture like the other display buttons. Camera controls
now capture touch-down, suppress page dragging for that press, debounce release
for 70 ms, and validate the actual contact duration and release target. A press
up to 1400 ms with up to 32 px movement inside the same control is accepted;
ending outside or on another control is cancelled. Serial diagnostics report
camera press and release acceptance. Drawing and hit-testing share geometry in
`src/bambu_camera_controls.h`. The host regression check is:

```sh
clang++ -std=c++17 tests/bambu_camera_controls_test.cpp -o /tmp/scrybar-camera-controls-test
/tmp/scrybar-camera-controls-test
```

The navbar retains its 30 px height: Printing (210 px), CAM (112 px), and status
(152 px) use the same 18 px semibold font and 28 px boxes, with 12 px gaps.
Printing and status have a green outline; CAM keeps its solid green fill. The
redundant status dot is removed. Physical tap acceptance must be checked after
flashing; passing geometry tests alone does not prove the touchscreen behavior.

r304 was uploaded after matching the ScryBar chip identity; esptool verified the
flash hash. A live framebuffer then showed the camera open with a real image of
the ongoing print, establishing that the new touch path can open the viewer.
The first navbar capture occurred mid-swipe and was rejected as layout evidence.
Automatic lamp-off and the settled navbar are being checked separately.

### Camera acceptance and polish (r305)

The user confirmed on the real A1 that the light button turns the lamp on and
closing the camera turns it off. This closes the basic physical lamp lifecycle
check; abrupt network/power-loss recovery remains a separate acceptance case.

The device viewer now has only Back and a light command. A separate label shows
`Light: ON` (yellow indicator) or `Light: OFF` (gray indicator), while the button
says `Turn off` or `Turn on`. Unknown state is shown explicitly and pending
commands display `Switching...`; requested state is not passed off as reported
state. Smart punctuation received from the companion is converted to ASCII
ellipsis/hyphen before rendering with the embedded font.

The startup logo asset is little-endian RGB565, but the display canvas requires
swapped bytes. Its old verbatim copy produced colored speckles in the gray edge
pixels. r305 swaps bytes only for a swapped canvas, preserving the original
283×152 asset and placement. `SPLASH` is available on serial for framebuffer QA
without another boot.

r305 upload completed with hardware identity and flash hash verification. The
SPLASH framebuffer shows clean grayscale antialiased logo edges. The camera
framebuffer shows the real printer image, gray `Light: OFF` state, `Turn on`
action and Back, with no Stop button. Camera was then closed with CAMBACK.

r306 displays the printer's `FINISH` status as `FINISHED` in the header, so it
reads as a completed state. The protocol value remains unchanged. Build and
identity-checked upload passed; the device framebuffer confirms the full label
fits the existing status box at 100% completion.
