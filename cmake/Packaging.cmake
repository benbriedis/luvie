# Binary packaging via CPack.
#
# One package per platform, holding both the standalone application and the LV2 plugin
# bundle. These were once split into a package each, per component; that only asked people
# to choose between two downloads that are useful together, weigh almost nothing, and are
# built from the same tree anyway.
#
# The install() rules keep their COMPONENT tags (see src/CMakeLists.txt and src/lv2/):
# tools/make-macos-zip.sh still selects one at a time with
# `cmake --install --component`. CPack simply does not split on them — with no
# CPACK_<GEN>_COMPONENT_INSTALL set, every generator below produces one monolithic package
# containing everything (bar install rules marked EXCLUDE_FROM_ALL).
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

# CPACK_PACKAGE_FILE_NAME, not CPACK_ARCHIVE_FILE_NAME: the archive generator only reads
# the latter when it is packaging per component, and these packages are monolithic. The
# DEB generator ignores this and uses DEB-DEFAULT naming (set below).
set(CPACK_PACKAGE_FILE_NAME "luvie-${LUVIE_VERSION}-${LUVIE_PLATFORM_TAG}")

# ---- Debian package ----------------------------------------------------------------
# A single `luvie` package carrying both halves.
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ben Briedis <benbriedis@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION    "sound")
# Up to v0.0.2 the plug-in shipped as a separate `luvie-lv2` package owning the files
# this one now installs, so declare both fields: Conflicts alone would just make the two
# uninstallable together, and Replaces alone would still let dpkg abort on the shared
# paths. Together they let `luvie` cleanly supersede the old package on upgrade.
set(CPACK_DEBIAN_PACKAGE_REPLACES   "luvie-lv2")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS  "luvie-lv2")
# Derive the runtime dependencies by inspecting what the binaries actually link, rather
# than maintaining a hand-written list that drifts as FLTK's backends change. Note this
# will *not* list JACK: it is dlopen'd, never linked, which is exactly the intent — JACK
# stays an optional runtime dependency (see src/jackShim.cpp).
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# The desktop entry and hicolor icons this package installs are only picked up once the
# icon and desktop caches are rebuilt. That happens through dpkg file triggers on
# /usr/share/icons/hicolor and /usr/share/applications, owned by the two packages below —
# which is why debhelper emits no maintainer script for hicolor and neither do we.
# Depending on them guarantees the triggers exist on the target system; shlibdeps above
# contributes the library dependencies, and CPack merges the two lists.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "hicolor-icon-theme, desktop-file-utils")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# jackd is genuinely optional (the app falls back to its internal transport), so it is a
# Suggests, not a Depends.
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "jackd2 | jackd, pipewire-jack")

# GNOME Software (and so Ubuntu's "double-click the .deb" flow) reads a License field
# straight out of the control file, and shows "Unknown license" without one. CPack
# cannot emit a non-standard field, so it is patched in after the fact — see
# cmake/DebLicenseField.cmake for the detail. The SPDX identifier is what AppStream
# expects, and matches project_license in packaging/com.benbriedis.luvie.metainfo.xml.
set(CPACK_LUVIE_DEB_LICENSE "Apache-2.0")
set(CPACK_POST_BUILD_SCRIPTS "${CMAKE_SOURCE_DIR}/cmake/DebLicenseField.cmake")

# ---- Guard: packaging layout -------------------------------------------------------
# Checked when cpack runs rather than at configure time, because the value it rejects is
# the correct default for a development build tree. See cmake/CheckPackagingLayout.cmake
# for what goes wrong. Forwarded through a CPACK_-prefixed variable because that is the
# only kind CPack copies into CPackConfig.cmake for the script to read.
set(CPACK_LUVIE_LV2_INSTALL_DIR "${LV2_INSTALL_DIR}")
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/cmake/CheckPackagingLayout.cmake")

include(CPack)
