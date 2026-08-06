# Core: every SRC from the old Makefile except main.cpp
add_library(luvie_core STATIC
    appWindow.cpp editor.cpp songEditor/songEditor.cpp patternEditors/basePatternEditor.cpp
    patternEditors/harmonyEditor.cpp grid.cpp songEditor/songGrid.cpp
    patternEditors/harmonyGrid.cpp songEditor/trackLabels.cpp
    patternEditors/noteContextPopup.cpp songEditor/patternInstanceContextPopup.cpp
    modern/modernTabs.cpp modern/cursors.cpp transports/transport.cpp
    transports/simpleTransport.cpp transports/sequencer.cpp transports/jackTransport.cpp
    transports/jackObserver.cpp transports/playhead.cpp songEditor/markerPopup.cpp
    songEditor/markerRuler.cpp songEditor/loopRuler.cpp songEditor/loopRulerContextPopup.cpp
    songEditor/observableSong.cpp songEditor/observablePattern.cpp
    songEditor/patternNames.cpp
    outputs/observableInstrument.cpp loopManager.cpp loopModeController.cpp patternEditors/patternPanel.cpp
    modern/inlineEditDispatch.cpp patternEditors/harmonyLabels.cpp songEditor/trackContextPopup.cpp
    patternEditors/drumGrid.cpp patternEditors/drumPatternEditor.cpp
    patternEditors/pianorollGrid.cpp patternEditors/pianorollEditor.cpp
    loopEditor.cpp loopContextPopup.cpp
    luvieApp.cpp songEditor/timelineIO.cpp modern/overlayWindow.cpp
    outputs/outputsOverlay.cpp paramDotPopup.cpp patternEditors/patternParamGrid.cpp
    outputs/jackPort.cpp
    outputs/rtMidiPort.cpp outputs/debugPort.cpp outputs/portRegistry.cpp jackShim.cpp
    patternEditors/noteAuditioner.cpp
    session/standaloneSession.cpp session/nsmSession.cpp
)

# NsmClient has two implementations of one interface: the real liblo-backed one, and an
# inert stub that reports "no session manager" — the state the app already handles, since
# it is what happens on Linux whenever NSM_URL is unset. Swapping the .cpp rather than
# #ifdef-ing the call sites keeps main.cpp and NsmSession identical everywhere.
# LUVIE_NSM is on by default on every platform; see the top-level CMakeLists.txt.
if(LUVIE_NSM)
    target_sources(luvie_core PRIVATE nsm.cpp)
    # liblo_static, not the system liblo: vendored and statically linked so the binaries
    # carry no liblo.so dependency (see NOTICE on the LGPL-2.1 section 6 obligations
    # that come with that).
    target_link_libraries(luvie_core PUBLIC liblo_static)
    target_compile_definitions(luvie_core PUBLIC LUVIE_HAVE_NSM)
else()
    target_sources(luvie_core PRIVATE nsmStub.cpp)
endif()

target_include_directories(luvie_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/modern
    ${CMAKE_CURRENT_SOURCE_DIR}/transports
    ${CMAKE_CURRENT_SOURCE_DIR}/patternEditors
    ${CMAKE_CURRENT_SOURCE_DIR}/outputs
    ${CMAKE_CURRENT_SOURCE_DIR}/songEditor
)

# PUBLIC so the plugin (which links luvie_core) inherits these too
target_link_libraries(luvie_core PUBLIC
    fltk::fltk
    fltk::images          # Fl_SVG_Image (dotted-note glyphs in the time-sig control)
    jack_headers          # JACK headers only; the library is dlopen'd at runtime (jackShim.cpp)
    ${CMAKE_DL_LIBS}      # dlopen/dlsym for the JACK shim
    rtmidi
    nlohmann_json::nlohmann_json   # header-only; songEditor/timelineIO.cpp
)

# ---- Version header generated from git at build time ----
# A custom target re-runs the script every build so the embedded version stays
# current as commits/tags change; the script itself only rewrites the header
# when the version string differs, so main.cpp isn't needlessly recompiled.
set(LUVIE_VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/luvieVersion.hpp)
add_custom_target(luvie_version ALL
    BYPRODUCTS ${LUVIE_VERSION_HEADER}
    COMMAND ${CMAKE_COMMAND}
            -DSRC_DIR=${CMAKE_SOURCE_DIR}
            -DOUT_FILE=${LUVIE_VERSION_HEADER}
            -P ${CMAKE_SOURCE_DIR}/cmake/GitVersion.cmake
    COMMENT "Generating luvieVersion.hpp from git"
    VERBATIM
)

