{ stdenv, cmake, fetchFromGitHub }:

let
  isMusl = stdenv.isMusl or false;
in stdenv.mkDerivation rec {
  name = "range-v3-${version}";
  version = "0.3.0";

  src = fetchFromGitHub {
    owner = "ericniebler";
    repo = "range-v3";
    rev = version;
    sha256 = "176vrxxq1ay7lvr2g3jh6wc2ds6yriaqdyf4s5cv8mbbsqis6z1n";
  };

  buildInputs = [ cmake ];

  cmakeFlags = [ "-DRANGE_V3_NO_HEADER_CHECK=1" ];

  # Warning about recursive macro in musl headers is not helpful
  patchPhase = stdenv.lib.optionalString isMusl ''
    sed -i 's,-Werror,,g' CMakeLists.txt
  '';

  doCheck = true;

  checkTarget = "test";

  enableParallelBuilding = true;
}
