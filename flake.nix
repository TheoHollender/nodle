{
  description = "nodle: C++ project built with Make, packaged with Nix";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      common = pkgs: {
        version = "0.1.0";
        src = self;
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildInputs = [ pkgs.gtest ];
      };
    in
    {
      packages = forAllSystems (pkgs: {
        # `nix build` -> ./result/bin/nodle (no tests run here)
        default = pkgs.stdenv.mkDerivation (common pkgs // {
          pname = "nodle";
          makeFlags = [ "PREFIX=${placeholder "out"}" ];
          doCheck = false;
        });

        # GoogleTest runner binary: ./result/bin/nodle-tests
        tests = pkgs.stdenv.mkDerivation (common pkgs // {
          pname = "nodle-tests";
          buildFlags = [ "build/tests/run_tests" ];
          installPhase = ''
            install -Dm755 build/tests/run_tests $out/bin/nodle-tests
          '';
        });
      });

      apps = forAllSystems (pkgs: {
        # `nix run`
        default = {
          type = "app";
          program = "${self.packages.${pkgs.system}.default}/bin/nodle";
        };
        # `nix run .#test` (extra gtest flags work: `nix run .#test -- --gtest_filter='Greeter.*'`)
        test = {
          type = "app";
          program = "${self.packages.${pkgs.system}.tests}/bin/nodle-tests";
        };
      });

      # `nix flake check` runs the tests in the sandbox
      checks = forAllSystems (pkgs: {
        default = pkgs.runCommand "nodle-tests-check" { } ''
          ${self.packages.${pkgs.system}.tests}/bin/nodle-tests
          touch $out
        '';
      });

      # `nix develop` (or direnv with `use flake`)
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.system}.tests ];
          packages = with pkgs; [
            gnumake
            gdb
            clang-tools   # clangd, clang-format, clang-tidy
            bear          # generates compile_commands.json: `bear -- make`
            valgrind
          ];
        };

        # `nix develop .#ci`: minimal shell for CI linting
        ci = pkgs.mkShell { packages = [ pkgs.clang-tools ]; };
      });

      formatter = forAllSystems (pkgs: pkgs.nixpkgs-fmt);
    };
}
