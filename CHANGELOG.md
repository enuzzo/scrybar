# Changelog

## 2026-10-23

### v0.9.5 (firmware r322) — active frame pacing, live Stats and WebUI redesign

- Isolate HTTP parsing and response transmission from the visual loop, retain
  main-thread ownership of callbacks and pace cached slides against absolute
  deadlines. Final r322 hardware profiling measured approximately 60 fps during
  active motion with zero missed periods in the stated 30-second trial under
  live traffic and delayed HTTP clients. Preparation, stationary time and handoff
  have separate costs; this is not a hard 60 fps or optical scanout guarantee.
  See the [measurements and limits](docs/performance/2026-10-23-frame-pacing.md).
- Plot raw 60-second Mac Stats samples at real receipt timestamps with a shared
  network scale, destination scope, leading-app rates and explicit unknown gaps.
  Preserve isolated observations and measured zero while the window scrolls.
- Review native typography, alignment, copy and seven-theme contrast across the
  display views. Preserve the existing artwork, fonts and native resolution;
  anonymous render fixtures are labelled separately from hardware evidence.
- Rebuild the public Astro page and mobile Control Surface around real previews
  and Display / Wi-Fi / Content tasks. Keep pending choices during Reload,
  distinguish live brightness from persisted levels, and retain unsaved state
  after reopening. Validate Chromium/WebKit, ROOT/Pages paths and the actual
  installer manifest. See the [web QA record](docs/design/2026-10-23-webui/QA.md).
- Update installer metadata for firmware v0.9.5 and link Companion 0.3.15 (53)
  as a Preview signed ad hoc, not notarized. Earlier release entries remain below.


### Companion v0.3.15 (53) — read native artwork and preserve track identity

- Read the allowed player's real artwork through a bundled native MediaRemote
  stream, instead of depending on a metadata-only JXA read and a catalog search
  for every track. Keep source, track and album identity together; publish
  permitted metadata while optional cover work completes asynchronously.
- Prevent the startup path from launching a competing JXA read while the native
  reader is starting. Reject stale or mismatched artwork and retain the last
  coherent current-track image through temporary missing artwork.
- Refresh covers after a display restart without requiring a track change, using
  a monotonic 15-second bound per successful endpoint delivery. Update the
  Companion preview when cover pixels arrive after metadata; incomplete pixels
  do not consume the first valid delivery.
- Preserve ScryBar's 150 px artwork. Meguru's optional 512 px variant requires a
  genuine source of at least 512 px and a cover/edition match; do not enlarge
  small artwork into HD. The current source returned 150 × 150 px even for
  explicit 512/768 px requests; that is an observation, not a universal API cap.
- Pass 92 Swift regression tests and 22 native Objective-C identity cases.
  A bounded producer check observed a first native frame in 110.9 ms, then 20/20
  valid in-sync snapshots with 19 covers while the first transcode completed
  asynchronously. No HD source was available in that check. Automated and
  producer evidence does not itself establish installed receiver acceptance.
- Package the universal app as Preview 0.3.15 (53), signed ad hoc and not notarized.
  Bundle the pinned BSD-3-Clause reader and its license notices; see the
  [Companion release record](docs/releases/companion-0.3.15.md) for package,
  publication and installed-test status.

### Firmware r316 — preserve network trace chronology

- Plot real receipt timestamps and raw rates with native LVGL XY lines. Advancing
  the clock now translates existing points without rebinning peaks or changing
  their values and automatic scale. Update scroll positions every 200 ms.
- Let the page orchestrator honor that cadence for valid Stats while retaining
  the other pages' clock cadence and the waiting screen's idle behavior.
- Connect known samples at their actual spacing when delivery skips an
  intermediate update; explicit unknown values and freshness-length outages
  still break continuity. Preserve isolated known markers around unknowns.
- Add on-demand `NETGRAPH` diagnostics containing relative sample times, rates
  and validity flags without host, application or network identifiers.
- Count actual accepted bytes during `SNAP`; retry partial/zero writes and
  report a bounded stall without falsely claiming a complete transfer.
- Verify temporal stability, isolated peaks, native rendering and serial
  screenshot sequences. See the [follow-up record](docs/performance/2026-10-23-network-scroll.md).

### Firmware r313 — cached page motion and Mac Stats history

- Skip unchanged LVGL text and local style assignments while preserving explicit
  text refreshes, inheritance and state selectors. Refit the HOME sentence only
  when its text changes. Register a 16 ms display refresh period explicitly.
