# Companion 0.3.12 (48): sustained network collection

Date: 2026-10-22. Correction to the shared ScryBar/Meguru network-scope preview.

Physical testing of 0.3.11 found network values stuck unavailable while hardware
stats and display delivery remained live. The child nettop process still emitted
bytes, so the previous output-only health check could not recover the collector.

- Track usable-frame progress independently from raw output. Five seconds
  without usable frames triggers bounded recovery even if output continues.
- Keep memory/socket limits and request restart on capacity exhaustion.
- Preserve valid sockets when individual rows are unreadable, marking partial
  coverage. Unknown process rows clear ownership. Fully unusable frames reset
  baselines and cannot refresh health or become fake idle zeroes.
- Distinguish known idle sockets from active wildcard-peer traffic and absent
  counters. Keep the child protocol locale fixed to C.
- Convert curly host-name apostrophes to ASCII for the embedded display font.
- Retain Internet as the initial preference, scope persistence and the same
  filtered wire contract for both displays. No DNS or destination persistence.

## Evidence

| Check | Result |
| --- | --- |
| Native suite without foreground windows | 70 tests passed; existing secure-paste window test excluded |
| Sustained real collector | Two 75-second runs passed; final run 73 measured samples, 79 frames, zero restarts and final usable-frame age 0.156 s |
| Recovery regression fixtures | Output-without-usable-frame timeout, per-row isolation, owner reset, socket cap and measured-idle distinction |
| Actual local TCP traffic | Visible in Local/All and excluded from Internet |
| Host-name wire formatting | Apostrophe regression passed after the final package change |
| Universal DMG | x86_64 + arm64, strict ad-hoc signature, checksum, readonly mounted 0.3.12 (48), executable parity passed |
| Installed runtime/display rates | Pending the coordinated normal quit/install/reopen and live acceptance |
| ScryBar r309 | [Installed and read back](scrybar-r309-installed.md), NVS/outside sectors preserved and clean boot verified |

These sustained checks cover the collector; they do not substitute for the
installed menu app or physical rate display. The exact reason the old nettop
stream stopped producing accepted frames was not recovered from its output;
capacity and malformed-frame recovery are bounded regardless of that trigger.

DMG SHA-256: `b914c73d45569f2a44f707a8b59f0efb775fdbbbccaf7557443032a76d67e1c2`.
Universal executable SHA-256:
`75abba674a7c5d6e0ef8c2f5ef2e5e70f41e6f03160ab2f32bd5cf5bd4d24523`.
The app is ad-hoc signed, not notarized. Source, package, installation, firmware
publication and physical acceptance are recorded separately.
