{ stdenv, fetchFromGitHub, cmake }:

stdenv.mkDerivation rec {
  name = "range-v3-${version}";
  version = "0.9.1";

  src = fetchFromGitHub {
    owner = "ericniebler";
    repo = "range-v3";
    rev = version;
    sha256 = "0qga2fnfrlrzrvnnk1z1plpmvcr8b4c75g5xz0jv0sav0kmq5zwn";
  };

  nativeBuildInputs = [ cmake ];

  # Warning about recursive macro in musl headers is not helpful
  CXXFLAGS = [ "-Wno-disabled-macro-expansion" ];

  cmakeFlags = [
    "-DRANGE_V3_PERF=OFF"
    "-DRANGE_V3_EXAMPLES=OFF"
    "-DRANGE_V3_DOCS=OFF"
    "-DRANGES_NATIVE=OFF"

    "-DRANGE_V3_TESTS=ON"
    "-DRANGE_V3_HEADER_CHECKS=ON"
  ];

  doCheck = true;
  checkTarget = "test";

  enableParallelBuilding = true;

  meta = with stdenv.lib; {
    description = "Experimental range library for C++11/14/17";
    homepage = https://github.com/ericniebler/range-v3;
    license = licenses.boost;
    platforms = platforms.all;
    maintainers = with maintainers; [ dtzWill /* xwvvvvwx */ ];
  };
}
