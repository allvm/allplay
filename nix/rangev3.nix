{ stdenv, cmake, fetchFromGitHub }:

let
  isMusl = stdenv.hostPlatform.isMusl or false;
in stdenv.mkDerivation rec {
  name = "range-v3-${version}";
  version = "0.3.5";

  src = fetchFromGitHub {
    owner = "ericniebler";
    repo = "range-v3";
    rev = version;
    sha256 = "00bwm7n3wyf49xpr7zjhm08dzwx3lwibgafi6isvfls3dhk1m4kp";
  };

  buildInputs = [ cmake ];

  cmakeFlags = [ "-DRANGE_V3_NO_HEADER_CHECK=1" ];

  doCheck = true;

  checkTarget = "test";

  enableParallelBuilding = true;
}
