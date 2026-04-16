#!/usr/bin/env bash
# update-vendor.sh — pin third-party JS/CSS assets for hyprmark.
#
# Bundling keeps hyprmark offline-first. Run this script to refresh the
# vendored files; commit the outputs to the repo.
#
# Versions pinned here must be kept in step with THIRD_PARTY_LICENSES.

set -euo pipefail

SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
ROOT="$( cd -- "${SCRIPT_DIR}/.." &> /dev/null && pwd )"
OUT="${ROOT}/assets/vendor"
mkdir -p "${OUT}"

MERMAID_VER="11.4.1"
HIGHLIGHT_VER="11.11.1"
KATEX_VER="0.16.11"

fetch() {
    local url="$1"
    local dest="$2"
    echo "→ ${url}"
    curl --fail --silent --show-error --location --output "${dest}" "${url}"
}

echo "Updating vendored assets into ${OUT}"
fetch "https://cdn.jsdelivr.net/npm/mermaid@${MERMAID_VER}/dist/mermaid.min.js"                     "${OUT}/mermaid.min.js"
# highlight.js's npm package ships CommonJS. The browser-ready UMD bundle with
# ~35 common languages pre-registered lives in the sibling @highlightjs/cdn-assets package.
fetch "https://cdn.jsdelivr.net/npm/@highlightjs/cdn-assets@${HIGHLIGHT_VER}/highlight.min.js"     "${OUT}/highlight.min.js"
fetch "https://cdn.jsdelivr.net/npm/katex@${KATEX_VER}/dist/katex.min.js"                          "${OUT}/katex.min.js"
fetch "https://cdn.jsdelivr.net/npm/katex@${KATEX_VER}/dist/katex.min.css"                         "${OUT}/katex.min.css"
fetch "https://cdn.jsdelivr.net/npm/katex@${KATEX_VER}/dist/contrib/auto-render.min.js"            "${OUT}/auto-render.min.js"

# qwebchannel.js is embedded inside the Qt6 WebChannel library and exposed to
# embedded pages as qrc:/qtwebchannel/qwebchannel.js — no vendoring needed.

echo "Wrote to ${OUT}"
ls -la "${OUT}"
