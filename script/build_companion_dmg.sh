#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_DIR="$ROOT_DIR/companion/mac/ScryBarCompanion"
PROJECT="$PROJECT_DIR/ScryBarCompanion.xcodeproj"
DERIVED_DATA="${SCRYBAR_DERIVED_DATA:-${TMPDIR:-/tmp}/scrybar-companion-release-derived-data.noindex}"
BUILT_APP="$DERIVED_DATA/Build/Products/Release/ScryBarCompanion.app"

VERSION="$(awk -F'"' '/MARKETING_VERSION:/ { print $2; exit }' "$PROJECT_DIR/project.yml")"
if [[ -z "$VERSION" ]]; then
  echo "Could not resolve MARKETING_VERSION from project.yml" >&2
  exit 1
fi

DIST_DIR="$ROOT_DIR/dist"
DMG="$DIST_DIR/ScryBarCompanion-$VERSION.dmg"
WORK_DIR="$(mktemp -d /tmp/scrybar-companion-dmg.XXXXXX)"
VOLUME_DIR="$WORK_DIR/ScryBar Companion"
trap 'rm -rf "$WORK_DIR"' EXIT

"$ROOT_DIR/script/embed_mediaremote_adapter.sh" --check-derived-data "$DERIVED_DATA"

xcodegen generate --spec "$PROJECT_DIR/project.yml" --project "$PROJECT_DIR"

xcodebuild \
  -project "$PROJECT" \
  -scheme ScryBarCompanion \
  -configuration Release \
  -derivedDataPath "$DERIVED_DATA" \
  ARCHS="arm64 x86_64" \
  ONLY_ACTIVE_ARCH=NO \
  SDK_STAT_CACHE_ENABLE="${SCRYBAR_SDK_STAT_CACHE_ENABLE:-YES}" \
  CODE_SIGN_STYLE=Manual \
  CODE_SIGNING_ALLOWED=NO \
  build

"$ROOT_DIR/script/embed_mediaremote_adapter.sh" "$BUILT_APP" "$DERIVED_DATA"
codesign --force --sign - --timestamp=none "$BUILT_APP"
codesign --verify --deep --strict --verbose=2 "$BUILT_APP"

mkdir -p "$VOLUME_DIR"
ditto "$BUILT_APP" "$VOLUME_DIR/ScryBarCompanion.app"
ln -s /Applications "$VOLUME_DIR/Applications"

mkdir -p "$DIST_DIR"
rm -f "$DMG"
hdiutil create \
  -volname "ScryBar Companion" \
  -srcfolder "$VOLUME_DIR" \
  -format UDZO \
  -ov \
  "$DMG"

echo "Built $DMG"
