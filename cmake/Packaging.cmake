# Binary packaging via CPack.
#
# One package per platform, holding both the standalone application and the LV2 plugin
# bundle. These were once split into a package each, per component; that only asked people
# to choose between two downloads that are useful together, weigh almost nothing, and are
# built from the same tree anyway.
#
# The install() rules keep their COMPONENT tags (see src/CMakeLists.txt and src/lv2/):
# tools/make-macos-dmg.sh still selects one at a time with
# `cmake --install --component`. CPack simply does not split on them — with no
# CPACK_<GEN>_COMPONENT_INSTALL set, every generator below produces one monolithic package
# containing everything (bar install rules marked EXCLUDE_FROM_ALL).
#
# Usage, from a configured build tree:
#     cpack --config build/CPackConfig.cmake            # every generator listed below
#     cpack --config build/CPackConfig.cmake -G DEB     # just one
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
    # Nothing for CPack to do. Windows ships exactly one artifact, the Inno Setup
    # installer built by tools/make-windows-installer.sh, so that it is the one file
    # SignPath signs — a second, unsigned download beside a signed one only invites people
    # to take the one with no publisher on it.
    #
    # A .zip was published alongside it up to v0.0.6, on the reasoning that browsers block
    # unsigned .exe downloads more readily than .zip ones. Signing is what actually
    # answers that, and it removes the reason to hand anyone a directory tree and a page
    # of instructions for placing luvie.lv2 by hand.
    #
    # Returning before include(CPack) is deliberate and cannot be replaced by clearing
    # CPACK_GENERATOR: CPack.cmake fills an empty generator list with its own per-platform
    # defaults, which on Windows is exactly the ZIP being removed here. No
    # CPackConfig.cmake is written, so `cpack` in a Windows build tree now says it cannot
    # find one, rather than quietly producing an archive nobody ships.
    message(STATUS "Windows: no CPack package; run tools/make-windows-installer.sh")
    return()
elseif(APPLE)
    # The release artifact is a .dmg and is *not* built by CPack: a .app has to be
    # code-signed after staging and before archiving, and stripping (below) would
    # invalidate the signature — so tools/make-macos-dmg.sh stages, signs and archives in
    # the right order instead. ZIP here is only a sane fallback for a developer running
    # cpack in this tree directly; nothing ships it.
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_STRIP_FILES FALSE)
    # A universal build has two architectures; say so rather than naming one of them.
    if(CMAKE_OSX_ARCHITECTURES MATCHES ";")
        set(LUVIE_PLATFORM_TAG "macos-universal")
    else()
        set(LUVIE_PLATFORM_TAG "macos-${CMAKE_SYSTEM_PROCESSOR}")
    endif()
else()
    # Packages only — no TGZ. A generic Linux tarball was published until v0.0.4 and
    # dropped: it installed nothing, owned nothing and could not be upgraded or removed,
    # so every question it answered ("where does the plugin go?", "why does the icon not
    # appear?") was one the .deb and .rpm answer by construction. Anyone those two do not
    # cover is better served by the AUR recipe or by building from source, which BUILD
    # documents and which is no harder than unpacking an archive to the right places.
    set(CPACK_GENERATOR "DEB")
    # RPM only if rpmbuild is installed: the generator shells out to it, and cpack fails
    # outright when it is missing. Debian's `rpm` package provides it, so release CI gets
    # a .rpm off the same Ubuntu runner as the .deb, while a developer without it still
    # gets the .deb from a plain `cpack`.
    find_program(RPMBUILD_EXECUTABLE rpmbuild)
    if(RPMBUILD_EXECUTABLE)
        list(APPEND CPACK_GENERATOR "RPM")
    else()
        message(STATUS "rpmbuild not found - cpack will not produce a .rpm")
    endif()
    set(LUVIE_PLATFORM_TAG "linux-${CMAKE_SYSTEM_PROCESSOR}")
endif()

