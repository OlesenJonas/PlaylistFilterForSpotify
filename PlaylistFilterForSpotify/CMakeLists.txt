cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(SpotifyPlaylistFilter)

##############################################################################

#Compiler Commands etc

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if("${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
	SET(BUILD_TYPE_64_BIT true)
endif()

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
   SET(BUILD_TYPE_64_BIT true)
endif()

if(CMAKE_CL_64)
   SET(BUILD_TYPE_64_BIT true)
endif()

add_definitions(-DNOMINMAX) #dont want windows.h or other predefined min/max functions

#Set desired compile commands here. not forcing any atm, getting warnings from clangd
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
		message("Using Clang-cl MSVC Compiler")
		add_compile_options(/W4)
		#add_compile_options(/W4 /WX)
	else()
		message("Using Clang Compiler")
		# add_compile_options(-Wall)
		# add_compile_options(-Wall -Wextra -pedantic)
	endif()
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	message("Using GNU Compiler")
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
	message("Using Intel Compiler")
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	message("Using MSCV Compiler")
    # add_compile_options(/W4 /WX)
endif()

##############################################################################

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/debug)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/debug)
#on windows RUNTIME_OUTPUT decides where .dll s will be put
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/out/debug)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib/release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib/release)
#on windows RUNTIME_OUTPUT decides where .dll s will be put
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/out/release)

# Add .lib files
link_directories(${CMAKE_SOURCE_DIR}/lib)

# Add source files
file(GLOB_RECURSE SOURCE_FILES 
	${CMAKE_SOURCE_DIR}/src/*.c
	${CMAKE_SOURCE_DIR}/src/*.cpp
)
	
# Add header files
file(GLOB_RECURSE HEADER_FILES 
	${CMAKE_SOURCE_DIR}/src/*.h
	${CMAKE_SOURCE_DIR}/src/*.hpp
)
	
#list(FILTER SOURCE_FILES EXCLUDE REGEX ".*main.cpp$")
# message(STATUS ${SOURCE_FILES})
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*/_Executables/.*$") #exclude files from the specific executables' folders
# message(STATUS ${SOURCE_FILES})

IF(CMAKE_BUILD_TYPE MATCHES Debug)
	add_compile_definitions(MISC_PATH="${CMAKE_SOURCE_DIR}/misc")
	add_compile_definitions(SHADERS_PATH="${CMAKE_SOURCE_DIR}/src/Shaders")
ELSEIF(CMAKE_BUILD_TYPE MATCHES Release)
	add_compile_definitions(MISC_PATH="./misc")
	add_compile_definitions(SHADERS_PATH="./Shaders")
ELSE()
	message(FATAL_ERROR "unknown BUILD_TYPE")
ENDIF()

# We need a CMAKE_DIR with some code to find external dependencies
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

#######################################
# LOOK for the packages that we need! #
#######################################
# OpenGL
find_package(OpenGL REQUIRED)

# GLM
find_package(GLM REQUIRED)
message(STATUS "GLM included at ${GLM_INCLUDE_DIR}")

# GLFW
find_package(GLFW3 REQUIRED)
message(STATUS "Found GLFW3 in ${GLFW3_INCLUDE_DIR}")

# STB_IMAGE
add_library(STB_IMAGE STATIC "src/thirdparty/stb_image.cpp")

# GLAD
add_library(GLAD STATIC "src/thirdparty/glad.c")

# CPR
include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG beb9e98806bb84bcc130a2cebfbcbbc6ce62b335) # The commit hash for 1.7.2. Replace with the latest from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

# Put all libraries into a variable
# set(LIBS glfw3 "cryptopp.lib" opengl32 STB_IMAGE GLAD cpr::cpr)
set(LIBS glfw3 debug cryptlibDebug optimized cryptlib opengl32 STB_IMAGE GLAD cpr::cpr)

# Define the include DIRs
include_directories(
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/src/thirdparty"
)
add_subdirectory("${CMAKE_SOURCE_DIR}/src/_Executables/")