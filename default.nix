{ nixpkgs ? import ./nix/fetch-nixpkgs.nix }:

with import nixpkgs {};
{
  allvm-analysis = callPackage ./nix/build.nix {
    inherit (llvmPackages_4) stdenv llvm clang;
    rangev3 = callPackage ./nix/rangev3.nix {
      stdenv = llvmPackages_4.stdenv;
    };
  };
}
