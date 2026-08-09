# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0
#
# Add a "License:" field to the finished .deb's control file.
#
# Why this is not just another CPACK_DEBIAN_PACKAGE_* variable: License is not a
# field Debian Policy defines for binary packages, so CPack — which writes the
# control file from a fixed list of known fields — has no way to emit it, and no
# pass-through for arbitrary ones (CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA adds *files*
# to the control archive, not fields).
#
# It is nonetheless what GNOME Software reads when you open a .deb: its dpkg plugin
# shells out to
#     dpkg-deb --showformat=${Package}\n${Version}\n${License}\n...
# and, finding the field absent, labels the package "Unknown license". Chrome and
# other third-party debs carry the same de-facto field for this reason. dpkg itself
# ignores unknown fields in a binary control file, so nothing else is affected.
#
# Run as a CPACK_POST_BUILD_SCRIPTS hook, i.e. after the packages exist, with
# CPACK_PACKAGE_FILES listing them.

if(NOT CPACK_PACKAGE_FILES)
    return()
endif()

find_program(DPKG_DEB dpkg-deb)

foreach(pkg IN LISTS CPACK_PACKAGE_FILES)
    if(NOT pkg MATCHES "\\.deb$")
        continue()
    endif()

    if(NOT DPKG_DEB)
        # Only reachable if a .deb was produced by something other than dpkg-deb.
        message(WARNING "DebLicenseField: dpkg-deb not found; '${pkg}' will have no "
                        "License field and software centres will call it unlicensed.")
        continue()
    endif()

    # Unpack, edit, repack. Working on the whole package rather than splicing the
    # control member back in with `ar` keeps dpkg-deb responsible for the archive
    # layout, compression and permissions.
    get_filename_component(pkgName "${pkg}" NAME_WE)
    set(work "${CPACK_TOPLEVEL_DIRECTORY}/_deb_license/${pkgName}")
    file(REMOVE_RECURSE "${work}")
    file(MAKE_DIRECTORY "${work}")

    execute_process(COMMAND "${DPKG_DEB}" --raw-extract "${pkg}" "${work}"
                    RESULT_VARIABLE rc OUTPUT_QUIET)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "DebLicenseField: could not unpack '${pkg}' (${rc}).")
    endif()

    file(READ "${work}/DEBIAN/control" control)
    if(control MATCHES "(^|\n)License:")
        continue()   # already there; nothing to do
    endif()
    # Append rather than insert: field order in a control file is not significant,
    # and the file always ends with the Description field, which is the one field
    # that continues across lines — so appending after it needs no care about
    # landing in the middle of a continuation.
    # Normalise to exactly one trailing newline. Written as strip-then-add because a
    # "\n*$" pattern is allowed to match nothing, which CMake rejects outright.
    string(REGEX REPLACE "[\n]+$" "" control "${control}")
    set(control "${control}\nLicense: ${CPACK_LUVIE_DEB_LICENSE}\n")
    file(WRITE "${work}/DEBIAN/control" "${control}")

    # --root-owner-group because the repack runs as an ordinary user, and without it
    # every file in the package would be installed owned by whoever built it.
    execute_process(COMMAND "${DPKG_DEB}" --root-owner-group --build "${work}" "${pkg}"
                    RESULT_VARIABLE rc ERROR_VARIABLE err OUTPUT_QUIET)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "DebLicenseField: could not repack '${pkg}' (${rc}): ${err}")
    endif()

    file(REMOVE_RECURSE "${work}")
    message(STATUS "DebLicenseField: License: ${CPACK_LUVIE_DEB_LICENSE} -> ${pkg}")
endforeach()