- Compose adjacent page slides from two native-order PSRAM snapshots through
  the existing DMA buffers. Support finger tracking, completion and cancellation;
  return overlays, unsupported pages and interrupted gestures to normal LVGL.
- Work around the installed ESP-IDF 5.5.2 SPI LCD callback's missing ISR yield.
- Implement the owner's selected Stats concept 2: prominent network rates,
  shared-scale 60-second history, destination scope/partial-coverage context and
  leading-app directional rates. Keep missing samples distinct from measured zero.
- Preserve continuous traces across normal sampling jitter, leave true gaps and
  explicit unknown values visible, and reserve headroom for peak strokes.
- Add native pixel/state tests, actual-LVGL visual checks and bounded serial
  `PERFBENCH` telemetry. See the [performance record](docs/performance/2026-10-23-display-and-stats.md)
  for device measurements, installation and remaining physical acceptance.

### Companion artifact cleanup

- Keep Companion 0.3.14 (50) in Applications and its DMG in root `dist/`.
  Archive 44 duplicate/old artifacts in a verified local ZIP before removal.
  Future builds use external derived data and avoid a second loose app copy.
  See the [cleanup record](docs/maintenance/2026-10-23-companion-artifact-cleanup.md).

## 2026-10-22

### Companion v0.3.14 (50) — Center menu-bar network rates

- Render download/upload as one native template image with the visible glyph
  block vertically centered, replacing the multiline status-button title.
- Retain the same font, directional rows, item width, click actions and accessible
  live value. The system tints the template for appearance and selection.
- Check visible top/bottom bounds offscreen at 1× and 2× before installation.

### Companion v0.3.13 (49) — Keep Now Playing responsive

- Publish permitted music metadata immediately; resolve covers on one background
  worker so TIDAL cache scans, image downloads and transcoding cannot stall the
  app or either display's song/elapsed updates.
- Bound TIDAL fallback scans and reuse short-lived catalogs/metadata lookups.
  Match asynchronous covers to the complete track and album edition.
- Deliver artwork that becomes available after the first metadata-only payload
  to both ScryBar and Meguru. Retain privacy gates and 150/512-pixel variants.
- Add stalled-cover, late-cover delivery and optional actual-music smoke checks.
  See the [release record](docs/releases/companion-0.3.13.md).

### Companion v0.3.12 (48) — Recover stale network collection

- Recover when nettop emits bytes but stops producing usable frames, or reaches
  the bounded socket/buffer capacity. Do not leave old rates stale indefinitely.
- Isolate unreadable socket rows with explicit partial coverage while preserving
  valid app traffic; fully unusable frames cannot masquerade as measured idle.
- Keep known idle sockets distinct from active traffic with an unknown peer;
  fix the child protocol locale and normalize host-name apostrophes for the
  embedded display fonts.
- Add sustained native collection and overload/recovery regression checks.

### Companion v0.3.11 (47) — Destination scopes

- Add persistent Internet / Rete interna / Tutto selection, default Internet.
  Filter totals, app rankings and display payloads from the same socket deltas.
- Classify numeric peers, including private/shared addresses, Tailscale and
  physical on-link IPv6 prefixes; avoid counting Tailscale transport twice.
- Keep wildcard peers and missing byte counters explicit as partial coverage.
  No destination logging, DNS lookups or endpoint delivery. Clear old-scope
  values immediately and discard old-scope computations after a selection change.
- Preserve legacy display compatibility with optional `networkScope` and
  `networkCoveragePartial` fields. See the [contract](docs/mac-stats-live-v1.md).

### Companion v0.3.10 (46) — Live network preview

- One universal Mac app for ScryBar and Meguru, retaining the music-only source
  policy, optional studio TV push, Bambu camera cleanup and negotiated HD artwork.
- Add a compact download/upload menu item alongside the existing music item.
  Clicking either opens the same popover with up to five ranked apps and separate
  directional rates. The network indicator can be hidden in Settings.
- Sample hardware/network at approximately 1 Hz. Total rates use public 64-bit
  physical-interface counters; process summaries come from one persistent nettop
  process. App helpers aggregate by containing bundle. No destinations or domains
  are collected. Only the leading app goes to displays; rankings stay on the Mac.
- Distinguish measured idle from unknown/stale. Sleep, counter resets and long
  gaps require a new baseline. A PTY keeps nettop output prompt and its input open,
  fixing a native EOF busy loop found during profiling.
