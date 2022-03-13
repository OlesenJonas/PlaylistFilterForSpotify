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

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD    # Adds a post-build event to MyTest
    COMMAND ${CMAKE_COMMAND} -E copy_if_different  		# which executes "cmake - E copy_if_different..."
        "${CMAKE_SOURCE_DIR}/lib/cryptopp.dll"      	# <--this is in-file
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)                 		# <--this is out-file path