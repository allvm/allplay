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
callPackage ./build.nix {
  inherit (llvmPackages_4) llvm clang lld;
  stdenv = overrideCC stdenv gcc-snapshot;
  rangev3 = callPackage ./rangev3.nix {
    stdenv = overrideCC stdenv gcc-snapshot;
  };
}
