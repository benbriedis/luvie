# Assert that an LV2 module exports exactly one symbol: its entry point.
#
# Run as a POST_BUILD script (see the end of src/lv2/CMakeLists.txt). The export-control
# flags this checks are easy to get silently wrong -- MinGW's ld accepts ELF-only flags
# like --version-script and ignores them, which once left luvie_ui.dll exporting all of
# RtMidi's C++ symbols while the build reported success. An over-exporting plugin links,
# loads and runs normally right up until the host interposes one of those symbols against
# its own copy, which is a crash in someone else's process and nearly unattributable. So
# the invariant is checked mechanically on every build.
#
# Expected -D variables:
#   MODULE    the built .so / .dylib / .dll
#   EXPECTED  the entry point it is allowed to export
#   FORMAT    PE | MACHO | ELF -- passed in, not inferred: in script mode CMake's WIN32
#             describes the *host*, so a cross-build would pick the wrong reader
#   TOOL      the nm/objdump belonging to the target toolchain (CMAKE_NM/CMAKE_OBJDUMP),
#             which for a cross-build is not the one on PATH

if(NOT EXISTS "${MODULE}")
    message(FATAL_ERROR "CheckExports: no such module: ${MODULE}")
endif()

get_filename_component(name "${MODULE}" NAME)

if(NOT TOOL OR NOT EXISTS "${TOOL}")
    # Not fatal: this is a safety net, and a toolchain that ships no binutils should not
    # break the build. The flags themselves are still applied.
    message(STATUS "CheckExports: no symbol reader for ${name}; skipping")
    return()
endif()

set(symbols "")

if(FORMAT STREQUAL "PE")
    # objdump -p prints an "[Ordinal/Name Pointer] Table" listing the export table.
    execute_process(COMMAND "${TOOL}" -p "${MODULE}"
                    OUTPUT_VARIABLE out ERROR_QUIET RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(STATUS "CheckExports: objdump failed on ${name}; skipping")
        return()
    endif()
    # Scope the parse to the name table. objdump also prints an "Export Address Table"
    # whose rows have a confusingly similar "[   1] 5330 Export RVA" shape, so matching
    # bracketed rows across the whole output counts addresses as if they were symbols.
    string(REGEX REPLACE "\r?\n" ";" lines "${out}")
    set(inTable FALSE)
    foreach(line IN LISTS lines)
        if(line MATCHES "\\[Ordinal/Name Pointer\\] Table")
            set(inTable TRUE)
        elseif(inTable)
            # Rows look like:  	[   0] lv2ui_descriptor
            if(line MATCHES "^[ \t]*\\[[ 0-9]+\\][ \t]+(.+)$")
                set(sym "${CMAKE_MATCH_1}")
                string(STRIP "${sym}" sym)
                list(APPEND symbols "${sym}")
            else()
                break()   # blank line or a new section ends the table
            endif()
        endif()
    endforeach()
else()
    # ELF: nm -D --defined-only. Mach-O: nm -gU (global, defined).
    if(FORMAT STREQUAL "MACHO")
        execute_process(COMMAND "${TOOL}" -gU "${MODULE}"
                        OUTPUT_VARIABLE out ERROR_QUIET RESULT_VARIABLE rc)
    else()
        execute_process(COMMAND "${TOOL}" -D --defined-only "${MODULE}"
                        OUTPUT_VARIABLE out ERROR_QUIET RESULT_VARIABLE rc)
    endif()
    if(NOT rc EQUAL 0)
        message(STATUS "CheckExports: nm failed on ${name}; skipping")
        return()
    endif()
    # Lines look like:  0000000000008da9 T lv2_descriptor
    string(REGEX REPLACE "\r?\n" ";" lines "${out}")
    foreach(line IN LISTS lines)
        if(line MATCHES "^[0-9a-fA-F]+ +[A-Za-z] +(.+)$")
            set(sym "${CMAKE_MATCH_1}")
            string(STRIP "${sym}" sym)
            # Mach-O prefixes C symbols with an underscore.
            string(REGEX REPLACE "^_" "" sym "${sym}")
            list(APPEND symbols "${sym}")
        endif()
    endforeach()
endif()

if(NOT symbols)
    message(STATUS "CheckExports: ${name} lists no exports; skipping")
    return()
endif()

list(REMOVE_DUPLICATES symbols)
list(REMOVE_ITEM symbols "${EXPECTED}")

if(symbols)
    list(LENGTH symbols n)
    string(REPLACE ";" "\n    " pretty "${symbols}")
    message(FATAL_ERROR
        "${name} exports ${n} symbol(s) beyond ${EXPECTED}:\n    ${pretty}\n"
        "The plugin must export only its LV2 entry point, or it risks being interposed "
        "against the host's own copies of FLTK/libstdc++/RtMidi. Check the export-control "
        "flags for this binary format in src/lv2/CMakeLists.txt.")
endif()

message(STATUS "CheckExports: ${name} exports only ${EXPECTED}")
