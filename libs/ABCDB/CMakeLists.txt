set(LLVM_LINK_COMPONENTS
  Core
  BitWriter
  Object
  IRReader
  Linker
)

add_llvm_library(ABCDB
  ABCDB.cpp
  ABCDBOnDisk.cpp
)

add_definitions(${LLVM_DEFINITIONS})
