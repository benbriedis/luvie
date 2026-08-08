# Binary packaging via CPack.
#
# Luvie ships as two independent things — the standalone application and the LV2 plugin
# bundle — and people want one without the other, so every install() rule is tagged with
# a component (see src/CMakeLists.txt and src/lv2/) and CPACK_ARCHIVE_COMPONENT_INSTALL
# turns each one into its own archive. A plain `cmake --install` is unaffected and still
# installs both.
#
# Usage, from a configured build tree:
#     cpack --config build/CPackConfig.cmake            # every generator listed below
#     cpack --config build/CPackConfig.cmake -G TGZ     # just one
#
# Note that the plugin component only lands inside the prefix if LV2_INSTALL_DIR points
# there — its default is the absolute per-user LV2 directory, which is right for a
# developer install but would escape the staging tree. Configure packaging builds with
#     -DLV2_INSTALL_DIR=lib/lv2
# (a relative path, resolved against the packaging prefix).

include(GNUInstallDirs)

set(CPACK_PACKAGE_NAME            "luvie")
set(CPACK_PACKAGE_VENDOR          "Ben Briedis")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Pattern-based MIDI sequencer")
set(CPACK_PACKAGE_HOMEPAGE_URL    "https://github.com/benbriedis/luvie")
set(CPACK_PACKAGE_VERSION         "${LUVIE_VERSION_NUMERIC}")
set(CPACK_RESOURCE_FILE_LICENSE   "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "luvie")
set(CPACK_STRIP_FILES             TRUE)

# Component metadata (shown by the DEB generator and by `cpack --help-component`).
set(CPACK_COMPONENTS_ALL Standalone Plugin PluginLicense PluginArchiveLicense)
set(CPACK_COMPONENT_STANDALONE_DISPLAY_NAME "Luvie standalone application")
set(CPACK_COMPONENT_PLUGIN_DISPLAY_NAME     "Luvie LV2 plugin")
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

# The plug-in's payload and its licences are separate components only so that the
# licences can land in a different place per generator (see src/lv2/CMakeLists.txt);
# they still belong in one package, hence the shared group and ONE_PER_GROUP. Standalone
# is left ungrouped, which under ONE_PER_GROUP still gives it a package of its own.
#
# Every CPACK_*_<NAME>_* variable below keys off the group name for the plug-in and the
# component name for the app -- both spelled the same as before this split.
set(CPACK_COMPONENTS_GROUPING ONE_PER_GROUP)
set(CPACK_COMPONENT_PLUGIN_GROUP               plugin)
set(CPACK_COMPONENT_PLUGINLICENSE_GROUP        plugin)
set(CPACK_COMPONENT_PLUGINARCHIVELICENSE_GROUP plugin)

# CPack reads this once per generator, with CPACK_GENERATOR naming that one generator,
# which is the only point at which the two licence components can be told apart.
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/cmake/CPackGenerator.cmake")

# ---- Per-platform generators and file naming --------------------------------------
# Archive names carry the *full* git description (not just X.Y.Z) plus platform and
# architecture, so a downloaded file is self-identifying and two builds of the same tag
# from different runners can't be confused for each other.
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
    set(LUVIE_PLATFORM_TAG "windows-x86_64")
elseif(APPLE)
    # ZIP is what macOS users expect, and what `ditto` produces. Note the release
    # artifact is *not* built by CPack: a .app has to be code-signed after staging and
    # before archiving, and stripping (below) would invalidate the signature — so
    # tools/make-macos-zip.sh stages, signs and archives in the right order instead.
    # These settings only matter if someone runs cpack here directly.
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_STRIP_FILES FALSE)
    # A universal build has two architectures; say so rather than naming one of them.
    if(CMAKE_OSX_ARCHITECTURES MATCHES ";")
        set(LUVIE_PLATFORM_TAG "macos-universal")
    else()
        set(LUVIE_PLATFORM_TAG "macos-${CMAKE_SYSTEM_PROCESSOR}")
    endif()
else()
    set(CPACK_GENERATOR "TGZ;DEB")
    set(LUVIE_PLATFORM_TAG "linux-${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CPACK_ARCHIVE_STANDALONE_FILE_NAME "luvie-${LUVIE_VERSION}-${LUVIE_PLATFORM_TAG}")
set(CPACK_ARCHIVE_PLUGIN_FILE_NAME     "luvie-lv2-${LUVIE_VERSION}-${LUVIE_PLATFORM_TAG}")

# ---- Debian package ----------------------------------------------------------------
# One .deb per component, so `luvie` and `luvie-lv2` can be installed independently.
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ben Briedis <benbriedis@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION    "sound")
# Derive the runtime dependencies by inspecting what the binaries actually link, rather
# than maintaining a hand-written list that drifts as FLTK's backends change. Note this
# will *not* list JACK: it is dlopen'd, never linked, which is exactly the intent — JACK
# stays an optional runtime dependency (see src/jackShim.cpp).
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_STANDALONE_PACKAGE_NAME "luvie")
set(CPACK_DEBIAN_PLUGIN_PACKAGE_NAME     "luvie-lv2")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# jackd is genuinely optional (the app falls back to its internal transport), so it is a
# Suggests, not a Depends.
set(CPACK_DEBIAN_STANDALONE_PACKAGE_SUGGESTS "jackd2 | jackd, pipewire-jack")

include(CPack)
