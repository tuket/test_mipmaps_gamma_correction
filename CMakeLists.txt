cmake_minimum_required(VERSION 3.8)
project(test_mipmaps)

set (CMAKE_CXX_STANDARD 17)

# this function preppends a path to all files in a list
FUNCTION(PREPEND var prefix)
SET(listVar "")
   FOREACH(f ${ARGN})
   LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

find_package(OpenGL REQUIRED)
if(OPENGL_FOUND)
	include_directories(${OPENGL_INCLUDE_DIRS})
	link_libraries(${OPENGL_LIBRARIES})
endif()

add_subdirectory(libs/glad)
add_subdirectory(libs/glfw)
add_subdirectory(libs/imgui)

set(SOURCES
    main.cpp
)
PREPEND(SOURCES "src/" ${SOURCES})

add_executable(test_mipmaps
    ${SOURCES}
)

target_link_libraries(test_mipmaps glad)
target_link_libraries(test_mipmaps glfw)
target_link_libraries(test_mipmaps imgui)