# Core: every SRC from the old Makefile except main.cpp
add_library(luvie_core STATIC
    appWindow.cpp editor.cpp songEditor.cpp basePatternEditor.cpp
    patternEditor.cpp grid.cpp songGrid.cpp patternGrid.cpp trackLabels.cpp
    trackControls.cpp noteContextPopup.cpp patternInstanceContextPopup.cpp
    modern/modernTabs.cpp modern/cursors.cpp transport.cpp simpleTransport.cpp
    jackTransport.cpp jackObserver.cpp playhead.cpp markerPopup.cpp
    markerRuler.cpp observableSong.cpp observablePattern.cpp
    observableInstrument.cpp activePatternSet.cpp patternPanel.cpp
    inlineEditDispatch.cpp noteLabels.cpp trackContextPopup.cpp drumGrid.cpp
    drumPatternEditor.cpp pianorollGrid.cpp pianorollEditor.cpp loopEditor.cpp
    luvieApp.cpp nsm.cpp timelineIO.cpp modern/overlayWindow.cpp
    outputsOverlay.cpp paramDotPopup.cpp patternParamGrid.cpp jackPort.cpp
    rtMidiPort.cpp debugPort.cpp portRegistry.cpp jackShim.cpp
)

target_include_directories(luvie_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/modern
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
