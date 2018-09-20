{
nixpkgs ? import ./nix/fetch-nixpkgs.nix,
allvm-tools-src ? (builtins.fetchGit https://github.com/allvm/allvm-tools),
allvm-tools ? (import allvm-tools-src {}).allvm-tools
}:

with (import nixpkgs {}).pkgsMusl;
{
  allvm-analysis = callPackage ./nix/build.nix {
    inherit allvm-tools;
    inherit (llvmPackages_4) stdenv llvm clang;
    rangev3 = callPackage ./nix/rangev3.nix {
      stdenv = llvmPackages_4.stdenv;
    };
  };
}
