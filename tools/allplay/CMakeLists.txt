set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  AsmParser
  AsmPrinter
  BitReader
  BitWriter
  Core
  IPO
  MC
  MCParser
  Object
  Support
)

add_llvm_tool(allplay
  allplay.cpp

  # subcommands
  AsmScan.cpp
  CombinedModule.cpp
  Cypher.cpp
  Decompose.cpp
  DecomposeAllexes.cpp
  FindUses.cpp
  FunctionHash.cpp
  Graph.cpp
  Neo.cpp
  NeoDecomposed.cpp
  PrintSource.cpp
  StringGraph.cpp
  Uncombine.cpp

  subcommand-registry.cpp
  # Other
  SplitModule.cpp
)
target_link_libraries(allplay BCDB liball ResourcePaths)

install(PROGRAMS csv2sqlite.sh DESTINATION bin)

add_definitions(${LLVM_DEFINITIONS})
