#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-run}"
APP_NAME="ScryBarCompanion"
BUNDLE_ID="com.netmilk.ScryBarCompanion"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$ROOT_DIR/companion/mac/ScryBarCompanion/ScryBarCompanion.xcodeproj"
DERIVED_DATA="$ROOT_DIR/dist/companion-derived-data"
APP_BUNDLE="$DERIVED_DATA/Build/Products/Debug/$APP_NAME.app"
APP_BINARY="$APP_BUNDLE/Contents/MacOS/$APP_NAME"

pkill -x "$APP_NAME" >/dev/null 2>&1 || true

xcodebuild \
  -project "$PROJECT" \
  -scheme "$APP_NAME" \
  -configuration Debug \
  -derivedDataPath "$DERIVED_DATA" \
  build

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
    echo "usage: $0 [run|--debug|--logs|--telemetry|--verify|--bambu-setup|--open]" >&2
    exit 2
    ;;
esac
