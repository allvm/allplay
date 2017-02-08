set(LLVM_LINK_COMPONENTS
  Core
  BitWriter
  Object
  IRReader
  Linker
)

add_llvm_library(BCDB
  BCDB.cpp
  BCDBOnDisk.cpp
)

add_definitions(${LLVM_DEFINITIONS})
