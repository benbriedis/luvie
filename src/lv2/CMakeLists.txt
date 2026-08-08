set(LV2_BUNDLE "${CMAKE_BINARY_DIR}/luvie.lv2")
set(SRC "${CMAKE_SOURCE_DIR}/src")

find_package(Threads REQUIRED)

# DSP: owns the real-time MIDI engine, so it compiles the FLTK-free engine sources
# directly (NOT via luvie_core, which drags in FLTK). As an LV2 plugin it emits MIDI
# through its atom output port (no JACK), so it compiles only the backend-agnostic
# Sequencer core — not jackTransport / the JACK shim. nlohmann/json comes in as a
# header-only target. Compiling these into the module (rather than linking an archive) means
# -fvisibility=hidden below governs their symbols.
add_library(luvie_dsp MODULE
    luvie_dsp.cpp
    ${SRC}/transports/sequencer.cpp
    ${SRC}/songEditor/observableSong.cpp
    ${SRC}/songEditor/patternNames.cpp
    ${SRC}/songEditor/timelineIO.cpp
    ${SRC}/loopManager.cpp)
target_include_directories(luvie_dsp PRIVATE
    ${SRC} ${SRC}/transports ${SRC}/patternEditors ${SRC}/outputs ${SRC}/songEditor)
target_link_libraries(luvie_dsp PRIVATE
    # libm is a separate library only on Linux; on macOS and Windows the maths
    # functions live in the C runtime and there is no -lm to link.
    $<$<PLATFORM_ID:Linux>:m>
    lv2_headers Threads::Threads ${CMAKE_DL_LIBS}
    nlohmann_json::nlohmann_json)

# UI (links the shared core; LV2 headers; FLTK/jack come via luvie_core)
add_library(luvie_ui MODULE luvie_ui.cpp)
target_link_libraries(luvie_ui PRIVATE luvie_core lv2_headers)

# The one symbol each module is allowed to export. Used twice below: to build the
# export-control link flags, and to assert the finished export table.
set(luvie_dsp_ENTRY lv2_descriptor)
set(luvie_ui_ENTRY  lv2ui_descriptor)

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
# Each of the three binary formats needs a different mechanism, and the choice is made
# on the *format*, not on whether a flag is accepted. That distinction is load-bearing:
# MinGW's ld accepts -Wl,--version-script and -Wl,--exclude-libs without complaint (so
# check_linker_flag reports them as supported) but both are ELF concepts and neither has
# any effect on a PE export table. Selecting by flag-probe alone therefore silently
# produced a Windows DLL exporting all 87 of RtMidi's C++ symbols while reporting
# success. Probe each flag still, but only the one that belongs to this format — except
# on Mach-O, where the probe itself gives a false negative (see the APPLE branch).
include(CheckCompilerFlag)
include(CheckLinkerFlag)

