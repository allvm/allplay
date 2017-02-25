set(LLVM_LINK_COMPONENTS
  BitReader
  BitWriter
  Core
  Support
)

add_llvm_tool(allplay
  allplay.cpp

  CombinedModule.cpp
  Cypher.cpp
  Decompose.cpp
  FindUses.cpp
  FunctionHash.cpp
  Graph.cpp
  Neo.cpp
  StringGraph.cpp
  Uncombine.cpp

  subcommand-registry.cpp
)
target_link_libraries(allplay BCDB liball ResourcePaths)

install(PROGRAMS csv2sqlite.sh DESTINATION bin)

add_definitions(${LLVM_DEFINITIONS})
