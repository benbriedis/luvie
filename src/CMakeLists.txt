# Core: every SRC from the old Makefile except main.cpp
add_library(luvie_core STATIC
    appWindow.cpp editor.cpp songEditor.cpp patternEditors/basePatternEditor.cpp
    patternEditors/patternEditor.cpp grid.cpp songGrid.cpp
    patternEditors/patternGrid.cpp trackLabels.cpp
    trackControls.cpp noteContextPopup.cpp patternInstanceContextPopup.cpp
    modern/modernTabs.cpp modern/cursors.cpp transports/transport.cpp
    transports/simpleTransport.cpp transports/jackTransport.cpp
    jackObserver.cpp transports/playhead.cpp markerPopup.cpp
    markerRuler.cpp observableSong.cpp observablePattern.cpp
    observableInstrument.cpp activePatternSet.cpp patternPanel.cpp
    inlineEditDispatch.cpp patternEditors/noteLabels.cpp trackContextPopup.cpp
    patternEditors/drumGrid.cpp patternEditors/drumPatternEditor.cpp
    patternEditors/pianorollGrid.cpp patternEditors/pianorollEditor.cpp
    loopEditor.cpp
    luvieApp.cpp nsm.cpp timelineIO.cpp modern/overlayWindow.cpp
    outputsOverlay.cpp paramDotPopup.cpp patternEditors/patternParamGrid.cpp
    jackPort.cpp
    rtMidiPort.cpp debugPort.cpp portRegistry.cpp jackShim.cpp
    patternEditors/noteAuditioner.cpp
)

target_include_directories(luvie_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/modern
    ${CMAKE_CURRENT_SOURCE_DIR}/transports
    ${CMAKE_CURRENT_SOURCE_DIR}/patternEditors
)

# PUBLIC so the plugin (which links luvie_core) inherits these too
target_link_libraries(luvie_core PUBLIC
    fltk::fltk
    jack_headers          # JACK headers only; the library is dlopen'd at runtime (jackShim.cpp)
    ${CMAKE_DL_LIBS}      # dlopen/dlsym for the JACK shim
    rtmidi
    PkgConfig::LO
)

add_executable(luvie main.cpp)
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