set(LUVIE_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/luvie.version")

# -fvisibility=hidden (and the C++ inlines variant) — GCC/Clang; not an MSVC concept.
# Meaningless on PE (which has no visibility attribute) but harmless there.
check_compiler_flag(CXX -fvisibility=hidden          LUVIE_HAVE_FVISIBILITY)
check_compiler_flag(CXX -fvisibility-inlines-hidden  LUVIE_HAVE_FVISIBILITY_INLINES)

if(LUVIE_HAVE_FVISIBILITY)
    target_compile_options(luvie_dsp PRIVATE -fvisibility=hidden)
    target_compile_options(luvie_ui  PRIVATE -fvisibility=hidden)
endif()
if(LUVIE_HAVE_FVISIBILITY_INLINES)
    target_compile_options(luvie_ui PRIVATE -fvisibility-inlines-hidden)
endif()

set(LUVIE_EXPORTS_CONTROLLED FALSE)

if(WIN32)
    # PE/COFF. There is no version script; instead ld auto-exports symbols, and a
    # module that links static archives (luvie_ui pulls in luvie_core, FLTK and RtMidi)
    # ends up advertising their internals. --exclude-all-symbols turns auto-export off
    # so the export table holds exactly what LV2_SYMBOL_EXPORT marked with
    # __declspec(dllexport) — i.e. the one entry point each module defines.
    check_linker_flag(CXX "-Wl,--exclude-all-symbols" LUVIE_HAVE_EXCLUDE_ALL_SYMBOLS)
    if(LUVIE_HAVE_EXCLUDE_ALL_SYMBOLS)
        target_link_options(luvie_dsp PRIVATE -Wl,--exclude-all-symbols)
        target_link_options(luvie_ui  PRIVATE -Wl,--exclude-all-symbols)
        set(LUVIE_EXPORTS_CONTROLLED TRUE)
    endif()

elseif(APPLE)
    # Mach-O. ld64 has neither --version-script nor --exclude-libs, and needs neither:
    # -exported_symbol is absolute — everything not named becomes private extern,
    # including symbols pulled from the static archives, so there is no --exclude-libs
    # equivalent to add. The name carries Mach-O's leading underscore.
    #
    # Applied without a check_linker_flag probe, deliberately. The probe links a test
    # *executable*, and restricting its exports to a symbol it does not define makes ld
    # diagnose the link; the check then reports "unsupported" for a flag ld64 has always
    # supported, silently dropping it and leaving luvie_ui.so exporting all 4222 of
    # FLTK's and RtMidi's symbols. Apple's clang/ld64 is the only toolchain that builds
    # this branch, the flag is its documented mechanism, and the POST_BUILD export check
    # below is what actually proves the flag took effect.
    foreach(mod luvie_dsp luvie_ui)
        target_link_options(${mod} PRIVATE "-Wl,-exported_symbol,_${${mod}_ENTRY}")
    endforeach()
    set(LUVIE_EXPORTS_CONTROLLED TRUE)

else()
    # ELF. Two complementary flags:
    #   --exclude-libs,ALL   localizes every symbol pulled from a static archive
    #                        (luvie_core, FLTK, RtMidi, static libstdc++);
    #   --version-script     pins the dynamic symbol table to exactly the entry points,
    #                        also catching stray std:: template instantiations emitted
    #                        into the module's own objects.
    check_linker_flag(CXX "-Wl,--exclude-libs,ALL"                       LUVIE_HAVE_EXCLUDE_LIBS)
    check_linker_flag(CXX "-Wl,--version-script=${LUVIE_VERSION_SCRIPT}" LUVIE_HAVE_VERSION_SCRIPT)
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
        set(LUVIE_EXPORTS_CONTROLLED TRUE)
    endif()
endif()

if(NOT LUVIE_EXPORTS_CONTROLLED)
    message(WARNING
        "LV2 plugin symbol-hiding is incomplete on this toolchain: the modules may "
        "export internal FLTK/libstdc++/RtMidi symbols and risk being interposed by "
        "the host. Add the export-control mechanism for this binary format (a .def "
        "file under MSVC) before shipping this build.")
endif()

# The export table is the whole point of the block above, and it fails quietly — a DLL
# or .so with too many exports links, loads and works right up until it interposes a
# host's symbol. So assert on it at build time rather than trusting the flags took.
# The format and the symbol reader are passed in explicitly: a -P script has no notion
# of the target platform (its WIN32 describes the host), and CMAKE_NM/CMAKE_OBJDUMP are
# the target toolchain's, which under a cross-build are not the ones on PATH.
if(WIN32)
    set(LUVIE_EXPORT_FORMAT PE)
    set(LUVIE_EXPORT_TOOL   "${CMAKE_OBJDUMP}")
elseif(APPLE)
    set(LUVIE_EXPORT_FORMAT MACHO)
    set(LUVIE_EXPORT_TOOL   "${CMAKE_NM}")
else()
    set(LUVIE_EXPORT_FORMAT ELF)
    set(LUVIE_EXPORT_TOOL   "${CMAKE_NM}")
endif()

foreach(mod luvie_dsp luvie_ui)
    add_custom_command(TARGET ${mod} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DMODULE=$<TARGET_FILE:${mod}>
                -DEXPECTED=${${mod}_ENTRY}
                -DFORMAT=${LUVIE_EXPORT_FORMAT}
                -DTOOL=${LUVIE_EXPORT_TOOL}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckExports.cmake
        COMMENT "Checking ${mod} export table"
        VERBATIM)
endforeach()

# Match the Makefile output names (luvie_dsp.so, not libluvie_dsp.so)
# and drop both into the .lv2 bundle directory.
set_target_properties(luvie_dsp luvie_ui PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${LV2_BUNDLE}")

# manifest.ttl names the module files by full filename, so it cannot be a static file:
# CMake's module suffix is .so on Linux *and* macOS but .dll on Windows, and a manifest
# pointing at the wrong name makes the plugin simply invisible to every host. Generate it
# from the template instead. (The other two TTLs contain no filenames, so they are copied
# verbatim below.)
set(LUVIE_MODULE_SUFFIX "${CMAKE_SHARED_MODULE_SUFFIX}")
configure_file(manifest.ttl.in ${LV2_BUNDLE}/manifest.ttl @ONLY)

# Data files that make up the bundle alongside the two modules.
set(LV2_BUNDLE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/luvie_dsp.ttl
    ${CMAKE_CURRENT_SOURCE_DIR}/luvie_ui.ttl)

# One OUTPUT-driven copy rule per file, rather than a POST_BUILD hook on a
# module: POST_BUILD only fires when that module relinks, so editing a .ttl or
# the NOTICE on its own would leave a stale copy in the bundle.
set(LV2_BUNDLE_COPIES "")
foreach(f IN LISTS LV2_BUNDLE_FILES)
    get_filename_component(fname "${f}" NAME)
    add_custom_command(
        OUTPUT  ${LV2_BUNDLE}/${fname}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${f} ${LV2_BUNDLE}/${fname}
        DEPENDS ${f}
        COMMENT "Copying ${fname} into luvie.lv2"
        VERBATIM)
    list(APPEND LV2_BUNDLE_COPIES ${LV2_BUNDLE}/${fname})
endforeach()

add_custom_target(luvie_lv2_bundle ALL DEPENDS ${LV2_BUNDLE_COPIES})

# Install the .lv2 bundle (was the root Makefile `install-lv2` target).
#
# The default is the per-user LV2 directory each platform's hosts actually scan, so a
# plain `cmake --install build` drops the plugin somewhere useful. That path is absolute
# and deliberately outside CMAKE_INSTALL_PREFIX — which is right for a developer install
# but wrong for packaging, so cmake/Packaging.cmake and CI override it:
#   cmake -DLV2_INSTALL_DIR=<prefix>/lib/lv2 ...
if(WIN32)
    # $ENV{HOME} is not a thing on Windows; %APPDATA%\LV2 is the convention. Fall back
    # into the prefix if even that is unset (an unusual, service-account-style shell).
    if(DEFINED ENV{APPDATA})
        set(LUVIE_DEFAULT_LV2_DIR "$ENV{APPDATA}/LV2")
    else()
        set(LUVIE_DEFAULT_LV2_DIR "${CMAKE_INSTALL_PREFIX}/lib/lv2")
    endif()
elseif(APPLE)
    set(LUVIE_DEFAULT_LV2_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/LV2")
else()
    set(LUVIE_DEFAULT_LV2_DIR "$ENV{HOME}/.lv2")
endif()
# STRING, not PATH: CMake rewrites a PATH-typed cache entry to an absolute path against
# the current working directory, which would silently turn the relative value packaging
# passes ("lib/lv2", meant to sit under the staging prefix) into an absolute one outside
# it — and the plugin would then be missing from the archive rather than misplaced in it.
set(LV2_INSTALL_DIR "${LUVIE_DEFAULT_LV2_DIR}" CACHE STRING
    "LV2 bundle install location: absolute, or relative to CMAKE_INSTALL_PREFIX")

# MODULE libraries are LIBRARY artifacts on every platform, Windows DLLs included.
install(TARGETS luvie_dsp luvie_ui
        LIBRARY DESTINATION ${LV2_INSTALL_DIR}/luvie.lv2 COMPONENT Plugin)
# The generated manifest, not the template. luvie_dsp.ttl / luvie_ui.ttl are static.
install(FILES ${LV2_BUNDLE}/manifest.ttl luvie_dsp.ttl luvie_ui.ttl
        DESTINATION ${LV2_INSTALL_DIR}/luvie.lv2 COMPONENT Plugin)

# The gathered licences (Apache-2.0 section 4a/4d, plus the terms of every library
# compiled into luvie_ui.so) go *inside* the bundle, and deliberately nowhere else.
#
# luvie.lv2 is the one directory a user copies onto their LV2 path, on its own, away from
# whatever it arrived in — so anything that has to travel with it has to be in it. The
# standalone half of the package carries the same texts under share/doc/luvie (see
# src/CMakeLists.txt); the duplication is a few kilobytes and means neither half depends
# on the other still being there.
install(DIRECTORY ${LUVIE_LICENSE_DIR} DESTINATION ${LV2_INSTALL_DIR}/luvie.lv2
        COMPONENT Plugin)
