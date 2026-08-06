# Derive Luvie's version from git at *configure* time.
#
# This is the companion to cmake/GitVersion.cmake, which runs at *build* time to
# regenerate luvieVersion.hpp so the string embedded in the binary tracks new commits
# without a reconfigure. That one cannot serve here: package file names and the Windows
# VERSIONINFO resource are fixed when the build files are generated, so they need a value
# available now. Both read the same `git describe`, so they agree for any given configure.
#
# Sets, in the caller's scope:
#   LUVIE_VERSION          full descriptive string, e.g. "v0.0.1-12-gabc1234-dirty"
#   LUVIE_VERSION_NUMERIC  bare MAJOR.MINOR.PATCH, e.g. "0.0.1" (what CPack/VERSIONINFO need)
#   LUVIE_VERSION_COMMAS   MAJOR,MINOR,PATCH,0 — the form a Windows VERSIONINFO block wants

set(LUVIE_VERSION "unknown")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_RESULT)
    if(GIT_RESULT EQUAL 0 AND GIT_DESCRIBE)
        set(LUVIE_VERSION "${GIT_DESCRIBE}")
    endif()
endif()

# Pull an X.Y.Z out of the description if there is a tag to pull it from. A shallow clone
# or a tagless repo yields a bare hash, which has no version in it — fall back to 0.0.0
# rather than guessing, so an unversioned package is obviously unversioned.
if(LUVIE_VERSION MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(LUVIE_VERSION_NUMERIC "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
    set(LUVIE_VERSION_COMMAS  "${CMAKE_MATCH_1},${CMAKE_MATCH_2},${CMAKE_MATCH_3},0")
else()
    set(LUVIE_VERSION_NUMERIC "0.0.0")
    set(LUVIE_VERSION_COMMAS  "0,0,0,0")
endif()

message(STATUS "Luvie version: ${LUVIE_VERSION} (numeric ${LUVIE_VERSION_NUMERIC})")
