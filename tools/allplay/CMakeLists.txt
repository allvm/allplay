set(LLVM_LINK_COMPONENTS
  BitReader
  BitWriter
  Core
  Support
)

add_llvm_tool(allplay
  allplay.cpp

  CombinedModule.cpp
  FindUses.cpp
  FunctionHash.cpp
  Graph.cpp

  subcommand-registry.cpp
)
target_link_libraries(allplay BCDB liball ResourcePaths)

add_definitions(${LLVM_DEFINITIONS})
