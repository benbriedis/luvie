cmake_minimum_required(VERSION 3.28)
project(luvie LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)        # replaces `bear`
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)              # matches Makefile `-g`
endif()

include(GNUInstallDirs)

# Vendored FLTK (static)
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/deps/installation")
find_package(FLTK 1.5 CONFIG REQUIRED)

# System libs (same as current pkg-config usage)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
pkg_check_modules(LO   REQUIRED IMPORTED_TARGET liblo)

add_subdirectory(src)
add_subdirectory(lv2)
