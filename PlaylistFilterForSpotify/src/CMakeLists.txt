# OpenGL
find_package(OpenGL REQUIRED)

# using vcpkg atm
# # CPR
# include(FetchContent)
# FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
#                          GIT_TAG beb9e98806bb84bcc130a2cebfbcbbc6ce62b335) # The commit hash for 1.7.2. Replace with the latest from: https://github.com/libcpr/cpr/releases
# FetchContent_MakeAvailable(cpr)

include(vcpkgTargets)

add_subdirectory("thirdparty/")
add_subdirectory("PlaylistFilter/")