- Add the backward-compatible optional network payload fields and ScryBar r309's
  compact CPU/GPU, RAM/disk and network/app layout. Meguru's native circular view
  is implemented and verified in its own repository.
- Build the universal ad-hoc signed DMG outside Dropbox's source tree. See the
  [release record](docs/releases/companion-0.3.10.md) for exact checks and remaining
  installation, publication and physical-display acceptance.
- Correct the test-launch popover persistence exposed by build 45: every launch
  retains transient dismissal, and both main/settings headers have an explicit
  Close action with Escape. Build 46 was prepared without relaunching the app;
  visible testing must be agreed with the owner before interrupting their Mac.


## 2026-10-19

### Unreleased — Companion v0.3.10 planned

- `092b279d`: retain `NowPlayingPayload.webArtworkURL`, the public URL of the
  cover actually resolved by the System provider (matching TIDAL edition or
  iTunes fallback). Studio TV push prefers it over the original `artworkURL`.
- Keep this field internal to the Companion/web destination. The
  `NowPlayingWirePayload` schema and ScryBar/Meguru transport are unchanged.
- This commit is **not in the 0.3.9 DMG**. Next package is planned as 0.3.10;
  source version metadata still reads 0.3.9 (44) pending the release bump.

### Companion v0.3.9 (44) — Local package

- `e68add4`: add `SourcePolicy.swift` and PRIVACY settings. **Music apps only is
  on by default in every build** (when unset): only Apple Music (`com.apple.Music`),
  Spotify (`com.spotify.client`) and TIDAL (`com.tidal.desktop`) pass. Studio
  privacy is mandatory: video players, browsers, YouTube/YouTube Music and unknown
  apps must never expose content on displays, in logs or through cover searches.
- Gate the System provider after bundle-ID resolution, before artwork/iTunes
  lookup, caching or logging; gate all providers again in `AppModel.refreshPayload`.
  Blocked content becomes an empty placeholder with `Hidden for privacy`; remember
  only the hidden category, never its title.
- Add disabled-by-default browser/YouTube and video-player extras, usable only
  with music-only off. Turning it off also allows unknown apps; these opt-outs
  are incompatible with the required studio configuration.
- Add optional STUDIO TV HTTPS push (`YakamozPush.swift`), off by default.
  `yakamoz.url` / `yakamoz.enabled` persist configuration; a separate Keychain
  service holds the push key. With Auto-send on, send permitted metadata on change
  and every 10 seconds; send `{"bundleId":"none"}` when nothing permitted is
  available. The TV receiver has an independent music-source allowlist.
- `da6bf78`: package `companion/mac/ScryBarCompanion-0.3.9.dmg`. The owner reports
  installation in `/Applications`. This local package is distinct from the older
  published 0.3.5 (40) baseline; no new public release or firmware update is implied.
- Implementation-session validation reported on 2026-10-19: **44/44 Swift tests
  passed**, including six `SourcePolicyTests`. This documentation-only pass does
  not rerun tests or verify live app/TV/display state.
- Document the binding owner privacy decision and recommend an additional
  ScryBar firmware source allowlist before logging/artwork/rendering. That second
  firmware defence is **not implemented** in this update.

### Local source checkpoint — Firmware r308 / Companion v0.3.8 (43)

This saves existing work; it is not a new public release, package, tag or device
installation. Published baseline remains ScryBar v0.9.4 / Companion v0.3.5 (40).

- Add persistent multi-display selection and simultaneous Bonjour discovery for
  ScryBar and Meguru. Negotiate protocol v1 capabilities and deliver feeds through
  independent destination/feed tasks with bounded timeouts and stale-data handling.
- Add separate Meguru RGB565 artwork delivery: 150 px legacy covers or negotiated
  512 px covers, with resolution-aware caching and preferred 640 px TIDAL sources.
  Preserve ScryBar's 150 px JSON artwork; identify Mock metadata as demo content.
- Replace the empty launch Settings scene while keeping the menu-bar popover.
  Project metadata records Companion 0.3.8 (43); retain the earlier Apple Silicon
  0.3.7 (42) candidate DMG without presenting it as the 0.3.8 source build.
- Read saved Bambu credentials off the main thread. Share one camera stream with
  independent display leases and frame acknowledgements; persist consumed light
  revisions, reject expired commands and bound ON retries while retaining OFF
  cleanup and printer-state confirmation.
