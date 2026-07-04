set(LV2_BUNDLE "${CMAKE_BINARY_DIR}/luvie.lv2")
set(SRC "${CMAKE_SOURCE_DIR}/src")

find_package(Threads REQUIRED)

# DSP: owns the real-time MIDI engine, so it compiles the FLTK-free engine sources
# directly (NOT via luvie_core, which drags in FLTK). As an LV2 plugin it emits MIDI
# through its atom output port (no JACK), so it compiles only the backend-agnostic
# Sequencer core — not jackTransport / the JACK shim. nlohmann/json is a system
# header. Compiling these into the module (rather than linking an archive) means
# -fvisibility=hidden below governs their symbols.
add_library(luvie_dsp MODULE
    luvie_dsp.cpp
    ${SRC}/transports/sequencer.cpp
    ${SRC}/songEditor/observableSong.cpp
    ${SRC}/songEditor/patternNames.cpp
    ${SRC}/songEditor/timelineIO.cpp
    ${SRC}/activePatternSet.cpp)
target_include_directories(luvie_dsp PRIVATE
    ${SRC} ${SRC}/transports ${SRC}/patternEditors ${SRC}/outputs ${SRC}/songEditor)
target_link_libraries(luvie_dsp PRIVATE
    m lv2_headers Threads::Threads ${CMAKE_DL_LIBS})

# UI (links the shared core; LV2 headers; FLTK/jack come via luvie_core)
add_library(luvie_ui MODULE luvie_ui.cpp)
target_link_libraries(luvie_ui PRIVATE luvie_core lv2_headers)

# A plugin is dlopen'd into the host's address space, sharing one dynamic-symbol
# namespace. Exporting our copies of FLTK / libstdc++ / RtMidi risks the dynamic
# linker interposing them against the host's (or another plugin's) — binding us to
# an incompatible library and crashing. So export *only* the LV2 entry points
# (lv2_descriptor / lv2ui_descriptor, explicitly marked visibility("default") via
# LV2_SYMBOL_EXPORT) and hide everything else:
#   -fvisibility=hidden        hides this module's own compiled symbols;
#   --exclude-libs,ALL         localizes every symbol pulled from a static archive
#                              (luvie_core, FLTK, RtMidi, static libstdc++).
# The entry points live in the module's own object (not an archive), so they survive.
# A version script then pins the dynamic symbol table to exactly those entry points,
# also catching stray std:: template instantiations emitted into the module itself.
#
# The mechanisms here are toolchain-specific: -fvisibility is a GCC/Clang flag, and
# --exclude-libs / --version-script are GNU binutils-ld / lld features. They cover
# the Linux build. macOS (ld64: -exported_symbols_list) and Windows (MSVC: hidden by
# default, export via __declspec/.def) use different mechanisms — wire those up when
# those ports land. Each flag is feature-detected so an unsupported toolchain simply
# builds without it (with a warning) rather than failing to configure.
include(CheckCompilerFlag)
include(CheckLinkerFlag)

set(LUVIE_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/luvie.version")

# -fvisibility=hidden (and the C++ inlines variant) — GCC/Clang; not an MSVC concept.
check_compiler_flag(CXX -fvisibility=hidden          LUVIE_HAVE_FVISIBILITY)
check_compiler_flag(CXX -fvisibility-inlines-hidden  LUVIE_HAVE_FVISIBILITY_INLINES)
# --exclude-libs,ALL and --version-script — GNU ld / lld only.
check_linker_flag(CXX "-Wl,--exclude-libs,ALL"                         LUVIE_HAVE_EXCLUDE_LIBS)
check_linker_flag(CXX "-Wl,--version-script=${LUVIE_VERSION_SCRIPT}"   LUVIE_HAVE_VERSION_SCRIPT)

if(LUVIE_HAVE_FVISIBILITY)
    target_compile_options(luvie_dsp PRIVATE -fvisibility=hidden)
    target_compile_options(luvie_ui  PRIVATE -fvisibility=hidden)
endif()
if(LUVIE_HAVE_FVISIBILITY_INLINES)
    target_compile_options(luvie_ui PRIVATE -fvisibility-inlines-hidden)
endif()
if(LUVIE_HAVE_EXCLUDE_LIBS)
    target_link_options(luvie_dsp PRIVATE -Wl,--exclude-libs,ALL)
    target_link_options(luvie_ui  PRIVATE -Wl,--exclude-libs,ALL)
endif()
if(LUVIE_HAVE_VERSION_SCRIPT)
    target_link_options(luvie_dsp PRIVATE -Wl,--version-script=${LUVIE_VERSION_SCRIPT})
    target_link_options(luvie_ui  PRIVATE -Wl,--version-script=${LUVIE_VERSION_SCRIPT})
    # Relink the modules if the version script changes.
    set_target_properties(luvie_dsp luvie_ui PROPERTIES
        LINK_DEPENDS "${LUVIE_VERSION_SCRIPT}")
endif()

if(NOT (LUVIE_HAVE_EXCLUDE_LIBS AND LUVIE_HAVE_VERSION_SCRIPT))
    message(WARNING
        "LV2 plugin symbol-hiding is incomplete on this toolchain: the modules may "
        "export internal FLTK/libstdc++/RtMidi symbols and risk being interposed by "
        "the host. Add the platform-specific export-control mechanism (macOS "
        "-exported_symbols_list / Windows .def) before shipping this build.")
endif()

# Match the Makefile output names (luvie_dsp.so, not libluvie_dsp.so)
# and drop both into the .lv2 bundle directory.
set_target_properties(luvie_dsp luvie_ui PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${LV2_BUNDLE}")

# Copy the .ttl manifests into the bundle after build
add_custom_command(TARGET luvie_ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.ttl
        ${CMAKE_CURRENT_SOURCE_DIR}/luvie_dsp.ttl
        ${CMAKE_CURRENT_SOURCE_DIR}/luvie_ui.ttl
        ${LV2_BUNDLE})

# Install the .lv2 bundle (was the root Makefile `install-lv2` target).
# Override location with: cmake -DLV2_INSTALL_DIR=/path ...
set(LV2_INSTALL_DIR "$ENV{HOME}/.lv2" CACHE PATH "LV2 bundle install location")
install(TARGETS luvie_dsp luvie_ui LIBRARY DESTINATION ${LV2_INSTALL_DIR}/luvie.lv2)
install(FILES manifest.ttl luvie_dsp.ttl luvie_ui.ttl
        DESTINATION ${LV2_INSTALL_DIR}/luvie.lv2)
