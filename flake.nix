{
  description = "Markdown viewer for Hyprland";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    systems.url = "github:nix-systems/default-linux";

    hyprutils = {
      url = "github:hyprwm/hyprutils";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    hyprlang = {
      url = "github:hyprwm/hyprlang";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };
  };

  outputs = {
    self,
    nixpkgs,
    systems,
    ...
  } @ inputs: let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);
    pkgsFor = eachSystem (system:
      import nixpkgs {
        localSystem.system = system;
        overlays = with self.overlays; [default];
      });
  in {
    overlays = import ./nix/overlays.nix {inherit inputs lib self;};

    packages = eachSystem (system: {
      default = self.packages.${system}.hyprmark;
      inherit (pkgsFor.${system}) hyprmark;
    });

    devShells = eachSystem (system: {
      default = pkgsFor.${system}.mkShell {
        inputsFrom = [self.packages.${system}.hyprmark];
        packages = with pkgsFor.${system}; [gdb clang-tools gcovr];
      };
    });

    checks = eachSystem (system: self.packages.${system});

    formatter = eachSystem (system: pkgsFor.${system}.alejandra);
  };
}
