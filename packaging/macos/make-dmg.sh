#!/usr/bin/env bash
# Build the macOS .dmg locally, the same way .github/workflows/release.yml
# does it on the runner.
#
#   packaging/macos/make-dmg.sh [build-dir] [dist-dir]
#
# Expects a Release build of hyprmark.app in build-dir (default: ./build).
# Stages a copy, bundles Qt with macdeployqt, repairs and signs the bundle
# with finalize-bundle.sh, then packs it into dist-dir (default: ./dist).
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
build=${1:-$root/build}
dist=${2:-$root/dist}
version=$(tr -d '[:space:]' < "$root/VERSION")
qt_prefix=${QT_PREFIX:-$(brew --prefix qt@6)}
app="$build/hyprmark.app"
staging="$dist/dmg-staging"
out="$dist/hyprmark-${version}-macos-$(uname -m).dmg"

[ -d "$app" ] || { echo "error: no bundle at $app (build it first: cmake --build $build)"; exit 1; }

echo "==> Staging $app"
rm -rf "$staging"
mkdir -p "$staging"
cp -a "$app" "$staging/"

echo "==> macdeployqt"
"$qt_prefix/bin/macdeployqt" "$staging/hyprmark.app" -always-overwrite

"$here/finalize-bundle.sh" "$staging/hyprmark.app"

echo "==> Creating $out"
ln -s /Applications "$staging/Applications"
hdiutil create -volname "hyprmark ${version}" -srcfolder "$staging" -ov -format UDZO "$out"
hdiutil verify "$out"
echo "==> Done: $out"
