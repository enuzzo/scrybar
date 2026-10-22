# Companion 0.3.11 (47): network destination scopes

Date: 2026-10-22. Additive update to the shared ScryBar/Meguru Mac app.

- Persistent **Internet / Rete interna / Tutto** selector in the network section;
  Internet is the default when no preference is stored.
- Filter totals, ranked apps and display payloads from the same socket deltas.
  Changing scope clears old network values and rejects old-scope computations.
- Exclude LAN, Tailscale shared/ULA addresses, loopback and link-local from
  Internet; include active physical on-link prefixes, including public IPv6,
  in Local. Exclude identified Tailscale transport to avoid duplicate traffic.
- Explicit partial coverage for wildcard peers and missing native byte counters.
  Transient destination classification stays in RAM; no DNS, endpoint logging,
  persistence or delivery. Socket accounting can omit short-lived connections
  and differs from physical-link totals; see the [contract](../mac-stats-live-v1.md).
- Optional `networkScope` and `networkCoveragePartial` fields preserve receiver
  compatibility; current ScryBar r309 and Meguru M0.15.0 need no scope update.

## Verification

| Check | Result |
| --- | --- |
| Swift suite without foreground UI | 66 tests passed; existing secure-paste window test intentionally excluded |
| Native real collector | Fresh measured totals and app attribution observed |
| Native local TCP transfer | Own-process traffic observed in Local/All and excluded from Internet |
| Fixtures | Mixed WAN/LAN/tailnet flows, IPv4/IPv6, public IPv6 on-link, socket ownership/duplicates, missing counters, unknown peers, persisted preference, wire scope |
| Tailscale identity | Real system-extension executable path readable and recognized by transport predicate |
| Universal DMG | x86_64 + arm64, ad-hoc signature strict validation, readonly mounted version 0.3.11 (47), executable parity and DMG checksum passed |
| Installed Mac GUI | Installed after normal quit; selector/default/persistence verified, but a prolonged real test found persistent stale network data while hardware delivery stayed live. Corrected by 0.3.12 |
| ScryBar USB | Owner-confirmed handoff completed; [r309 installed and read back](scrybar-r309-installed.md). Network acceptance depends on the Companion correction |

The package is ad-hoc signed, not notarized. The 0.3.10 release and its immutable
assets remain available independently. Firmware and website publication are
separate operations.

```sh
SCRYBAR_NETWORK_LIVE_SMOKE=1 swift test --build-system native --disable-sandbox \
  --scratch-path <EXTERNAL_SCRATCH> --skip secureAccessCodeFieldHandlesCommandV
SCRYBAR_SDK_STAT_CACHE_ENABLE=NO SCRYBAR_DERIVED_DATA=<EXTERNAL_BUILD> \
  bash script/build_companion_dmg.sh
```

DMG SHA-256: `c1c73e5624bda95f9df69bfeab80331cf175586e7d37f148c591656542b83cbd`.
Universal executable SHA-256:
`8ad74756edacc61a6deddd47739bce546a99d3b210b0f35ef449e83ed78256a9`.
