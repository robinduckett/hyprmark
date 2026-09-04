{
  lib,
  inputs,
  self,
}: let
  mkDate = longDate: (lib.concatStringsSep "-" [
    (builtins.substring 0 4 longDate)
    (builtins.substring 4 2 longDate)
    (builtins.substring 6 2 longDate)
  ]);

  version = lib.removeSuffix "\n" (builtins.readFile ../VERSION);
in {
  default = inputs.self.overlays.hyprmark;

  hyprmark = lib.composeManyExtensions [
    inputs.hyprlang.overlays.default
    inputs.hyprutils.overlays.default
    (final: prev: {
      hyprmark = prev.callPackage ./default.nix {
        # Build with the same compiler as hyprutils. A fixed gccNNStdenv here
        # breaks every time hyprutils bumps theirs: libhyprutils.so then wants
        # GLIBCXX symbols our libstdc++ does not have and the link fails.
        stdenv = if prev.stdenv.hostPlatform.isDarwin then prev.stdenv else final.hyprutils.stdenv;
        version = version + "+date=" + (mkDate (inputs.self.lastModifiedDate or "19700101")) + "_" + (inputs.self.shortRev or "dirty");
        inherit (final) hyprlang hyprutils;
        shortRev = self.sourceInfo.shortRev or "dirty";
      };
    })
  ];
}