# CPACK_PACKAGE_FILE_NAME, not CPACK_ARCHIVE_FILE_NAME: the archive generator only reads
# the latter when it is packaging per component, and these packages are monolithic. The
# DEB and RPM generators ignore this and use their distributions' own naming conventions
# (DEB-DEFAULT / RPM-DEFAULT, set below) — so with the Linux tarball and the Windows zip
# both gone, this names the macOS developer fallback zip and nothing else. It is still set
# unconditionally because CPack falls back to it for the staging directory name whatever
# the generator.
set(CPACK_PACKAGE_FILE_NAME "luvie-${LUVIE_VERSION}-${LUVIE_PLATFORM_TAG}")

# The SPDX identifier, shared by both Linux package formats and matching project_license
# in packaging/com.benbriedis.luvie.metainfo.xml.
set(LUVIE_LICENSE_SPDX "Apache-2.0")

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
# cmake/DebLicenseField.cmake for the detail. (RPM needs none of this: License is a
# first-class spec field, set in the RPM section below.)
set(CPACK_LUVIE_DEB_LICENSE "${LUVIE_LICENSE_SPDX}")
set(CPACK_POST_BUILD_SCRIPTS "${CMAKE_SOURCE_DIR}/cmake/DebLicenseField.cmake")

# ---- RPM package --------------------------------------------------------------------
# The Fedora/openSUSE counterpart of the .deb above, and built on the same Ubuntu runner:
# rpmbuild does not care what distribution it runs on, and the dependencies it derives
# below are soname-based, so they resolve on the target rather than the build host.
set(CPACK_RPM_FILE_NAME       RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "${LUVIE_LICENSE_SPDX}")
set(CPACK_RPM_PACKAGE_GROUP   "Applications/Multimedia")
# Same reasoning as CPACK_DEBIAN_PACKAGE_DEPENDS: these two own the file triggers that
# rebuild the icon and desktop caches, so the entry and icons this package installs are
# picked up without a scriptlet of our own. Fedora and openSUSE both use these names.
set(CPACK_RPM_PACKAGE_REQUIRES "hicolor-icon-theme, desktop-file-utils")
# No Obsoletes for luvie-lv2, unlike the .deb: that package only ever existed as a .deb,
# so there is no RPM-side upgrade path to smooth over.
#
# Nor is there a Suggests for JACK to match CPACK_DEBIAN_PACKAGE_SUGGESTS. CPackRPM
# decides whether rpmbuild understands weak dependencies by running `${RPM_EXECUTABLE}
# --suggests` — a variable it never sets (checked against CMake 3.28) — so the probe
# fails, the tag is dropped and an author warning is printed however new the rpmbuild is.
# RPM's Suggests is advisory in any case: dnf acts on Recommends, not Suggests.

