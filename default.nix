let
  default_nixpkgs = (import <nixpkgs> {}).fetchFromGitHub {
    owner = "NixOS";
    repo = "nixpkgs-channels";
    rev = "f0fac3b578086066b47360de17618448d066b30e"; # current -unstable
    sha256 = "1mpwdminwk1wzycwmgi2c2kwpbcfjwmxiakn7bmvvsaxb30gwyyb";
  };
in
{ nixpkgs ? default_nixpkgs }:

with import nixpkgs {};
let
  gcc_stdenv = overrideCC stdenv gcc-snapshot;
  clang_stdenv = llvmPackages_4.stdenv;
in rec {
  # Nope, still doesn't work :(
  # allvm-tools-gcc = callPackage ./build.nix {
  #   inherit (llvmPackages_4) llvm clang lld;
  #   stdenv = gcc_stdenv;
  #   rangev3 = callPackage ./rangev3.nix {
  #     stdenv = gcc_stdenv;
  #   };
  # };

  allvm-tools-clang = callPackage ./build.nix {
    inherit (llvmPackages_4) llvm clang lld;
    stdenv = clang_stdenv;
    rangev3 = callPackage ./rangev3.nix {
      stdenv = clang_stdenv;
    };
  };

}
