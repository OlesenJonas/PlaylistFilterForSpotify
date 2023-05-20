# get libraries from vcpk
find_package(cpr CONFIG REQUIRED)
find_package(cryptopp CONFIG REQUIRED)
find_package(daw-json-link CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

target_compile_definitions(glfw INTERFACE "-DGLFW_INCLUDE_NONE" )
target_compile_definitions(glm::glm INTERFACE "-DGLM_FORCE_RADIANS")