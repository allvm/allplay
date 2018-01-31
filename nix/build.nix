# This file is usually 'default.nix'
{ stdenv
, cmake, git
, llvm, clang
, rangev3
, useClangWerrorFlags ? stdenv.cc.isClang
, allvm-tools
}:

# Make sure no one tries to enable clang-specific flags
# when building using gcc or any non-clang
assert useClangWerrorFlags -> stdenv.cc.isClang;

let
  inherit (stdenv) lib;
  gitrev = if (builtins.pathExists ../.git) then lib.commitIdFromGitRepo ../.git else "0000000000000000000000000000000000000000";
  gitshort = builtins.substring 0 7 gitrev;

  sourceFilter = name: type: let baseName = baseNameOf (toString name); in
    (lib.cleanSourceFilter name type) && !(
      (type == "directory" && (lib.hasPrefix "build" baseName ||
                               lib.hasPrefix "install" baseName))
  );
in

stdenv.mkDerivation {
  name = "allvm-tools-analysis-git-${gitshort}";
  version = gitshort;

  src = builtins.filterSource sourceFilter ./..;

  nativeBuildInputs = [ cmake git ];
  buildInputs = [ llvm rangev3 allvm-tools ];

  doCheck = true;

  cmakeFlags = [
    "-DGITVERSION=${gitshort}-dev"
    "-DCLANGFORMAT=${clang.cc}/bin/clang-format"
  ] ++ stdenv.lib.optional useClangWerrorFlags "-DUSE_CLANG_WERROR_FLAGS=ON";

  # Check formatting, not parallel for more readable output
  preCheck = ''
    make check-format -j1
  '';

  enableParallelBuilding = true;
}
