{ stdenv, cmake, fetchFromGitHub }:

let
  isMusl = stdenv.isMusl or false;
in stdenv.mkDerivation rec {
  name = "range-v3-${version}";
  version = "2.5";

  src = fetchFromGitHub {
    owner = "ericniebler";
    repo = "range-v3";
    rev = "8e12b0ea21b4e2c57fd0e77726b0e51b96e6a8b6";
    sha256 = "14avawj4ycnff5cz7gwvcr19abahlhrv2b41glkmahj16an04qjr";
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
