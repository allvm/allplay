{ stdenv, fetchFromGitHub, cmake }:

stdenv.mkDerivation rec {
  pname = "range-v3";
  version = "0.11.0";

  src = fetchFromGitHub {
    owner = "ericniebler";
    repo = pname;
    rev = version;
    sha256 = "18230bg4rq9pmm5f8f65j444jpq56rld4fhmpham8q3vr1c1bdjh";
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
