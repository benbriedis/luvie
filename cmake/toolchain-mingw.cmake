# Cross-compile Luvie for Windows from Linux with mingw-w64.
#
# This is a *checking* toolchain, not the shipping one: releases are built with MSYS2's
# native UCRT64 GCC (see .github/workflows/release.yml and the windows-dist preset). Its
# value is that it lets anyone on Linux compile the Windows build in a couple of minutes,
# instead of finding out from CI whether the platform branches in jackShim.cpp,
# appWindow.cpp, cursors.cpp and stateFile.hpp actually hold together.
#
#     sudo apt install g++-mingw-w64-x86-64
#     cmake -S . -B build-mingw -G Ninja \
#           -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw.cmake \
#           -DCMAKE_BUILD_TYPE=Release
#     cmake --build build-mingw
#
# The result runs under Wine or on Windows, but it is not what gets released: it links
# the MSVCRT C runtime rather than UCRT, so treat it as a compile check.

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# Debian ships two threading models under the same prefix. The "-posix" variants are the
# ones with a working <thread>/<mutex>; the win32 ones leave std::thread undefined, which
# luvie_dsp needs. Prefer them, and fall back to the unsuffixed names on distributions
# that only ship one.
find_program(MINGW_C_COMPILER   NAMES ${TOOLCHAIN_PREFIX}-gcc-posix ${TOOLCHAIN_PREFIX}-gcc   REQUIRED)
find_program(MINGW_CXX_COMPILER NAMES ${TOOLCHAIN_PREFIX}-g++-posix ${TOOLCHAIN_PREFIX}-g++   REQUIRED)
find_program(MINGW_RC_COMPILER  NAMES ${TOOLCHAIN_PREFIX}-windres                             REQUIRED)

set(CMAKE_C_COMPILER   ${MINGW_C_COMPILER})
set(CMAKE_CXX_COMPILER ${MINGW_CXX_COMPILER})
set(CMAKE_RC_COMPILER  ${MINGW_RC_COMPILER})

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
# Look for programs on the host (cmake, git, ninja) but headers and libraries only in the
# target sysroot, so a stray Linux library can never satisfy a Windows link.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg_check_modules does NOT obey CMAKE_FIND_ROOT_PATH — it just runs pkg-config, which
# reads the host's default search path. Left alone, configuring with -DLUVIE_NSM=ON here
# "finds" the host's Linux liblo and feeds -I/usr/include to a Windows compiler, which
# fails deep in glibc's headers with a confusing missing bits/wordsize.h. Point it at the
# target sysroot instead, so a package that genuinely isn't available for Windows is
# reported as missing rather than silently substituted.
set(ENV{PKG_CONFIG_LIBDIR}
    "/usr/${TOOLCHAIN_PREFIX}/lib/pkgconfig:/usr/${TOOLCHAIN_PREFIX}/share/pkgconfig")
unset(ENV{PKG_CONFIG_PATH})
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH OFF)

# Match what the windows-dist preset does natively: link the GCC runtime statically so
# the binaries have no dependency on the toolchain's own DLLs.
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-static-libgcc -static-libstdc++ -static")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++ -static")
