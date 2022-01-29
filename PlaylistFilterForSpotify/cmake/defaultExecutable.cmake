get_filename_component(FOLDER_VAR ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project("${FOLDER_VAR}")

file(GLOB_RECURSE ADDITIONAL_FILES 
	${CMAKE_CURRENT_SOURCE_DIR}/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

add_compile_definitions(EXECUTABLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")

# Define the executable
add_executable(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES} ${ADDITIONAL_FILES})

# Define the link libraries
target_link_libraries(${PROJECT_NAME} ${LIBS})