- Preserve r308 / 2026-09-10 firmware changes: screensaver at 5 minutes and backlight
  off at 15 minutes on USB/battery, including QR overlays. Qualify touch for 40 ms,
  bound stuck contact to 30 seconds, remove raw-touch/IMU wake and add inactivity
  diagnostics. Brightness updates respect idle-off.
- Save the PNG and Icon Composer proposals without replacing the active app icon.
- Record removal of six obsolete tracked DMGs (0.2.4 and 0.3.1 through 0.3.5) from
  the current tree; previous versions remain in Git history. Leave untracked
  `output/` untouched and outside all commits.
- Refresh the Companion README, shared project knowledge, decision log and
  multi-display protocol with source/package boundaries and dated evidence.

Validation on 2026-10-19 (Apple Silicon, Swift 6.4):

- Default `swift test`: blocked before test execution. The default `swiftbuild`
  engine's ad-hoc test-bundle signing failed with `resource fork, Finder information,
  or similar detritus not allowed` on generated Dropbox-local output.
- `swift test --build-system native`: compilation/linking succeeded; 38 tests
  started, 33 passed, one failed and four had no completion result before abort.
  `secureAccessCodeFieldHandlesCommandV` failed at
  `BambuPrinterPayloadTests.swift:360` because `NSPasteboard.setString` returned
  false. The sandbox logged denied service access; the runner then aborted with
  signal 6 while reporting an invalid/null NSPasteboard value. Sandboxed run: FAIL.
- Re-run the same native test binary with `--skip-build` outside the sandbox so
  the named test pasteboard can use macOS services: **all 38 tests passed**, exit 0.
  No app launch, developer signing or code/test changes were needed. The native
  build engine reports its deprecation; the two earlier failures above remain
  recorded as environment/toolchain limitations.
- Host C++ tests `display_idle_test`, `touch_timing_test` and
  `bambu_camera_controls_test`: all compiled with C++17 and passed (exit 0).
- Retained 0.3.7 DMG checksum: valid (`hdiutil verify`). No firmware build/flash,
  app launch/install or physical camera/light/display checks were performed.

## 2026-09-09

### ScryBar v0.9.4 — Firmware r307 / Companion v0.3.5 (40)

- Fix page drags snapping back instead of selecting the next view: release debounce now uses elapsed time rather than loop count, and recognized drags no longer expire after three seconds.
- Measure tap duration using actual finger contact, excluding release processing delays.
- Show the estimated print finish day and local time in Companion, alongside remaining time. Hide the estimate when disconnected, paused, blocked, finished or without a positive remaining duration.
- Add touch timing regression coverage, including delayed processing and timer wraparound, plus estimated-finish tests.
- Verify r307 on the physical ScryBar; page swiping was confirmed working after installation.

## 2026-09-08

### ScryBar v0.9.3 — Firmware r306 / Companion v0.3.4 (39)

- Add on-demand Bambu A1/P1 camera viewing on ScryBar and in the macOS Companion, sharing a single LAN stream without Bambu Studio.
- Add light controls with persisted light-off cleanup when the last camera viewer closes, retry after reconnection, and graceful Companion quit handling.
- Improve the Bambu header with aligned outlined state badges and a prominent CAM button; display FINISHED instead of FINISH.
- Fix CAM touch handling so normal finger drift does not turn a camera tap into page navigation.
- Keep only light and Back controls on the device camera view; separate Light: ON/OFF state from Turn on/off actions and remove unsupported Connecting punctuation.
- Correct the boot logo RGB565 byte order, restoring clean grayscale edges without changing the source asset.
- Add camera protocol, viewer lifecycle, recovery, image conversion and touch regression checks.

## 2026-09-06

### ScryBar v0.9.2 — Firmware r302 / Companion v0.3.3 (38)

- Restart rate-limited printer discovery after network failures and use the saved LAN name as a fallback identity after the serial.
- Forward print filament labels and colors, rotating them on the physical Bambu display.
- Show observed settled filament changes in the Companion and firmware; show the total when the printer supplies a change sequence. Counts cover transitions observed by the running Companion.
- Add regression coverage for LAN-name selection, recovery throttling, authentication failures and filament transitions.

## 2026-09-02

### ScryBar v0.9.1 — Firmware DB-M0-r301 / Companion v0.3.2 (37)

- Fixed Bambu reconnection after DHCP address changes: the Companion now matches the saved printer serial before a selected or persisted IP and automatically restarts MQTT on the current address.
- Rejected bare SSDP echoes that contain a requested Bambu service type but no printer serial or model, preventing false devices from suppressing the verified LAN-service scan.
- Preserved manual-IP fallback for routed/VLAN setups and kept LAN access codes scoped to each printer identity in macOS Keychain.
- Added regression coverage for stale-IP recovery, serial-first selection, manual fallback and false SSDP responses.

### ScryBar v0.9.0 — Firmware DB-M0-r300 / Companion v0.3.1 (36)

- Added the Bambu Lab print-monitor view with live job state, progress, remaining time, layers, nozzle/bed/chamber temperatures, speed, fans, Wi-Fi, filament, AMS and alert telemetry.
- Added direct Bambu LAN MQTT/TLS support to the macOS Companion. The LAN access code stays in Keychain; ScryBar receives only a sanitized HTTP status payload.
- Added printer auto-discovery, duplicate merging, multi-printer selection, retry, manual fallback, connection feedback and concise in-app setup help.
- Added distinct macOS notification sounds for print start, blocked/error states and completion.
- Added persistent drag-to-reorder controls for the physical swipe carousel in the ScryBar web UI.
- Replaced Funnel Display with Dosis Regular/SemiBold across the physical UI and retuned view typography, weights and alignment for the 640×172 viewport.
- Redesigned the physical Bambu view around a large progress ring, compact telemetry pills and a rotating green/white detail footer.
- Added 19 Meteocons mappings driven by live Open-Meteo WMO codes. The main icon animates at a calmer 5 fps; the three-hour forecast stays static and high-contrast.
- Changed the Home weather card to a dark, theme-tinted surface with automatic contrast across all seven themes.
- Restored Now Playing cover art for players without a stable artwork identifier and added album-title forwarding to the physical view.
- Fixed Companion secure-field Command-V support and added truthful saving, connecting, connected and failure feedback.
- Fixed Companion popover sizing: it now uses the display's real usable height, opens tall enough for its content when possible and scrolls only on genuinely short screens.
- Added the real-print Bambu screenshot to the README and public website gallery.

## 2026-03-13

### Firmware DB-M0-r185
- Power button UX pass:
  - `PWR` long-press now triggers while the button is still held, instead of waiting for release
  - soft-off and wake thresholds reduced to `3s`
  - wake from soft-off now resumes directly to `HOME`
  - added held-key release gate after wake/boot to avoid immediate retrigger loops
- Power/button docs refresh:
  - updated README and knowledge notes to match `3s` hold behavior and `HOME` resume policy
- DOOM startup policy:
  - disabled automatic DOOM page autostart at boot; default landing page remains clock + meteo
- DOOM IMU tuning:
  - reduced near-neutral twitchiness with wider engage thresholds, softer filtering, and hysteresis around zero

## 2026-03-06

### Firmware DB-M0-r152
- Power button hardening:
  - added press-side debounce (`45ms`) and minimum short-press gate (`90ms`)
  - ignores micro-pulses/bounce so screensaver is no longer triggered by false short presses
- Screensaver policy update:
  - idle timeout unified to `2h` on both USB and battery (`SCREENSAVER_IDLE_USB_MS`/`SCREENSAVER_IDLE_BATTERY_MS`)
- RSS/WIKI readability and layout pass:
  - RSS timestamp moved back to the left source line (`SOURCE - DD Mon HH:MM`)
  - content block shifted slightly upward and clipped before action buttons
  - WIKI title is shown in header meta (`Wikipedia | <family> | <article>`) while body focuses on summary
- WIKI content sanitization:
  - improved HTML cleanup pipeline (entity decode + tag strip + whitespace collapse) to avoid raw HTML fragments on screen
- WIKI image pipeline reliability:
  - added Wikimedia JPG->PNG thumb preference path where possible
  - added JPEG JFIF normalization for LVGL decoder compatibility
  - increased thumb budget to `65536` bytes for richer featured images
- QR robustness:
  - QR modal now immediately falls back to original article URL while short-link generation is pending, avoiding black/empty QR states
  - QR widgets now get explicit background/border styling for better visibility
- AUX/WIKI buttons contrast pass:
  - improved runtime contrast resolution for `NXT`, `SKIP`, and `QR` buttons across themes
- Web UI responsiveness improvements:
  - external font/icon stylesheets are loaded asynchronously
  - long RSS/WIKI HTTP operations now periodically pump web server loop to reduce perceived page stalls
  - RSS HTTP timeout lowered to `3000ms` for faster failure recovery
- WIKI live-preload while pinned on page:
  - visible WIKI item now gets progressive meta/thumb/favicon preload even without leaving the view
- Screensaver 1337 polish:
  - thought text is now forced uppercase when language is `l33t`

## 2026-03-05

### Firmware DB-M0-r151
- Restored AUX/RSS behavior without replacing user RSS feeds.
- Added dedicated `WIKI` view (`VIEW3` / `VIEWWIKI`) separate from AUX/RSS.
- Wiki view now follows AUX interaction model:
  - `SKIP` -> next article
  - `NXT` -> next feed family
  - `QR` only on demand
- Added Wiki content enrichment and media UX:
  - article summary + title rendering
  - right-side remote thumbnail when available
  - feed-family badge/label for clearer context (`Featured`, `On this day`, `Wikinews`)
- Navigation updates:
  - `VIEWFIRST` / `VIEWLAST`
  - physical `BOOT` short press mapped to jump `HOME`
  - `PWR` short press keeps screensaver behavior

## 2026-02-13

### Firmware DB-M0-r132
- GPT `ASK` flow now forces response variability with per-request random seed in prompt.
- Updated GPT prompt profile for less repetitive/truncated answers:
  - chat `temperature` increased to `0.8`,
  - default ask now requests random number + random quote in Italian.
- Increased audio recording window from `4s` to `20s` for mic-to-GPT tests.
- Reduced visible truncation in GPT panel:
  - enlarged answer buffer (`g_gptPanelText`) and question buffer (`g_gptLastQuestion`),
  - kept higher TTS char cap for longer spoken replies.

### Firmware DB-M0-r131
- Added `GPT` as a new LVGL page (`VIEW3` / `VIEWGPT`) with dedicated header/body placeholder panel.
- Extended swipe navigation and page drag/slide logic from 3 pages to 4 pages (`INFO -> HOME -> AUX -> GPT`).
- Updated serial help output to include the new GPT view commands.

### Firmware DB-M0-r130
- Screensaver idle timeout is now power-aware:
  - `60 min` on USB-C,
  - `30 min` on battery.
- `SAVERSTAT` now reports the active idle timeout in ms.

### Firmware DB-M0-r129
- Screensaver anti-retention update:
  - field/grass line now scrolls slowly,
  - tiny tree marker drifts with the field,
  - footer date/time now applies periodic micro-jitter offsets.

### Firmware DB-M0-r128
- Fixed right-facing moving cow eye spacing to `-e e -`.
- Updated thought visibility timing to `15s ON / 10s OFF` (25s cycle).

### Firmware DB-M0-r127
- Tightened screensaver thought wrapping boundary:
  - max width reduced to ~56% screen,
  - explicit wrap set to 28 chars / line (max 3 lines).

### Firmware DB-M0-r126
- Added explicit quote word-wrap (max 34 chars per line, max 3 lines) to prevent right-edge overflow in screensaver thoughts.

### Firmware DB-M0-r125
- Thought text max width reduced to ~65% of screen to force more frequent 2-3 line wrapping.

### Firmware DB-M0-r124
- Thought text width reduced to ~72% of screen to favor 2-3 line wrapping with small monospace font.

### Firmware DB-M0-r123
- Screensaver thought width cap changed from fixed `90px` to responsive `90%` of screen width for readability.
- Kept text-only thought style plus dashed underline (no balloon box).

### Firmware DB-M0-r122
- Screensaver thought style simplified:
  - removed balloon box, now text-only plus dashed underline,
  - forced max thought width to 90px.

### Firmware DB-M0-r121
- Balloon wrap tuning: reduced maximum bubble width to improve line balance and avoid right-edge glyph artifacts.

### Firmware DB-M0-r120
- Screensaver balloon text flow improved: removed forced line breaks from quotes for cleaner natural wrapping.
- Replaced quote set with the new 40 philosophical/cynical lines.
- Reduced balloon text spacing to near-zero extra leading for denser, cleaner rendering.

### Firmware DB-M0-r119
- Screensaver balloon now auto-sizes to quote length (no oversized fixed box).
- Cow thought text refreshed with 40 more sarcastic lines.
- Right-facing cow eye alignment tuned (`-e e`) for consistent gaze.
- Balloon readability slightly increased with modest mono spacing bump (+~20% perceived size).
- Stars shifted to a warmer/yellow tint for softer night mood.

### Firmware DB-M0-r118
- Screensaver cow updated using user-provided ASCII files for both directions.
- Cow raised slightly and spacing reset to avoid stretched look.
- Balloon typography tuned to mid-size mono style with:
  - thicker border (`2px`),
  - increased inner padding,
  - moderate line spacing.

### Firmware DB-M0-r117
- Screensaver cow tuning:
  - reduced perceived size by lifting/compacting placement and tighter monospace spacing,
  - adjusted right-facing eye row alignment.
- Balloon text switched to larger monospace font with increased line spacing.

### Firmware DB-M0-r116
- Screensaver cow size reduced and repositioned for better proportion on panel.
- Balloon text font switched to monospaced glyphs.
- Cow mirrored sprite refined to stabilize eye alignment in both directions.

### Firmware DB-M0-r115
- Screensaver stars slowed down further for softer twinkle (independent timing remains async).
- Balloon upgraded to comic style:
  - white dashed border,
  - dark translucent fill,
  - tail marker pointing downward toward the cow.
- Balloon visibility cycle changed to:
  - hidden for 15s,
  - visible for 10s,
  - repeat.
- Expanded philosophical quote pool to ~40 Italian lines with longer wording.

### Firmware DB-M0-r113
- Fixed screensaver footer placement: date/time is now anchored reliably to bottom-right after text updates.

### Firmware DB-M0-r112
- Screensaver typography/layout refinement:
  - switched cow rendering to true mono bitmap font path (`unscii_16`) to reduce stretched look,
  - cow color set to white,
  - grass simplified to a single row.
- Removed bottom hint text and added compact date/time footer at bottom-right.
- Star glyph rendering remains sparse and asynchronous with mono-safe glyphs.

### Firmware DB-M0-r111
- Screensaver star glyph compatibility fix:
  - switched twinkle glyphs from `*` to a safe set (`.`, `:`, `o`) to avoid missing-glyph rendering on the current mono font.

### Firmware DB-M0-r110
- Screensaver star bootstrap tweak:
  - ensures at least one lit star per sky row at saver start for better vertical distribution,
  - keeps independent timing and gentle fade behavior.

### Firmware DB-M0-r109
- Screensaver star tuning:
  - keeps stars asynchronous and sparse (max 2 per row),
  - initializes a small random subset already lit to avoid empty-sky first frames,
  - preserves slow independent fade cadence.

### Firmware DB-M0-r108
- Screensaver stars redesigned for a softer look:
  - independent per-star fade timing (no global blinking),
  - max 2 stars per row, sparse and random placement,
  - star field extended vertically to stop about 3 text rows above the field.
- Fixed footer hint text readability (`touch to start`).
- Added cow speech balloon with rotating short philosophical quotes in Italian (refresh every ~10s).

### Firmware DB-M0-r107
- Screensaver cow sprite updated to the new ASCII art supplied by user.
- Added automatic sprite mirroring when movement direction changes, so the cow faces the walk direction.

### Firmware DB-M0-r106
- Screensaver behavior refinement:
  - fixed wake logic to avoid touch just “refreshing” saver,
  - added wake guard to prevent immediate re-entry right after wake.
- New minimal ASCII scene:
  - wider star sky (multi-row), random sparse twinkle with pseudo-fade levels,
  - full-width field with grass + small tree detail,
  - cow movement changed from constant float to horizontal short-step bursts with random pause (1..6s).

### Firmware DB-M0-r105
- Stabilized screensaver wake detection:
  - added stricter raw-touch filters (reject phantom/out-of-panel points),
  - added short raw-touch debounce before wake.
- Keeps touch wake responsive while avoiding immediate accidental saver exit.

### Firmware DB-M0-r104
- Screensaver refinement for testing and style:
  - idle timeout reduced to 10s (`SCREENSAVER_IDLE_MS=10000`) for quick validation.
  - minimal ASCII scene added (star sky + field + moving cow) using monospaced font.
  - touch wake path hardened with raw touch-presence fallback while saver is active.

### Firmware DB-M0-r103
- Simplified energy saver to a binary policy only:
  - `USB-C`: backlight 100%
  - `BATTERY`: backlight reduced (`ENERGY_BACKLIGHT_ON_BATTERY`, default 72%)
- Removed energy-driven weather/RSS refresh scaling (refresh behavior unchanged).
- Added anti-retention screensaver:
  - auto-start after inactivity (`SCREENSAVER_IDLE_MS`, default 90s),
  - moving pixel-style cow across variable heights,
  - wake on touch (and shake when IMU loop is enabled).
- Added serial helper commands: `SAVERON`, `SAVEROFF`, `SAVERSTAT`.

### Firmware DB-M0-r102
- INFO stats tuning:
  - removed duplicate `ip` line from `net` card (IP remains in header).
  - restored visual breathing space under `[net]` and `[system]`.
- Energy saving first integration (battery-aware):
  - automatic backlight policy by power profile (`AC`, `BATT`, `ECO`, `ULTRA`).
  - weather/RSS refresh and retry intervals now scale up on battery to reduce consumption.
  - runtime profile switch logs on serial (`[ENERGY] ...`).

### Firmware DB-M0-r101
- Fixed LVGL recolor parsing on battery visual line in INFO stats.
- Battery bars now use recolor-safe glyphs (`=`/`.`) to avoid showing raw hex color codes.

### Firmware DB-M0-r100
- Added at-a-glance battery visual in INFO stats: colored level bars (`[-----]` to `[#####]`) plus charging marker (`+CHG`).
- Battery line now communicates level immediately without reading percentages first.

### Firmware DB-M0-r099
- INFO panel readability pass: switched stats body to a larger, cleaner sans font for easier at-a-glance reading.
- Added real-time `src` line under power status to show current power source (`USB-C` or `BATTERY`).
- Improved plug/unplug responsiveness for power source detection with a faster slope hint while keeping stable charging trend logic.
- Tightened stats text layout (removed extra blank rows) to keep all fields visible.

### Firmware DB-M0-r098
- INFO panel now shows Wi-Fi SSID and power status (`CHARGING`/`BATTERY` + percentage).
- Wi-Fi reconnection is now non-blocking and cyclic: each configured SSID is tried for 10s, then the firmware moves to the next one continuously until handshake.
- Boot flow no longer depends on immediate Wi-Fi success: LVGL UI starts even offline (no more indefinite logo wait when network is unavailable).
- Added deferred NTP sync in loop once Wi-Fi becomes available.

## 2026-02-12

### Firmware DB-M0-r097
- Set canonical physical orientation to USB-C/power on the left (speaker top, mic bottom).
- Added `DISPLAY_FLIP_180` runtime config flag and enabled it by default.
- Applied 180deg panel flip on ESP LCD backend and aligned touch mapping accordingly.

### Firmware DB-M0-r096
- Tuned Tron Grid for mobile: brighter lines/glow, higher grid plane, and stronger horizon visibility.
- Simplified visual hierarchy by removing outer wrapper chrome (`hero`/`panel`) while keeping inner cards.
- Increased UI translucency so animated background remains visible without hurting readability.

### Firmware DB-M0-r095
- Integrated animated Tron Grid background into Web UI (`fx-grid` layer with plane/glow/horizon/scanline/vignette).
- Added reduced-motion support for accessibility (`prefers-reduced-motion`).
- Applied glass-style transparency treatment to key Web UI sections.

### Firmware DB-M0-r094
- Fixed regression where `ScryBar Stats` labels could disappear (black cards with no text).
- Stabilized INFO stats typography using visible monospace fallback (`LV_FONT_UNSCII_8`) with recolor tags enabled.
- Kept colored section labels (`[net]` cyan, `[system]` magenta) and compact `key: value` formatting.
- Confirmed clean compile/upload workflow with explicit build path and no stale cache artifacts.

### Firmware DB-M0-r093
- Tried intermediate visual scaling for monospace text (later reverted in r094 due rendering side effects).

### Firmware DB-M0-r092
- Increased stats monospace readability and added extra spacing under section headers.

### Firmware DB-M0-r091
- Introduced monospace style for INFO stats blocks.
- Colored section tokens for technical readability (`net/system`).
- Tightened separator spacing around `:` in stats lines.

### Firmware DB-M0-r090
- Unified INFO header typography with other views (title + endpoint).
- Darkened INFO header background and set content background to full black.

### Firmware DB-M0-r089
- RSS AUX controls stabilized:
  - `NXT` jumps to next feed block.
  - `SKIP` advances to next item.
- Added stronger touch hit areas and visual press feedback parity across `NXT/SKIP/QR`.
- Added fixed-top status message for web UI and improved `/reload` behavior using current form state.
- Added WebUI QR in INFO system card and compacted DNS display to single line.
