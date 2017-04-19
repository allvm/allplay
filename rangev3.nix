{ stdenv, cmake, fetchFromGitHub }:

stdenv.mkDerivation rec {
  name = "range-v3-${version}";
  version = "2017.02.09";

  src = fetchFromGitHub {
    owner = "ericniebler";
    repo = "range-v3";
    rev = "da1a362efac70c3c524229dd8817f73d367e08d2";
    sha256 = "01g6d87i06dj87fwyf0ji4m4b41n18yvhl89fxnpb0k3f2l79zhg";
  };

  buildInputs = [ cmake ];

  cmakeFlags = [ "-DRANGE_V3_NO_HEADER_CHECK=1" ];

  # Warning about recursive macro in musl headers is not helpful
  patchPhase = stdenv.lib.optionalString stdenv.isMusl ''
    sed -i 's,-Werror,,g' CMakeLists.txt
  '';

  doCheck = true;

  checkTarget = "test";

  enableParallelBuilding = true;
}
