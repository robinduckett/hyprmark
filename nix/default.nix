{
  lib,
  stdenv,
  cmake,
  pkg-config,
  qt6,
  md4c,
  hyprlang,
  hyprutils,
  wrapQtAppsHook,
  version ? "git",
  shortRev ? "",
}:
stdenv.mkDerivation {
  pname = "hyprmark";
  inherit version;

  src = ../.;

  nativeBuildInputs = [
    cmake
    pkg-config
    wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtwayland
    qt6.qtwebengine
    qt6.qtwebchannel
    md4c
    hyprlang
    hyprutils
  ];

  cmakeFlags = lib.mapAttrsToList lib.cmakeFeature {
    HYPRMARK_COMMIT = shortRev;
    HYPRMARK_VERSION_COMMIT = ""; # stays empty so --version always shows commit
  };

  dontWrapQtApps = false;

  meta = {
    homepage = "https://github.com/hyprwm/hyprmark";
    description = "Markdown viewer for Hyprland";
    license = lib.licenses.bsd3;
    platforms = lib.platforms.linux;
    mainProgram = "hyprmark";
  };
}
