# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0
#
# Refuse to build a package whose payload would escape the staging tree.
#
# LV2_INSTALL_DIR (src/lv2/CMakeLists.txt) defaults to the *absolute* per-user plug-in
# directory — $HOME/.lv2 and its per-platform equivalents — which is the right target for
# a developer's `cmake --install` and quietly wrong for packaging. An absolute DESTINATION
# makes CMake ignore CMAKE_INSTALL_PREFIX and honour only DESTDIR, which CPack points at
# its staging directory; the bundle is then archived under the *build machine's* home
# path. dpkg has no notion of prefixes or relocation, so installing such a package writes
# to that literal path as root, on a machine where the user usually does not exist, and
# records it as package-owned — an eventual `apt remove` deletes it again.
#
# The distribution presets get this right (LV2_INSTALL_DIR=lib/lv2 in the "dist" preset,
# which is what release CI configures with), so this guard exists for `cpack` run by hand
# in a development build tree, where the default is absolute by design. That is also why
# the check lives here, in a CPACK_PROJECT_CONFIG_FILE evaluated when cpack runs, rather
# than at configure time: an ordinary `cmake --preset default` *should* keep the absolute
# per-user path, and must not be broken by this.
#
# lintian catches the resulting package too (E: dir-or-file-in-home), but only on Linux,
# only for .deb, and only if it is run — the macOS and Windows archives have the same flaw
# with no equivalent check.

if(NOT CPACK_LUVIE_LV2_INSTALL_DIR)
    return()
endif()

if(IS_ABSOLUTE "${CPACK_LUVIE_LV2_INSTALL_DIR}")
    message(FATAL_ERROR
        "Refusing to package: LV2_INSTALL_DIR is absolute.\n"
        "    LV2_INSTALL_DIR = ${CPACK_LUVIE_LV2_INSTALL_DIR}\n"
        "The LV2 bundle would be archived under that literal path instead of inside the "
        "package prefix, and installing the result would write there as root.\n"
        "Package from a distribution preset, which sets a relative path and its own build "
        "tree:\n"
        "    cmake --preset linux-dist && cmake --build --preset linux-dist\n"
        "    (cd build-dist && cpack)\n"
        "Or override it in this build tree — reconfiguring is enough, there is no need to "
        "clear the cache:\n"
        "    cmake --preset default -DLV2_INSTALL_DIR=lib/lv2\n")
endif()
