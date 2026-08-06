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

# Sets LUVIE_VERSION / LUVIE_VERSION_NUMERIC / LUVIE_VERSION_COMMAS from `git describe`,
# for package file names and the Windows VERSIONINFO resource. (The version *compiled into*
# the binary comes from cmake/GitVersion.cmake, which re-runs every build.)
include(${CMAKE_SOURCE_DIR}/cmake/ProjectVersion.cmake)

# ---- Vendored dependencies: fetched + pinned for reproducibility ----
# EXCLUDE_FROM_ALL on the declarations below is what keeps `cmake --install` from
# emitting the dependencies' own install rules into our prefix. Without it, FLTK and
# RtMidi install their headers, static libs, pkg-config/CMake config files, man pages
# and even a stray fltk-options.desktop alongside Luvie. We consume them as static
# libraries, so nothing of theirs belongs in an install of Luvie. They are still built
# on demand, as our targets link them.
#
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
    EXCLUDE_FROM_ALL
)

# LV2 — headers only (upstream is meson-based; nothing to compile/link).
FetchContent_Declare(lv2
    GIT_REPOSITORY https://github.com/lv2/lv2.git
    GIT_TAG        v1.18.10
    GIT_SHALLOW    TRUE
)

# JACK — headers only, same deal (upstream is a Waf project, so MakeAvailable adds no
# targets). Fetched rather than found via pkg-config so that all three platforms compile
# against the identical header set: macOS and Windows have no libjack-dev equivalent to
# depend on, and the library itself is never linked — it is dlopen'd at runtime by
# src/jackShim.cpp. jack2's headers are self-contained and already handle MinGW/Apple.
FetchContent_Declare(jack_hdrs
    GIT_REPOSITORY https://github.com/jackaudio/jack2.git
    GIT_TAG        v1.9.22
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
    EXCLUDE_FROM_ALL
)

# nlohmann/json — header-only; used for project (JSON) load/save in
# songEditor/timelineIO.cpp. Fetched rather than found on the system so that the
# header we compile against is pinned (find_package(nlohmann_json 3) accepted any
# system 3.x) and so that its LICENSE.MIT is present in the build tree for
# cmake/GatherLicenses.cmake — the Debian package ships no licence file at all,
# only an SPDX line inside json.hpp, and json is compiled into our binaries, so
# its MIT notice has to travel with them.
set(JSON_BuildTests OFF)
set(JSON_Install    OFF)   # we install the licence, not json's headers
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(fltk lv2 rtmidi nlohmann_json jack_hdrs)

# RtMidi's CMakeLists does `target_compile_definitions(rtmidi PRIVATE RTMIDI_EXPORT)`
# unconditionally, without consulting RTMIDI_BUILD_SHARED_LIBS. On Windows that macro
# turns RTMIDI_DLL_PUBLIC into __declspec(dllexport), so every class in this *static*
# archive is marked for export — and those marks survive into anything that links it.
# The LV2 UI module then re-exported RtMidi's entire C++ API (87 symbols), which is
# exactly the interposition hazard src/lv2/CMakeLists.txt works to avoid; and no linker
# flag can undo it, because --exclude-all-symbols suppresses only *auto*-export, not an
# explicit dllexport. So drop the definition: a static build must not claim to export.
# (Harmless on ELF/Mach-O, where RtMidi's header ignores the macro and always asks for
# default visibility — that case is handled by --exclude-libs,ALL instead.)
get_target_property(luvie_rtmidi_defs rtmidi COMPILE_DEFINITIONS)
if(luvie_rtmidi_defs)
    list(REMOVE_ITEM luvie_rtmidi_defs RTMIDI_EXPORT)
    set_target_properties(rtmidi PROPERTIES COMPILE_DEFINITIONS "${luvie_rtmidi_defs}")
endif()

# Expose the fetched LV2 headers as an interface target for the plugin.
add_library(lv2_headers INTERFACE)
target_include_directories(lv2_headers INTERFACE ${lv2_SOURCE_DIR}/include)

# JACK is loaded at runtime via dlopen (see src/jackShim.cpp), NOT linked. Linking
# -ljack would bake a load-time NEEDED dependency into the binary, so it would
# refuse to start on machines without libjack installed. Instead we compile against
# the JACK *headers* only and resolve the library at runtime when present. This
# interface target carries the include dirs without the -ljack link flag.
add_library(jack_headers INTERFACE)
target_include_directories(jack_headers INTERFACE ${jack_hdrs_SOURCE_DIR}/common)
# ALSA is no longer a direct dependency: the "native" MIDI backend goes through
# RtMidi, which finds and links libasound itself.

# ---- System libraries (host-provided; not vendored) ----
# liblo backs the NSM (Non Session Manager) client, which is a Linux session-management
# protocol — there is nothing on macOS or Windows to talk to, so both the library and
# the NSM sources are Linux-only (see src/CMakeLists.txt). Keeping liblo a *system*
# library also keeps it dynamically linked, which is what LGPL-2.1 section 5 relies on;
# see NOTICE and cmake/GatherLicenses.cmake.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LO REQUIRED IMPORTED_TARGET liblo)
    set(LUVIE_NSM ON)
else()
    set(LUVIE_NSM OFF)
endif()

# ---- Licences ----
# Assembles build/LICENSES/ from LICENSE, NOTICE and the fetched dependency sources.
# Defines LUVIE_LICENSE_DIR, which src/ and src/lv2/ install.
include(${CMAKE_SOURCE_DIR}/cmake/GatherLicenses.cmake)

enable_testing()

add_subdirectory(src)
add_subdirectory(src/lv2)

# ---- Binary packaging (CPack) ----
# Must come after the subdirectories, so every install(... COMPONENT ...) rule is
# already registered when CPack collects them.
include(${CMAKE_SOURCE_DIR}/cmake/Packaging.cmake)
