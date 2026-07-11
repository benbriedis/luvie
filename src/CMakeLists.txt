# Core: every SRC from the old Makefile except main.cpp
add_library(luvie_core STATIC
    appWindow.cpp editor.cpp songEditor/songEditor.cpp patternEditors/basePatternEditor.cpp
    patternEditors/patternEditor.cpp grid.cpp songEditor/songGrid.cpp
    patternEditors/patternGrid.cpp songEditor/trackLabels.cpp
    songEditor/trackControls.cpp patternEditors/noteContextPopup.cpp songEditor/patternInstanceContextPopup.cpp
    modern/modernTabs.cpp modern/cursors.cpp transports/transport.cpp
    transports/simpleTransport.cpp transports/sequencer.cpp transports/jackTransport.cpp
    transports/jackObserver.cpp transports/playhead.cpp songEditor/markerPopup.cpp
    songEditor/markerRuler.cpp songEditor/loopRuler.cpp songEditor/loopRulerContextPopup.cpp
    songEditor/observableSong.cpp songEditor/observablePattern.cpp
    songEditor/patternNames.cpp
    outputs/observableInstrument.cpp loopManager.cpp loopModeController.cpp patternEditors/patternPanel.cpp
    modern/inlineEditDispatch.cpp patternEditors/noteLabels.cpp songEditor/trackContextPopup.cpp
    patternEditors/drumGrid.cpp patternEditors/drumPatternEditor.cpp
    patternEditors/pianorollGrid.cpp patternEditors/pianorollEditor.cpp
    loopEditor.cpp loopContextPopup.cpp
    luvieApp.cpp nsm.cpp songEditor/timelineIO.cpp modern/overlayWindow.cpp
    outputs/outputsOverlay.cpp paramDotPopup.cpp patternEditors/patternParamGrid.cpp
    outputs/jackPort.cpp
    outputs/rtMidiPort.cpp outputs/debugPort.cpp outputs/portRegistry.cpp jackShim.cpp
    patternEditors/noteAuditioner.cpp
    session/standaloneSession.cpp session/nsmSession.cpp
)

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
    jack_headers          # JACK headers only; the library is dlopen'd at runtime (jackShim.cpp)
    ${CMAKE_DL_LIBS}      # dlopen/dlsym for the JACK shim
    rtmidi
    PkgConfig::LO
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

# Install the standalone app + desktop integration (was the src/Makefile
# `install` target). Use a user-local prefix with: cmake --install build --prefix ~/.local
install(TARGETS luvie RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_SOURCE_DIR}/luvie.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${CMAKE_SOURCE_DIR}/logo/logo.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME luvie.svg)
# Raster icons at standard sizes. App lists / launchers request a fixed small
# size and don't always fall back to the scalable SVG, so ship size-specific
# PNGs too (this is what makes the icon show in RaySession's application list).
foreach(sz 16 24 32 48 64 128 256)
    install(FILES ${CMAKE_SOURCE_DIR}/logo/png/luvie-${sz}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${sz}x${sz}/apps
            RENAME luvie.png)
endforeach()
# Also install to the legacy pixmaps dir. Some apps (e.g. RaySession) look up
# app icons here directly, bypassing the icon theme — which matters because Qt
# apps under non-Qt desktops often have no icon theme set, so QIcon.fromTheme
# returns nothing.
install(FILES ${CMAKE_SOURCE_DIR}/logo/png/luvie-256.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
        RENAME luvie.png)
install(FILES ${CMAKE_SOURCE_DIR}/logo/logo.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
        RENAME luvie.svg)
