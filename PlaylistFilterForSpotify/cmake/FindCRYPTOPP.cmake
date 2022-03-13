# Locate the crypto++ library
#
# This module defines the following variables:
#
# CRYPTOPP_LIBRARY the name of the library;
# CRYPTOPP_INCLUDE_DIR where to find crypto++ include files.
# CRYPTOPP_FOUND true if both the CRYPTOPP_LIBRARY and CRYPTOPP_INCLUDE_DIR have been found.

set( _cryptopp_HEADER_SEARCH_DIRS "${CMAKE_SOURCE_DIR}/include")
set( _cryptopp_LIB_SEARCH_DIRS "${CMAKE_SOURCE_DIR}/lib")

# Search for the header
FIND_PATH(CRYPTOPP_INCLUDE_DIR "cryptopp/cryptlib.h" PATHS ${_cryptopp_HEADER_SEARCH_DIRS} )

# Search for the library
FIND_LIBRARY(CRYPTOPP_LIBRARY NAMES cryptopp
PATHS ${_cryptopp_LIB_SEARCH_DIRS} )

if(NOT CRYPTOPP_LIBRARY)
	message(STATUS "Crypto++ library was not found. Make sure to put the .lib/.a files into the {CMAKE_SOURCE_DIR}/lib/ folder!")
endif()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CRYPTOPP DEFAULT_MSG
CRYPTOPP_LIBRARY CRYPTOPP_INCLUDE_DIR)

