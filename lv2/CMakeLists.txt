set(LV2_BUNDLE "${CMAKE_BINARY_DIR}/luvie.lv2")

# DSP (pure C)
add_library(luvie_dsp MODULE luvie_dsp.c)
target_link_libraries(luvie_dsp PRIVATE m lv2_headers)

# UI (links the shared core; LV2 headers; FLTK/jack come via luvie_core)
add_library(luvie_ui MODULE luvie_ui.cpp)
target_link_libraries(luvie_ui PRIVATE luvie_core lv2_headers)

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