# Without a description of its own the package gets CPack's "This is an installer created
# using CPack" boilerplate, which is what `dnf info luvie` would then show. Abridged from
# packaging/com.benbriedis.luvie.metainfo.xml, the same text software centres display.
set(CPACK_RPM_PACKAGE_DESCRIPTION
"Luvie is a MIDI sequencer built around patterns: short musical phrases you edit
once and then arrange along a song timeline. It ships both as a standalone JACK
application and as an LV2 plugin, so the same editor can drive a rack of synths
on its own or sit inside a DAW.")

# Not relocatable. CPack makes every RPM relocatable by default, which puts a
# `Prefix: /usr` in the header and invites `rpm --relocate`; the desktop entry, icon
# lookups and LV2 path all assume the paths they were built with, so moving the payload
# would half-work at best. Distribution packages are not relocatable either.
#
# Both variables, because CPackRPM decides with
#     if(CPACK_PACKAGE_RELOCATABLE OR CPACK_RPM_PACKAGE_RELOCATABLE)
# — the generic one defaults to true, so on its own the RPM-specific one cannot turn
# this off. Clearing the generic one costs nothing here: the only other thing that reads
# it is CPack's productbuild/PackageMaker plist, and the macOS artifact is a disk image
# built by tools/make-macos-dmg.sh.
set(CPACK_PACKAGE_RELOCATABLE     OFF)
set(CPACK_RPM_PACKAGE_RELOCATABLE OFF)

# xz payload: the default gzip makes the .rpm roughly twice the size of the equivalent
# .deb for identical contents. Every rpm since 4.8 (2010) reads xz.
set(CPACK_RPM_COMPRESSION_TYPE "xz")

# Suppress the /usr/lib/.build-id/** symlink farm rpmbuild adds to any package containing
# ELF binaries. Those links belong to a debuginfo package, which this is not (CPack
# disables debuginfo generation by default), so here they are just files in a shared
# directory no ordinary package should be touching. There is no CPack variable for it;
# CPACK_RPM_SPEC_MORE_DEFINE is the supported way to inject spec preamble lines.
set(CPACK_RPM_SPEC_MORE_DEFINE "%define _build_id_links none")

# Directories every package on the system shares. RPM, unlike dpkg, records directory
# ownership, and CPack's default file list claims every directory the payload passes
# through — which would have Luvie co-owning the XDG trees that filesystem(1) and
# hicolor-icon-theme are responsible for. Own only what is genuinely ours: /usr/bin/luvie,
# /usr/share/doc/luvie, and /usr/lib64/lv2 (see the note on that path below).
# CPack's built-in exclusion list already covers /usr, /usr/bin, /usr/share, /usr/lib,
# /usr/lib64 and /usr/share/doc; this adds the rest.
set(LUVIE_RPM_DATADIR "/usr/${CMAKE_INSTALL_DATADIR}")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "${LUVIE_RPM_DATADIR}/applications"
    "${LUVIE_RPM_DATADIR}/metainfo"
    "${LUVIE_RPM_DATADIR}/pixmaps"
    "${LUVIE_RPM_DATADIR}/icons"
    "${LUVIE_RPM_DATADIR}/icons/hicolor")
foreach(sz scalable 16x16 24x24 32x32 48x48 64x64 128x128 256x256)
    list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        "${LUVIE_RPM_DATADIR}/icons/hicolor/${sz}"
        "${LUVIE_RPM_DATADIR}/icons/hicolor/${sz}/apps")
endforeach()

# A note on the LV2 path. The other two Linux packages put the bundle in /usr/lib/lv2 —
# the system location the LV2 specification names, and what an upstream-default lilv
# searches, so a downloaded Ardour or Carla build finds it whatever the distribution. The
# .rpm cannot follow them: Fedora and openSUSE build lilv with
#     ~/.lv2:/usr/local/lib64/lv2:/usr/lib64/lv2
# compiled in, and that list has no /usr/lib/lv2 entry, so a bundle installed there is
# found by nothing. (This block used to claim the opposite, on the strength of it being
# the specification's path; the install test added in .github/workflows/release.yml
# showed `lv2ls` listing nothing on a Fedora container, which is what settled it.)
#
# So the RPM alone follows %{_libdir}, and does it by moving the staged files rather than
# by configuring a second build: one `cpack` run serves all three generators from one
# configured project, but each generator stages separately, so the rename touches only
# this package. See cmake/RelocateLv2ForRpm.cmake.
#
# /usr/lib64 itself is already in CPackRPM's built-in exclusion list, so the package owns
# /usr/lib64/lv2 downwards and does not claim the directory above it.
set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_SOURCE_DIR}/cmake/RelocateLv2ForRpm.cmake")

# ---- Guard: packaging layout -------------------------------------------------------
# Checked when cpack runs rather than at configure time, because the value it rejects is
# the correct default for a development build tree. See cmake/CheckPackagingLayout.cmake
# for what goes wrong. Forwarded through a CPACK_-prefixed variable because that is the
# only kind CPack copies into CPackConfig.cmake for the script to read.
set(CPACK_LUVIE_LV2_INSTALL_DIR "${LV2_INSTALL_DIR}")
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/cmake/CheckPackagingLayout.cmake")

include(CPack)
