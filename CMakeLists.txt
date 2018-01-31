cmake_minimum_required (VERSION 3.4)

project (ALLVM-Analysis CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_definitions(-Wall)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
if (${LLVM_PACKAGE_VERSION} VERSION_LESS 4.0)
  message(FATAL_ERROR "LLVM 4.0 or newer is required")
endif()

message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

find_package(ALLVM REQUIRED)
message(STATUS "Found ALLVM ${ALLVM_VERSION}")
message(STATUS "Found ALLVM in: ${ALLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(AddLLVM)
include(HandleLLVMOptions)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(LLVM_BUILD_TOOLS ON)
set(LLVM_REQUIRES_RTTI ON)
set(LLVM_ENABLE_EH ON)

##########

option(USE_CLANG_WERROR_FLAGS OFF)

if (USE_CLANG_WERROR_FLAGS)
  set(WARNING_FLAGS -Weverything
    -Wno-unknown-warning-option -Wno-c++98-compat -Wno-padded
    -Wno-global-constructors -Wno-exit-time-destructors -Wno-unused-macros)
  list(APPEND LLVM_DEFINITIONS ${WARNING_FLAGS} -Werror)
endif()

# Put libs and bins in expected place.
# XXX: We don't setup CMAKE_CFG_INTDIR to distinguish Release/Debug/etc.
set(LLVM_RUNTIME_OUTPUT_INTDIR
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR
    ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib${LLVM_LIBDIR_SUFFIX})

link_directories(${LLVM_LIBRARY_DIR})
include_directories(${LLVM_INCLUDE_DIR})

### Headers

# Generated header path
set(ALLVM_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)

include_directories (${PROJECT_SOURCE_DIR}/include ${ALLVM_INCLUDE_DIR})

install(DIRECTORY include/allvm ${ALLVM_INCLUDE_DIR}/allvm
  DESTINATION include
  COMPONENT allvm-headers
  FILES_MATCHING PATTERN "*.h"
)
###

include(CheckFormat)
include(Version)

add_subdirectory (libs)
add_subdirectory (tools)
#add_subdirectory (test)
