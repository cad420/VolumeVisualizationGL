cmake_minimum_required(VERSION 3.12)
project(VolumeVisualizationGL)


set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
if(${isSystemDir} STREQUAL "-1")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()

include(scripts/external.cmake)

vm_external_module(GIT_REPOSITORY https://github.com/cad420/VMCore.git GIT_TAG ysldev)
add_subdirectory(src)

option(BUILD_TEST On)
if(BUILD_TEST)
add_subdirectory(test)
endif()