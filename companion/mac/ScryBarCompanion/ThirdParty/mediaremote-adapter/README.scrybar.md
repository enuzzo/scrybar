# Vendored MediaRemote adapter

Source: [ungive/mediaremote-adapter](https://github.com/ungive/mediaremote-adapter),
commit [`0e82bf1c44ec39554fbfc402ec88a48c6ae2a0fd`](https://github.com/ungive/mediaremote-adapter/tree/0e82bf1c44ec39554fbfc402ec88a48c6ae2a0fd).
License: BSD 3-Clause; the complete upstream license is retained in `LICENSE`.
`provenance.json` records the pin and archive checksum; `SOURCE_SHA256SUMS`
checks the selected source files, including the documented ScryBar patch, before
each build. `UPSTREAM_SHA256SUMS` preserves their original upstream hashes.

The vendored inputs include the CMake project, all Objective-C sources and
headers, the Perl loader, upstream README and license. The pinned `.gitmodules`
is empty: this version needs no additional submodule or downloaded binary.
ScryBar integration lives outside the upstream sources in
`script/embed_mediaremote_adapter.sh`.

One local change in `src/adapter/now_playing.m` exports the existing optional
`kMRMediaRemoteNowPlayingInfoArtworkIdentifier` metadata under the JSON key
`artworkIdentifier`. This lets the provider retain a track's actual artwork
identity without a title-based guess. The patch adds no MediaRemote call or
playback control. `scrybar-artwork-identifier.patch` records the exact change.
`scrybar-retention-identity.patch` adds the optional unique, content-item and
artwork identifiers to the upstream item-identity comparison before it retains
missing artwork. A change or disappearance of a known identifier therefore
cannot inherit the previous item's pixels merely because its title and album
match. Both optional identifiers being absent remains compatible with upstream.

`python3 scrybar_tests/run_tests.py` compiles the real metadata converter and
identity-key source plus the extracted upstream retention predicate/branch
against synthetic dictionaries. All compilation happens in a temporary
directory outside the source tree; it does not query live music or start the
system adapter.

The helper compiles only the `MediaRemoteAdapter` target, with arm64 and x86_64
slices and a macOS 14.0 minimum. Build products remain under external
`SCRYBAR_DERIVED_DATA`; the helper rejects Dropbox and in-repository paths.
It does not build or bundle the upstream TestClient. The app does not link
the framework: its read-only music provider invokes `/usr/bin/perl` with the
loader and framework paths.

The app bundle receives these resources:

```text
Contents/Resources/
  THIRD_PARTY_NOTICES
  MediaRemoteAdapter/
    mediaremote-adapter.pl
    MediaRemoteAdapter.framework/
    LICENSE
```

The nested framework is ad-hoc signed before the outer app. Both build scripts
verify the completed app with `codesign --verify --deep --strict`. Ad-hoc
signatures are not Developer ID signing or notarization. The framework uses
Apple's private MediaRemote API through a system Perl process; compatibility
can change with macOS releases. Packaging checks do not establish music-feed
correctness or physical-device acceptance.

To verify just the framework without launching or rebuilding the app:

```sh
script/embed_mediaremote_adapter.sh --framework-only /tmp/scrybar-bridge-check.noindex
```

Updates must deliberately change the source pin, provenance and source checksum
manifest together, retain the full BSD notice, and repeat architecture,
deployment-target, signing and provider tests.
