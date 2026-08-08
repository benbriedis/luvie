# Assemble ${CMAKE_BINARY_DIR}/LICENSES, the licence directory that ships with binary
# distributions: Luvie's own terms at the top, each third-party library's licence under its
# own subdirectory. Texts are copied straight out of the pinned dependency sources rather
# than kept by hand, so they always match the code actually linked.
#
# Runs at configure time, which is when FetchContent has populated *_SOURCE_DIR. Everything
# it needs is already on disk by then — this only copies local files, so it costs a few
# milliseconds and needs no network.
#
# Binary distributions have to carry the licences of the libraries compiled into them:
# RtMidi and nlohmann/json are MIT and the LV2 headers are ISC, all three requiring the
# notice to appear in every copy. liblo is LGPL-2.1+ and statically linked, so its text
# is mandatory rather than merely polite — section 6 requires shipping the licence and a
# prominent notice with any work containing portions of the Library. Not gathered:
#   JACK   — not linked at all; dlopen'd at runtime (see src/jackShim.cpp). Its headers
#            are fetched (jack2) so every platform compiles against the same set, but
#            headers are a build-time input only: no JACK code ends up in our binaries
#            and none is redistributed, so there is nothing here to carry.
# FLTK is gathered even though its exception 4 waives shipping the licence, because that
# text is also our evidence that statically linking it is permitted.

set(LUVIE_LICENSE_DIR "${CMAKE_BINARY_DIR}/LICENSES")

# Start clean, so a dependency that is dropped or renamed cannot leave a stale licence
# behind claiming we still ship it.
file(REMOVE_RECURSE "${LUVIE_LICENSE_DIR}")

file(COPY "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/NOTICE"
     DESTINATION "${LUVIE_LICENSE_DIR}")

# Copy one library's licence file(s) into LICENSES/<name>/. Arguments may be files or
# directories; a directory contributes its immediate contents.
function(luvie_add_library_license name)
    set(dest "${LUVIE_LICENSE_DIR}/${name}")
    foreach(src IN LISTS ARGN)
        if(NOT EXISTS "${src}")
            message(FATAL_ERROR
                "Licence file for ${name} not found:\n    ${src}\n"
                "A dependency version bump has probably moved or renamed it. Correct the "
                "path in cmake/GatherLicenses.cmake — do not simply drop the entry, as "
                "this licence has to ship with binary distributions.")
        endif()

        if(IS_DIRECTORY "${src}")
            file(GLOB entries "${src}/*")
            if(NOT entries)
                message(FATAL_ERROR "Licence directory for ${name} is empty:\n    ${src}")
            endif()
        else()
            set(entries "${src}")
        endif()

        foreach(entry IN LISTS entries)
            get_filename_component(fname "${entry}" NAME)
            # configure_file, not file(COPY): it reads the content through, so a licence
            # that upstream ships as a symlink lands here as a real file. LV2 does exactly
            # that — its LICENSES/ISC.txt is a link to ../COPYING, which file(COPY) would
            # reproduce as a link that dangles the moment it leaves the upstream tree,
            # silently costing us the ISC text that covers every LV2 header we compile.
            # It also registers a configure-time dependency on the source.
            configure_file("${entry}" "${dest}/${fname}" COPYONLY)
        endforeach()
    endforeach()
endfunction()

luvie_add_library_license(FLTK          "${fltk_SOURCE_DIR}/COPYING")
luvie_add_library_license(RtMidi        "${rtmidi_SOURCE_DIR}/LICENSE")
luvie_add_library_license(LV2           "${lv2_SOURCE_DIR}/COPYING"
                                        "${lv2_SOURCE_DIR}/LICENSES")
luvie_add_library_license(nlohmann_json "${nlohmann_json_SOURCE_DIR}/LICENSE.MIT")
# liblo ships the LGPL-2.1 text as COPYING. Statically linked, so this is the copy of the
# License that section 6 obliges us to supply with every binary distribution — but only
# when it is actually in the binary, hence the guard: a -DLUVIE_NSM=OFF build contains no
# liblo, and shipping its licence would misstate what the binary is made of.
if(LUVIE_NSM)
    luvie_add_library_license(liblo     "${liblo_SOURCE_DIR}/COPYING")
endif()

# Re-run configure if Luvie's own terms change, so the gathered copies cannot go stale.
# Editing LICENSE or NOTICE therefore costs one reconfigure — rare enough to be worth the
# guarantee that what ships matches what is in the tree.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/NOTICE")
