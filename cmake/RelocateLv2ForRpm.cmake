# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0
#
# Move the LV2 bundle from lib/lv2 to lib64/lv2 in the RPM's staging tree.
#
# Every other package Luvie ships puts the bundle in /usr/lib/lv2, which is the location
# the LV2 specification names and which an upstream-default lilv searches. The RPM
# distributions are the exception: Fedora and openSUSE build lilv with
#
#     ~/.lv2:/usr/local/lib64/lv2:/usr/lib64/lv2
#
# compiled in as the default search path, with no /usr/lib/lv2 entry at all. A bundle
# installed there is invisible to every host on the system while looking perfectly well
# installed -- `lv2ls` lists nothing and jalv reports the plugin as not found -- so the
# .rpm has to follow %{_libdir} even though the other formats do not.
#
# Done by moving files in the staging tree rather than by configuring a second build with
# LV2_INSTALL_DIR=lib64/lv2, because CPack installs every generator from one configured
# project: the .deb and the .rpm are produced by a single `cpack` run over a single build.
# Each generator does get its own staging directory, though, so a rename here changes the
# .rpm and nothing else.
#
# Run as a CPACK_PRE_BUILD_SCRIPTS hook -- after CPack has staged the files, before it
# builds the package from them. (CPACK_INSTALL_SCRIPTS cannot serve: those run *before*
# the project is installed into the staging tree, so there would be nothing to move.)

if(NOT CPACK_GENERATOR STREQUAL "RPM")
    return()
endif()

# Where the payload is staged: the temporary directory plus the prefix the package will
# unpack to, which for RPM is /usr.
set(LUVIE_STAGE "${CPACK_TEMPORARY_DIRECTORY}${CPACK_PACKAGING_INSTALL_PREFIX}")

# cmake/Packaging.cmake forwards LV2_INSTALL_DIR here; the sibling CheckPackagingLayout
# script has already rejected an absolute one. Anything but a lib/... path means the
# packaging build was configured with a layout this rename does not understand, and
# quietly shipping the bundle wherever it happens to be is exactly the failure this
# script exists to prevent -- so stop instead.
if(NOT CPACK_LUVIE_LV2_INSTALL_DIR MATCHES "^lib(/|$)")
    message(FATAL_ERROR
        "Refusing to package: cannot map LV2_INSTALL_DIR to the RPM's %{_libdir}.\n"
        "    LV2_INSTALL_DIR = ${CPACK_LUVIE_LV2_INSTALL_DIR}\n"
        "This script rewrites a leading \"lib\" to \"lib64\", and that path does not "
        "start with one.")
endif()
string(REGEX REPLACE "^lib" "lib64" LUVIE_RPM_LV2_RELDIR "${CPACK_LUVIE_LV2_INSTALL_DIR}")

set(LUVIE_LV2_FROM "${LUVIE_STAGE}/${CPACK_LUVIE_LV2_INSTALL_DIR}")
set(LUVIE_LV2_TO   "${LUVIE_STAGE}/${LUVIE_RPM_LV2_RELDIR}")

if(NOT IS_DIRECTORY "${LUVIE_LV2_FROM}")
    message(FATAL_ERROR
        "Refusing to package: no LV2 bundle at '${LUVIE_LV2_FROM}'.\n"
        "The .rpm would contain the standalone application only, with no sign that the "
        "plugin was ever meant to be there.")
endif()

get_filename_component(LUVIE_LV2_TO_PARENT "${LUVIE_LV2_TO}" DIRECTORY)
file(MAKE_DIRECTORY "${LUVIE_LV2_TO_PARENT}")
file(RENAME "${LUVIE_LV2_FROM}" "${LUVIE_LV2_TO}")

# The plugin is the only thing installed under lib/, so that tree is now an empty
# directory which would otherwise be packaged as one.
file(GLOB LUVIE_LIB_LEFTOVERS "${LUVIE_STAGE}/lib/*")
if(NOT LUVIE_LIB_LEFTOVERS)
    file(REMOVE_RECURSE "${LUVIE_STAGE}/lib")
endif()

message(STATUS "RelocateLv2ForRpm: ${CPACK_PACKAGING_INSTALL_PREFIX}/"
               "${CPACK_LUVIE_LV2_INSTALL_DIR} -> ${CPACK_PACKAGING_INSTALL_PREFIX}/"
               "${LUVIE_RPM_LV2_RELDIR}")
