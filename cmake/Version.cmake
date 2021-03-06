# Get a description of the git rev we're building from,
# and embed that information in ALLVMVersion.h

if(NOT GITVERSION)
  include(GetGitRevisionDescription)
  git_describe(GITVERSION --tags --always --dirty)
endif()

if (NOT GITVERSION)
  message(FATAL_ERROR "Unable to find 'git'! Is it installed?")
else()
  message(STATUS "Detected ALLVM Analysis source version: ${GITVERSION}")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/allvm-analysis/GitVersion.h.in
  ${ALLVM_INCLUDE_DIR}/allvm-analysis/GitVersion.h
)

include_directories(${ALLVM_INCLUDE_DIR})


