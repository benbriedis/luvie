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
include(FetchContent)

# ---- Vendored dependencies: fetched + pinned for reproducibility ----
# FLTK 1.5 (dev) — built static as part of configure. Only the core lib is used,
# so skip fluid/tests/examples/GL/forms to keep the build lean.
set(FLTK_BUILD_SHARED_LIBS OFF)
set(FLTK_BUILD_FLUID    OFF)
set(FLTK_BUILD_TEST     OFF)
set(FLTK_BUILD_EXAMPLES OFF)
set(FLTK_BUILD_GL       OFF)
set(FLTK_BUILD_FORMS    OFF)
FetchContent_Declare(fltk
    GIT_REPOSITORY https://github.com/fltk/fltk.git
    # FLTK 1.5 is unreleased (dev/master), so there is no version tag — pin the
    # exact commit. Bump this SHA to move FLTK forward.
    GIT_TAG        89c116bf5f418e2b9821e4c83052735763a6bf20
)

# LV2 — headers only (upstream is meson-based; nothing to compile/link).
FetchContent_Declare(lv2
    GIT_REPOSITORY https://github.com/lv2/lv2.git
    GIT_TAG        v1.18.10
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(fltk lv2)

# Expose the fetched LV2 headers as an interface target for the plugin.
add_library(lv2_headers INTERFACE)
target_include_directories(lv2_headers INTERFACE ${lv2_SOURCE_DIR}/include)

# ---- System libraries (host-provided; not vendored) ----
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
pkg_check_modules(LO   REQUIRED IMPORTED_TARGET liblo)

add_subdirectory(src)
add_subdirectory(lv2)
