#!/usr/bin/env bash
# Finalize a macOS .app after macdeployqt so it is self-contained and launchable.
#
#   packaging/macos/finalize-bundle.sh path/to/hyprmark.app
#
# 1. Remove Qt plugins whose framework dependencies macdeployqt could not
#    bundle. Homebrew splits Qt into separate formulae (qtsvg,
#    qtvirtualkeyboard, ...) that macdeployqt does not search, so it copies
#    the plugin but not the framework; such a plugin can never load.
# 2. Strip absolute rpaths (e.g. /opt/homebrew/opt/qt/lib) from the main
#    binary. Left in place they take precedence over the bundled frameworks
#    on machines that have Homebrew Qt installed, mixing two Qt builds.
# 3. Re-sign ad-hoc. macdeployqt rewrites install names, invalidating the
#    linker's signature, and Apple Silicon refuses to launch unsigned code.
set -euo pipefail

app=${1:?usage: finalize-bundle.sh path/to/App.app}
frameworks="$app/Contents/Frameworks"
main="$app/Contents/MacOS/$(basename "$app" .app)"

echo "==> Pruning plugins with unbundled framework dependencies"
while IFS= read -r plugin; do
  for fw in $(otool -L "$plugin" | grep -oE '@rpath/Qt[A-Za-z0-9]+\.framework' | sort -u); do
    name=${fw#@rpath/}
    if [ ! -d "$frameworks/$name" ]; then
      echo "    removing ${plugin#"$app/"} (needs $name)"
      rm -f "$plugin"
      break
    fi
  done
done < <(find "$app/Contents/PlugIns" -name '*.dylib' 2>/dev/null)

# Every Mach-O file in the bundle (frameworks are mode 644, so do not rely
# on the executable bit or a .dylib suffix). Computed once, after pruning,
# so the passes below never see a plugin that was just removed.
_machos=$(find "$app" -type f ! -name '*.pak' ! -name '*.dat' ! -name '*.qm' ! -name '*.png' ! -name '*.plist' \
            -exec sh -c 'file -b "$1" | grep -q "^Mach-O"' _ {} \; -print)
machos() { printf '%s\n' "$_machos"; }

echo "==> Stripping absolute rpaths from $(basename "$main")"
for rp in $(otool -l "$main" | awk '/LC_RPATH/{f=1} f&&/path /{print $2; f=0}'); do
  case "$rp" in
    @*) ;;
    *)  echo "    removing rpath $rp"; install_name_tool -delete_rpath "$rp" "$main" ;;
  esac
done

echo "==> Rewriting absolute dependency paths to bundled copies"
# macdeployqt does not fix up everything: with Homebrew's Qt (absolute
# install names, split formulae) the QtWebEngineProcess helper keeps linking
# /opt/homebrew/... frameworks and some dylibs keep their Homebrew LC_ID.
# Point each such reference at the copy in Contents/Frameworks via @rpath and
# make sure the binary carries an rpath that reaches that directory.
unresolved=0
while IFS= read -r bin; do
  rel=$(python3 -c 'import os,sys; print(os.path.relpath(sys.argv[1], os.path.dirname(sys.argv[2])))' "$frameworks" "$bin")
  while IFS= read -r dep; do
    [ -n "$dep" ] || continue
    case "$dep" in
      *.framework/*) name=$(printf '%s' "$dep" | sed -E 's#.*/([A-Za-z0-9_]+\.framework/.*)#\1#') ;;
      *)             name=$(basename "$dep") ;;
    esac
    if [ ! -e "$frameworks/$name" ]; then
      echo "    UNRESOLVED ${bin#"$app/"} -> $dep"; unresolved=1; continue
    fi
    if [ "$(basename "$dep")" = "$(basename "$bin")" ]; then
      install_name_tool -id "@rpath/$name" "$bin" 2>/dev/null
    else
      echo "    ${bin#"$app/"}: $dep -> @rpath/$name"
      install_name_tool -change "$dep" "@rpath/$name" "$bin" 2>/dev/null
    fi
    if ! otool -l "$bin" | grep -q " path @loader_path/$rel "; then
      install_name_tool -add_rpath "@loader_path/$rel" "$bin" 2>/dev/null \
        || { echo "    cannot add rpath to ${bin#"$app/"}"; unresolved=1; }
    fi
  done < <(otool -L "$bin" | tail -n +2 | awk '{print $1}' | grep -E '^(/opt/homebrew|/usr/local|/Users)' || true)
done < <(machos)
[ "$unresolved" -eq 0 ] || { echo "error: unresolved dependencies remain"; exit 1; }

echo "==> Converting @executable_path references to @rpath"
# macdeployqt rewrites dependencies as @executable_path/../Frameworks/X.
# That is resolved against the *process* executable, so inside the nested
# QtWebEngineProcess.app helper it points at a directory that does not
# exist and the helper dies at load time ("Library not loaded: ...
# libicui18n") leaving the web view blank. @rpath is resolved through the
# rpath chain, which both the main binary and the helper carry.
while IFS= read -r bin; do
  while IFS= read -r dep; do
    [ -n "$dep" ] || continue
    name=${dep#@executable_path/../Frameworks/}
    if [ "$(basename "$dep")" = "$(basename "$bin")" ] && [ "${bin#"$app/Contents/MacOS/"}" != "$bin" ]; then
      continue
    fi
    if [ "$(basename "$dep")" = "$(basename "$bin")" ]; then
      install_name_tool -id "@rpath/$name" "$bin" 2>/dev/null
    else
      install_name_tool -change "$dep" "@rpath/$name" "$bin" 2>/dev/null
    fi
  done < <(otool -L "$bin" | tail -n +2 | awk '{print $1}' | grep -E '^@executable_path/\.\./Frameworks/' || true)
  rel=$(python3 -c 'import os,sys; print(os.path.relpath(sys.argv[1], os.path.dirname(sys.argv[2])))' "$frameworks" "$bin")
  if [ "$rel" != "." ] && ! otool -l "$bin" | grep -qE " path (@loader_path/$rel|@loader_path/$rel/|@executable_path/\.\./Frameworks) "; then
    install_name_tool -add_rpath "@loader_path/$rel" "$bin" 2>/dev/null \
      || { echo "    cannot add rpath to ${bin#"$app/"}"; unresolved=1; }
  fi
done < <(machos)
[ "$unresolved" -eq 0 ] || { echo "error: could not add rpaths"; exit 1; }

echo "==> Checking for references outside the bundle"
leaks=0
while IFS= read -r bin; do
  if otool -L "$bin" 2>/dev/null | tail -n +2 | grep -qE '^\s*(/opt/homebrew|/usr/local|/Users)'; then
    echo "    $bin"; otool -L "$bin" | grep -E '^\s*(/opt/homebrew|/usr/local|/Users)'; leaks=1
  fi
done < <(machos)
[ "$leaks" -eq 0 ] && echo "    none" || { echo "error: bundle still references files outside itself"; exit 1; }

echo "==> Signing (ad-hoc)"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
echo "==> Done: $app"