add_executable(luvie main.cpp)
add_dependencies(luvie luvie_version)
target_include_directories(luvie PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(luvie PRIVATE luvie_core)

# ---- Platform application metadata ----
# Linux gets its identity from luvie.desktop + the hicolor icon theme (installed below).
# macOS and Windows carry it inside the executable instead, so it has to be built in.
if(WIN32)
    # A Windows executable's icon and version metadata come from a compiled resource
    # script. MinGW's windres handles this with no extra plumbing once RC is enabled.
    enable_language(RC)
    # An absolute path with forward slashes: the .rc lands in the build tree, so a
    # relative reference to logo/ would not resolve, and windres accepts '/' happily.
    set(LUVIE_ICO "${CMAKE_SOURCE_DIR}/logo/luvie.ico")
    configure_file(luvie.rc.in ${CMAKE_CURRENT_BINARY_DIR}/luvie.rc @ONLY)
    target_sources(luvie PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/luvie.rc)
elseif(APPLE)
    # Build Luvie.app rather than a bare CLI binary, so it is double-clickable, shows the
    # icon in the Dock and Finder, and is signable/notarizable as a unit.
    set_target_properties(luvie PROPERTIES
        MACOSX_BUNDLE           TRUE
        OUTPUT_NAME             "Luvie"
        MACOSX_BUNDLE_INFO_PLIST         "${CMAKE_SOURCE_DIR}/cmake/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME        "Luvie"
        MACOSX_BUNDLE_GUI_IDENTIFIER     "com.benbriedis.luvie"
        # Finder and Gatekeeper want a plain X.Y.Z here, not the full git description.
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${LUVIE_VERSION_NUMERIC}"
        MACOSX_BUNDLE_BUNDLE_VERSION       "${LUVIE_VERSION_NUMERIC}"
        MACOSX_BUNDLE_ICON_FILE            "luvie")
    # Listing the .icns as a source with MACOSX_PACKAGE_LOCATION is what copies it into
    # Contents/Resources; CFBundleIconFile in the plist then names it.
    set(LUVIE_ICNS ${CMAKE_SOURCE_DIR}/logo/luvie.icns)
    set_source_files_properties(${LUVIE_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(luvie PRIVATE ${LUVIE_ICNS})
endif()

# ---- Install (component: Standalone) -----------------------------------------
# Was the src/Makefile `install` target. Use a user-local prefix with:
#   cmake --install build --prefix ~/.local
# Everything here is tagged COMPONENT Standalone so CPack can produce an app archive
# separate from the plugin one (see cmake/Packaging.cmake); a plain `cmake --install`
# with no --component still installs all of it, exactly as before.
#
# BUNDLE DESTINATION only applies on macOS, where the target is a Luvie.app directory
# rather than a file; RUNTIME covers Linux and Windows.
install(TARGETS luvie
        BUNDLE  DESTINATION .                          COMPONENT Standalone
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}    COMPONENT Standalone)

# Apache-2.0 section 4a/4d: a redistributed copy must carry the licence and the
# NOTICE file. LUVIE_LICENSE_DIR (see cmake/GatherLicenses.cmake) holds both, plus
# the licences of the libraries compiled into the binary.
install(DIRECTORY ${LUVIE_LICENSE_DIR} DESTINATION ${CMAKE_INSTALL_DOCDIR}
        COMPONENT Standalone)

# Desktop integration below is XDG — Linux-only by construction. macOS carries the
# same information in Luvie.app's Info.plist and .icns, Windows in the executable's
# resource block; both are attached to the target above.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES ${CMAKE_SOURCE_DIR}/luvie.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
            COMPONENT Standalone)
    install(FILES ${CMAKE_SOURCE_DIR}/logo/logo.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME luvie.svg
            COMPONENT Standalone)
    # Raster icons at standard sizes. App lists / launchers request a fixed small
    # size and don't always fall back to the scalable SVG, so ship size-specific
    # PNGs too (this is what makes the icon show in RaySession's application list).
    foreach(sz 16 24 32 48 64 128 256)
        install(FILES ${CMAKE_SOURCE_DIR}/logo/png/luvie-${sz}.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${sz}x${sz}/apps
                RENAME luvie.png
                COMPONENT Standalone)
    endforeach()
    # Also install to the legacy pixmaps dir. Some apps (e.g. RaySession) look up
    # app icons here directly, bypassing the icon theme — which matters because Qt
    # apps under non-Qt desktops often have no icon theme set, so QIcon.fromTheme
    # returns nothing.
    install(FILES ${CMAKE_SOURCE_DIR}/logo/png/luvie-256.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
            RENAME luvie.png
            COMPONENT Standalone)
    install(FILES ${CMAKE_SOURCE_DIR}/logo/logo.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
            RENAME luvie.svg
            COMPONENT Standalone)
endif()

# ---- Smoke test ---------------------------------------------------------------
# The only automated check there is: build the whole app, show its window, then quit
# through the normal teardown path. That exercises startup wiring and the destruction
# order main.cpp warns about above, and is what CI runs on all three platforms
# (under xvfb-run on Linux). --test keeps it off JACK and routes MIDI to the console.
add_test(NAME smoke COMMAND luvie --test --exit-after 5)
set_tests_properties(smoke PROPERTIES TIMEOUT 120)
