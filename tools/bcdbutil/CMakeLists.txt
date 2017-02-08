set(LLVM_LINK_COMPONENTS
  BitReader
  BitWriter
  Core
  Support
)

add_llvm_tool(bcdbutil
  bcdbutil.cpp

  CombinedModule.cpp
  FindUses.cpp
  FunctionHash.cpp
  Graph.cpp

  subcommand-registry.cpp
)
target_link_libraries(bcdbutil BCDB liball ResourcePaths)

add_definitions(${LLVM_DEFINITIONS})
