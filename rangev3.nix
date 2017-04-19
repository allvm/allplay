{ stdenv, cmake, fetchFromGitHub }:

let
  isMusl = stdenv.isMusl or false;
in stdenv.mkDerivation rec {
  name = "range-v3-${version}";
  version = "2.4";

  src = fetchFromGitHub {
    owner = "ericniebler";
    repo = "range-v3";
    rev = "d753c6652f3bd1bb9fac42cfbf11fd55d856f97d";
    sha256 = "0qxmxqclq8nn5b5d1z8dfy5sw209ch6lxgfqmnsxmjwr6ms6v67z";
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
