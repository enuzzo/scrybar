#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-run}"
APP_NAME="ScryBarCompanion"
BUNDLE_ID="com.netmilk.ScryBarCompanion"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$ROOT_DIR/companion/mac/ScryBarCompanion/ScryBarCompanion.xcodeproj"
DERIVED_DATA="${SCRYBAR_DERIVED_DATA:-${TMPDIR:-/tmp}/scrybar-companion-debug-derived-data.noindex}"
APP_BUNDLE="$DERIVED_DATA/Build/Products/Debug/$APP_NAME.app"
APP_BINARY="$APP_BUNDLE/Contents/MacOS/$APP_NAME"

"$ROOT_DIR/script/embed_mediaremote_adapter.sh" --check-derived-data "$DERIVED_DATA"

xcodebuild \
  -project "$PROJECT" \
  -scheme "$APP_NAME" \
  -configuration Debug \
  -derivedDataPath "$DERIVED_DATA" \
  build

"$ROOT_DIR/script/embed_mediaremote_adapter.sh" "$APP_BUNDLE" "$DERIVED_DATA"
codesign --force --sign - --timestamp=none "$APP_BUNDLE"
codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE"

# Build-only checks must not interrupt a running companion or its camera cleanup.
if [[ "$MODE" == "--build-only" ]]; then exit 0; fi

# A normal quit gives the camera time to switch the printer light off. SIGTERM
# bypasses applicationShouldTerminate and can strand the light on.
if pgrep -x "$APP_NAME" >/dev/null; then
  /usr/bin/osascript -e 'tell application id "com.netmilk.ScryBarCompanion" to quit'
  for attempt in {1..20}; do
    if ! pgrep -x "$APP_NAME" >/dev/null; then break; fi
    sleep 0.5
  done
  if pgrep -x "$APP_NAME" >/dev/null; then
    echo "Companion is still open; resolve its light cleanup prompt before relaunching." >&2
    exit 1
  fi
fi

open_app() {
  if [[ $# -gt 0 ]]; then
    /usr/bin/open -n "$APP_BUNDLE" --args "$@"
  else
    /usr/bin/open -n "$APP_BUNDLE"
  fi
}

case "$MODE" in
  run)
    open_app
    ;;
  --debug|debug)
    lldb -- "$APP_BINARY"
    ;;
  --logs|logs)
    open_app
    /usr/bin/log stream --info --style compact --predicate "process == \"$APP_NAME\""
    ;;
  --telemetry|telemetry)
    open_app
    /usr/bin/log stream --info --style compact --predicate "subsystem == \"$BUNDLE_ID\""
    ;;
  --verify|verify)
    open_app
    sleep 2
    pgrep -x "$APP_NAME" >/dev/null
    ;;
  --bambu-setup|bambu-setup)
    open_app --bambu-setup
    ;;
  --open|open)
    open_app --open
    ;;
  *)
    echo "usage: $0 [run|--build-only|--debug|--logs|--telemetry|--verify|--bambu-setup|--open]" >&2
    exit 2
    ;;
esac
