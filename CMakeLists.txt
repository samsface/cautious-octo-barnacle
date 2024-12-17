cmake_minimum_required(VERSION 3.10)
project(gdnative-example)

###############################################################################
# options
###############################################################################

###############################################################################
# libs
###############################################################################

add_subdirectory(lib/godot-cpp)

###############################################################################

# install build into example project by default
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(
    CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/addons/sbhot" 
    CACHE PATH "..." 
    FORCE)
endif()

# let's us link depended dlls relative to process on linux
set(CMAKE_INSTALL_RPATH "$ORIGIN")

add_library(sbhot SHARED src/sbhot.cpp src/register_types.cpp)

target_link_libraries(
  sbhot
  PRIVATE
    godot-cpp
)

set_target_properties(
  sbhot
  PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)

if(APPLE)
  set_target_properties(
    sbhot
    PROPERTIES 
    OSX_ARCHITECTURES "arm64;x86_64")
endif()

install(TARGETS sbhot DESTINATION .)
