<p align="center">
  <img src="assets/icon.svg" alt="hyprmark" width="200">
</p>

# hyprmark

<p align="center">
  <a href="https://github.com/robinduckett/hyprmark/actions/workflows/ci.yml"><img src="https://github.com/robinduckett/hyprmark/actions/workflows/ci.yml/badge.svg?branch=main" alt="CI"></a>
  <a href="https://github.com/robinduckett/hyprmark/actions/workflows/release.yml"><img src="https://github.com/robinduckett/hyprmark/actions/workflows/release.yml/badge.svg" alt="Release"></a>
  <a href="https://github.com/robinduckett/hyprmark/releases/latest"><img src="https://img.shields.io/github/v/release/robinduckett/hyprmark?display_name=tag&sort=semver" alt="Latest release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/robinduckett/hyprmark" alt="License: BSD-3-Clause"></a>
</p>

A Markdown viewer for the Hyprland ecosystem.

<p align="center">
  <img src="assets/screenshots/hyprmark-main.png" alt="hyprmark viewing its own README on the left, a tiled code editor on the right, rendered with the hypr-dark theme">
</p>

- Qt6 + QtWebEngine render pipeline: Mermaid diagrams, KaTeX math, `highlight.js` code blocks with copy buttons, tables, task lists, and strike-through.
- 7 built-in themes (purple/cyan `hypr-dark` by default) plus user-dropped `~/.config/hypr/hyprmark/themes/*.css`.
- Live reload on disk edits; hot-reload of `~/.config/hypr/hyprmark.conf`.
- TOC sidebar, PDF export, drag-and-drop, remote-image opt-in.
- Single-instance IPC dispatcher (`hyprmark --dispatch <cmd>`) for Hyprland keybinds.

Status: early development (v0.1.0, pre-alpha).

## Recommended Setup

Add to `~/.config/hypr/hyprland.conf` (or wherever you keep user binds):

```ini
# Launch hyprmark (empty drop-zone window)
bind = $mainMod CTRL, M, exec, hyprmark

# Optional: dispatcher bindings for a running instance
bind = $mainMod CTRL, T, exec, hyprmark --dispatch cycle-theme
bind = $mainMod CTRL, B, exec, hyprmark --dispatch toggle-toc
```

Note hyprland bind syntax: four comma-separated fields - `MODS, KEY, DISPATCHER, PARAMS`. `exec` is the dispatcher; the command it runs is a separate arg after the comma.

## Usage

```sh
hyprmark README.md                        # open a file
hyprmark                                  # show the drop-zone
hyprmark --config /path/to/hyprmark.conf  # override config path
hyprmark --dispatch cycle-theme           # send command to running instance
hyprmark --dispatch list-themes           # print JSON list of themes
hyprmark --dispatch open /path/to/doc.md  # open a file in the running instance
```

Default keybinds: `Ctrl+O` open, `Ctrl+Alt+O` open in new window, `Ctrl+Alt+N` new window, `Ctrl+T` cycle theme, `Ctrl+B` toggle TOC, `Ctrl+F` find, `Ctrl+±` zoom, `Ctrl+P` export PDF, `Ctrl+W` close window.

## Configuration

`~/.config/hypr/hyprmark.conf` is parsed by `hyprlang`; changes apply live. See [`assets/example.conf`](assets/example.conf) for the full option surface.

```
general {
    default_theme = hypr-dark
    live_reload = true
    allow_remote_images = false
}
```

## Install

### Debian / Ubuntu (`.deb`)

`libhyprutils` and `libhyprlang` aren't in the stock archives - grab them from the community Hyprland PPA, then install the released `.deb`:

```sh
sudo add-apt-repository ppa:cppiber/hyprland
sudo apt update
wget https://github.com/robinduckett/hyprmark/releases/latest/download/hyprmark_0.1.0_amd64.deb
sudo apt install ./hyprmark_0.1.0_amd64.deb
```

### AppImage (any distro)

Single self-contained binary with Qt, QtWebEngine, and the hypr* libs bundled - no PPA or system libs needed.

```sh
wget https://github.com/robinduckett/hyprmark/releases/latest/download/hyprmark-0.1.0-x86_64.AppImage
chmod +x hyprmark-0.1.0-x86_64.AppImage
./hyprmark-0.1.0-x86_64.AppImage README.md
```

### Arch (AUR)

`yay -S hyprmark` (or any AUR helper) once the AUR package is published. Until then, build from the PKGBUILD under [`packaging/aur/`](packaging/aur/).

### Nix

```sh
nix run github:robinduckett/hyprmark
# or install into your profile
nix profile install github:robinduckett/hyprmark
```

### From source

```sh
cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build -j$(nproc)
sudo cmake --install build  # optional; installs to /usr/local by default
```

Runtime dependencies: `qt6-base`, `qt6-wayland`, `qt6-webengine`, `qt6-webchannel`, `md4c`, `hyprlang`, `hyprutils`. Debug builds also need `gtest`.

### macOS

Download `hyprmark-<version>-macos-arm64.dmg` from the [releases page](https://github.com/robinduckett/hyprmark/releases), open it and drag hyprmark into Applications. Needs macOS 15 or newer. The app is ad-hoc signed and not notarized, so the first launch is a right-click, then Open. After that, Finder's "Open With" lists hyprmark for Markdown files.

Alternatively, build the Homebrew formula in `packaging/homebrew/hyprmark.rb`:

```sh
brew install --formula --build-from-source packaging/homebrew/hyprmark.rb
```

To build from source directly, hyprutils and hyprlang must come from their tagged releases; their `main` branches refuse to configure on macOS.

```sh
brew install cmake ninja pkg-config qt@6 md4c pixman googletest
export PKG_CONFIG_PATH="$HOME/deps/lib/pkgconfig:$(brew --prefix)/lib/pkgconfig"

git clone --depth 1 --branch v0.14.1 https://github.com/hyprwm/hyprutils.git
cmake -S hyprutils -B hyprutils/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/deps"
cmake --build hyprutils/build --target install

git clone --depth 1 --branch v0.6.8 https://github.com/hyprwm/hyprlang.git
cmake -S hyprlang -B hyprlang/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/deps"
cmake --build hyprlang/build --target install

export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build
open build/hyprmark.app
```

`build/hyprmark.app` is a complete bundle: templates, themes and vendor scripts are copied into it at build time. `packaging/macos/make-dmg.sh` turns it into the same `.dmg` the release workflow publishes.

**pkg-config gotcha.** Put Homebrew's `pkgconfig` directory on `PKG_CONFIG_PATH` before building hyprutils, as above. Some installers add their own `.pc` files to the default search path; the Mono framework is a known case, and it ships a pixman from 2013. If `pkg-config` resolves `pixman-1` to that copy, hyprutils fails with "no matching function for call to `pixman_region32_copy`", because that old API is not const-correct. Check which copy is in use with:

```sh
pkg-config --modversion --variable=includedir pixman-1
```

## License

BSD-3-Clause. See [`LICENSE`](LICENSE).
