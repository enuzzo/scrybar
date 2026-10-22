# Companion 0.3.12 (48): sustained network collection

Date: 2026-10-22. Correction to the shared ScryBar/Meguru network-scope preview.

Physical testing of 0.3.11 found network values stuck unavailable while hardware
stats and display delivery remained live. The child nettop process still emitted
bytes, so the previous output-only health check could not recover the collector.

- Track usable-frame progress independently from raw output. Five seconds
  without usable frames triggers bounded recovery even if output continues.
  A complete but already stale measurement also triggers recovery.
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
| Sustained real collector | Three 75-second runs passed; exact final revision: 73 measured samples, 78 frames, zero restarts and final usable-frame age 0.204 s |
| Recovery regression fixtures | Output-without-usable-frame timeout, per-row isolation, owner reset, socket cap and measured-idle distinction |
| Actual local TCP traffic | Visible in Local/All and excluded from Internet |
| Host-name wire formatting | Apostrophe regression passed after the final package change |
| Universal DMG | x86_64 + arm64, strict ad-hoc signature, checksum, readonly mounted 0.3.12 (48), executable parity passed |
| Installed Mac runtime | Owner-confirmed normal quit, final48 hash/signature verified independently, single reopen by GUI owner and Internet preserved; same app/collector after more than three minutes |
| ScryBar physical | Two real 640 × 172 frames show LIVE, changing numeric down/up and CPU/GPU, leading-app rates and correct host apostrophe; independently inspected by the Meguru chat |
| Meguru sustained feed | 16/16 fresh `receivedMacStats` checks over 153.1 seconds; owner confirmed LIVE and updated numeric rates on the physical Meguru with Internet selected |
| ScryBar r309 | [Installed and read back](scrybar-r309-installed.md), NVS/outside sectors preserved and clean boot verified |

These sustained checks cover the collector; they do not substitute for the
installed menu app or physical rate display. The exact reason the old nettop
stream stopped producing accepted frames was not recovered from its output;
capacity and malformed-frame recovery are bounded regardless of that trigger.

DMG SHA-256: `cf841a383ce787922d2a39eac028cf8cc65eb8a0d87c9ec89312c325c4733f63`.
Universal executable SHA-256:
`8a1aeb8eaed98d4306e474333626ef3cf5a1dba07238671e279a1cb1b7e3fb73`.
The app is ad-hoc signed, not notarized. Source, package, installation, firmware
publication and physical acceptance are recorded separately.

The installed-runtime check is positive but brief relative to the intermittent
failure observed in 0.3.11. Physical Meguru confirmation was obtained once through the GUI-owning chat,
separately from HTTP/parser freshness.
No extra app launch or desktop interaction was used for the ScryBar captures.

## Publication

Published [Companion 0.3.12](https://github.com/enuzzo/scrybar/releases/tag/companion-v0.3.12)
with the already authorized source and DMG. The immutable source/tag snapshot is
`25a456404627c4f6dba373c0775f9f5098420391` on the isolated
`codex/companion-network-scopes` branch. It predates the owner's final visual
confirmation; this updated record and the release notes carry that acceptance.
The app sources and packaged executable remain unchanged.

DMG, source ZIP and checksum manifests were downloaded again from GitHub; all
asset hashes and every source-file hash matched. Stable firmware Latest remains
v0.9.4, and no website deployment or main-branch push was performed. The source
ZIP/tag retain their original checksums; later documentation does not replace
published assets.
