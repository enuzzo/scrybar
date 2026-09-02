#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_DIR="$ROOT_DIR/companion/mac/ScryBarCompanion"
PROJECT="$PROJECT_DIR/ScryBarCompanion.xcodeproj"
DERIVED_DATA="$ROOT_DIR/dist/companion-release-derived-data"
BUILT_APP="$DERIVED_DATA/Build/Products/Release/ScryBarCompanion.app"
PUBLIC_APP="$ROOT_DIR/companion/mac/dist/ScryBarCompanion.app"

VERSION="$(awk -F'"' '/MARKETING_VERSION:/ { print $2; exit }' "$PROJECT_DIR/project.yml")"
if [[ -z "$VERSION" ]]; then
  echo "Could not resolve MARKETING_VERSION from project.yml" >&2
  exit 1
fi

DMG="$ROOT_DIR/companion/mac/ScryBarCompanion-$VERSION.dmg"
WORK_DIR="$(mktemp -d /tmp/scrybar-companion-dmg.XXXXXX)"
VOLUME_DIR="$WORK_DIR/ScryBar Companion"
trap 'rm -rf "$WORK_DIR"' EXIT

xcodegen generate --spec "$PROJECT_DIR/project.yml" --project "$PROJECT_DIR"

xcodebuild \
  -project "$PROJECT" \
  -scheme ScryBarCompanion \
  -configuration Release \
  -derivedDataPath "$DERIVED_DATA" \
  ARCHS="arm64 x86_64" \
  ONLY_ACTIVE_ARCH=NO \
  CODE_SIGN_IDENTITY="-" \
  build

codesign --verify --deep --strict --verbose=2 "$BUILT_APP"

rm -rf "$PUBLIC_APP"
mkdir -p "$(dirname "$PUBLIC_APP")"
ditto "$BUILT_APP" "$PUBLIC_APP"

mkdir -p "$VOLUME_DIR"
ditto "$BUILT_APP" "$VOLUME_DIR/ScryBarCompanion.app"
ln -s /Applications "$VOLUME_DIR/Applications"

rm -f "$DMG"
hdiutil create \
  -volname "ScryBar Companion" \
  -srcfolder "$VOLUME_DIR" \
  -format UDZO \
  -ov \
  "$DMG"

echo "Built $DMG"
