set(LV2_BUNDLE "${CMAKE_BINARY_DIR}/luvie.lv2")

# DSP (pure C)
add_library(luvie_dsp MODULE luvie_dsp.c)
target_link_libraries(luvie_dsp PRIVATE m lv2_headers)

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
target_compile_options(luvie_dsp PRIVATE -fvisibility=hidden)
target_compile_options(luvie_ui  PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
set(LUVIE_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/luvie.version")
target_link_options(luvie_dsp PRIVATE
    -Wl,--exclude-libs,ALL -Wl,--version-script=${LUVIE_VERSION_SCRIPT})
target_link_options(luvie_ui PRIVATE
    -Wl,--exclude-libs,ALL -Wl,--version-script=${LUVIE_VERSION_SCRIPT})
# Relink the modules if the version script changes.
set_target_properties(luvie_dsp luvie_ui PROPERTIES
    LINK_DEPENDS "${LUVIE_VERSION_SCRIPT}")

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
