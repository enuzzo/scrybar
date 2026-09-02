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
