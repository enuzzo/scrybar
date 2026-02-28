# ScryBar

ScryBar is a compact, open-source ESP32-S3 desk companion with a fantasy twist.

It blends three live surfaces into one always-on horizontal display:
- a readable Italian word clock,
- weather at a glance,
- rotating RSS headlines with QR deep links.

The name comes from *scrying*: seeing what matters from afar.
That is exactly what ScryBar does on your desk.

## Why This Exists

Most dashboards are either noisy or boring. ScryBar aims for a different balance:
- glanceable, not distracting,
- expressive, not gimmicky,
- hackable, not locked down.

Built to be shared, remixed, and improved in public.

## Current Features

- Smooth swipe navigation between views
- Word clock + weather layout tuned for glanceability
- Multi-SSID Wi-Fi fallback
- RSS rotation with source metadata and on-demand QR
- Touch interactions optimized for quick desk usage
- Battery/power behavior instrumentation and diagnostics

## Hardware Target

- Waveshare ESP32-S3-Touch-LCD-3.49
- 16MB Flash + OPI PSRAM profile

## Project Structure

- `ScryBar.ino`: main firmware sketch (Arduino root sketch)
- `assets/`, `src/`, `tools/`: firmware resources, drivers, utilities
- `lvgl_validation/`: isolated validation sketch for UI/touch experiments
- `firmware_readme.md`: firmware setup/flash notes
- `knowledge/`: shared sanitized knowledge for any assistant (public, versioned)
- `.codex/`: local private working memory/session logs (not versioned)

## Open Source Spirit

If you fork ScryBar, make it yours:
- swap feeds and weather locations,
- redesign views,
- add voice/oracle experiences,
- publish your variant and share improvements back.

Small screen. Wide horizon.
