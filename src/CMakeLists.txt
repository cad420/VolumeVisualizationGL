
cmake_minimum_required(VERSION 3.12)

aux_source_directory(. SRC)

find_package(glfw3 CONFIG REQUIRED)

add_subdirectory(plugins)

add_executable(volvis)
target_sources(volvis PRIVATE "../gl3w/GL/gl3w.c" ${SRC})
if(WIN32)
target_link_libraries(volvis glfw vmcore)
else()
target_link_libraries(volvis glfw vmcore dl)
endif()
target_include_directories(volvis PRIVATE "../include" "../gl3w" ${glfw_INCLUDE_DIRS})

install(TARGETS volvis LIBRARY DESTINATION "lib" RUNTIME DESTINATION "bin" ARCHIVE DESTINATION "lib")
install(TARGETS vmcore LIBRARY DESTINATION "lib" RUNTIME DESTINATION "bin" ARCHIVE DESTINATION "lib")
