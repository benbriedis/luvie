cmake_minimum_required(VERSION 3.28)
project(luvie LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
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

# RtMidi — cross-platform "native" MIDI I/O (ALSA on Linux, CoreMIDI on macOS).
# Built static, ALSA-only: JACK is handled by our own engine, so disable RtMidi's
# JACK backend to avoid a second JACK client. (CMP0077 is NEW at our cmake_minimum,
# so these normal vars override RtMidi's option() defaults.)
set(RTMIDI_BUILD_SHARED_LIBS OFF)
set(RTMIDI_BUILD_TESTING     OFF)
set(RTMIDI_API_JACK          OFF)
# Rename RtMidi's "uninstall" target to avoid a clash with FLTK's same-named one.
set(RTMIDI_TARGETNAME_UNINSTALL "rtmidi_uninstall")
FetchContent_Declare(rtmidi
    GIT_REPOSITORY https://github.com/thestk/rtmidi.git
    GIT_TAG        6.0.0
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(fltk lv2 rtmidi)

# Expose the fetched LV2 headers as an interface target for the plugin.
add_library(lv2_headers INTERFACE)
target_include_directories(lv2_headers INTERFACE ${lv2_SOURCE_DIR}/include)

# ---- System libraries (host-provided; not vendored) ----
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack)
pkg_check_modules(LO   REQUIRED IMPORTED_TARGET liblo)
# ALSA is no longer a direct dependency: the "native" MIDI backend goes through
# RtMidi, which finds and links libasound itself.

add_subdirectory(src)
add_subdirectory(lv2)
