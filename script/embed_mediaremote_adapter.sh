#!/usr/bin/env bash
set -euo pipefail

# Build from vendored source; never download or copy a prebuilt bridge.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENDOR_DIR="$ROOT_DIR/companion/mac/ScryBarCompanion/ThirdParty/mediaremote-adapter"

usage() {
  echo "usage: $0 APP_BUNDLE DERIVED_DATA | --framework-only DERIVED_DATA | --check-derived-data DERIVED_DATA" >&2
  exit 2
}

[[ $# == 2 ]] || usage
MODE="$1"
DERIVED_DATA="$2"

# Resolve existing parent symlinks too, before CMake/Xcode creates any files.
# This also protects worktrees outside Dropbox from an in-repository override.
/usr/bin/python3 - "$ROOT_DIR" "$DERIVED_DATA" "$MODE" <<'PY'
from pathlib import Path
import os, sys
root, derived = (Path(os.path.realpath(p)) for p in sys.argv[1:3])
def check(path):
    parts = path.parts
    in_dropbox = any(part == "Dropbox" or part.startswith("Dropbox (") for part in parts)
    if in_dropbox or path == root or root in path.parents:
        raise SystemExit("Build products must stay outside Dropbox and the source tree; set SCRYBAR_DERIVED_DATA to an external directory.")
check(derived)
check(Path(os.path.realpath(derived / "MediaRemoteAdapter")))
if not sys.argv[3].startswith("--"):
    app = Path(os.path.realpath(sys.argv[3]))
    check(app)
    check(Path(os.path.realpath(app / "Contents" / "Resources")))
PY

if [[ "$MODE" == "--check-derived-data" ]]; then exit 0; fi
if [[ "$MODE" != "--framework-only" && "$MODE" == --* ]]; then usage; fi

for tool in cmake shasum xcrun codesign; do
  command -v "$tool" >/dev/null || { echo "Required build tool is missing: $tool" >&2; exit 1; }
done

(cd "$VENDOR_DIR" && shasum -a 256 -c SOURCE_SHA256SUMS)
/usr/bin/perl -c "$VENDOR_DIR/bin/mediaremote-adapter.pl"

BUILD_DIR="$DERIVED_DATA/MediaRemoteAdapter"
cmake -S "$VENDOR_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
# The upstream TestClient is deliberately neither built nor bundled.
cmake --build "$BUILD_DIR" --config Release --target MediaRemoteAdapter \
  --parallel "${SCRYBAR_BUILD_JOBS:-2}"

FRAMEWORK="$BUILD_DIR/MediaRemoteAdapter.framework"
FRAMEWORK_BINARY="$FRAMEWORK/MediaRemoteAdapter"
for architecture in arm64 x86_64; do
  xcrun lipo "$FRAMEWORK_BINARY" -verify_arch "$architecture"
done
xcrun vtool -show-build "$FRAMEWORK_BINARY" | /usr/bin/python3 -c '
import re, sys
minimums = re.findall(r"^\s*minos\s+(\S+)", sys.stdin.read(), re.M)
if len(minimums) != 2 or any(value not in ("14.0", "14.0.0") for value in minimums):
    raise SystemExit("Bridge must contain both architectures with macOS 14.0 deployment target")
'
codesign --verify --strict --verbose=2 "$FRAMEWORK"

if [[ "$MODE" == "--framework-only" ]]; then
  echo "Built $FRAMEWORK"
  exit 0
fi

APP_BUNDLE="$MODE"
[[ -d "$APP_BUNDLE/Contents/MacOS" ]] || { echo "Built app bundle is missing: $APP_BUNDLE" >&2; exit 1; }
RESOURCES="$APP_BUNDLE/Contents/Resources"
ADAPTER_RESOURCES="$RESOURCES/MediaRemoteAdapter"
mkdir -p "$ADAPTER_RESOURCES"
ditto "$FRAMEWORK" "$ADAPTER_RESOURCES/MediaRemoteAdapter.framework"
cp "$VENDOR_DIR/bin/mediaremote-adapter.pl" "$ADAPTER_RESOURCES/mediaremote-adapter.pl"
chmod 644 "$ADAPTER_RESOURCES/mediaremote-adapter.pl"
cp "$VENDOR_DIR/LICENSE" "$ADAPTER_RESOURCES/LICENSE"
cp "$VENDOR_DIR/THIRD_PARTY_NOTICES" "$RESOURCES/THIRD_PARTY_NOTICES"

# Resources are loaded by /usr/bin/perl; the app does not link the framework.
# Nested code must be signed before the caller signs the enclosing app.
codesign --force --sign - --timestamp=none "$ADAPTER_RESOURCES/MediaRemoteAdapter.framework"
codesign --verify --strict --verbose=2 "$ADAPTER_RESOURCES/MediaRemoteAdapter.framework"
echo "Embedded MediaRemoteAdapter resources in $APP_BUNDLE"
