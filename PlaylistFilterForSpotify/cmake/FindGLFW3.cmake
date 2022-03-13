# Locate the glfw3 library
#
# This module defines the following variables:
#
# GLFW3_LIBRARY the name of the library;
# GLFW3_INCLUDE_DIR where to find glfw include files.
# GLFW3_FOUND true if both the GLFW3_LIBRARY and GLFW3_INCLUDE_DIR have been found.

set( _glfw3_HEADER_SEARCH_DIRS "${CMAKE_SOURCE_DIR}/include")
set( _glfw3_LIB_SEARCH_DIRS "${CMAKE_SOURCE_DIR}/lib")

# Search for the header
FIND_PATH(GLFW3_INCLUDE_DIR "GLFW/glfw3.h" PATHS ${_glfw3_HEADER_SEARCH_DIRS} )

# Search for the library
FIND_LIBRARY(GLFW3_LIBRARY NAMES glfw3 glfw
PATHS ${_glfw3_LIB_SEARCH_DIRS} )

if(NOT GLFW3_LIBRARY)
	message(STATUS "GLFW library was not found. Make sure to put the .lib/.a files into the {CMAKE_SOURCE_DIR}/lib/ folder!")
endif()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GLFW3 DEFAULT_MSG
GLFW3_LIBRARY GLFW3_INCLUDE_DIR)

