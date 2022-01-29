# This module defines the following variables (on success):
# GLM_INCLUDE_DIRS - where to find glm/glm.hpp
# GLM_FOUND - if the library was successfully located

set(GLM_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include/)
if(EXISTS ${GLM_INCLUDE_DIRS}/glm/glm.hpp)
	set(GLM_FOUND true)
endif()
