Building Luvie
==============

Luvie builds with CMake (>= 3.28) and produces three artifacts:

  - build/src/luvie            the standalone application
  - build/src/libluvie_core.a  the shared core static library
  - build/luvie.lv2/           the LV2 plugin bundle (luvie_dsp.so,
                               luvie_ui.so, *.ttl, LICENSES/)
  - build/LICENSES/            licence texts (see "Licences" below)


Prerequisites
-------------

  - A C++23 compiler (g++-13 or newer; clang 16+ also works)
  - CMake >= 3.28 and a generator (Ninja recommended; Make works too)
  - git and a network connection (the configure step fetches FLTK, LV2,
    RtMidi and nlohmann/json)
  - System development libraries (Linux):
      - JACK   (libjack-dev / jack2) -- HEADERS ONLY at build time. The JACK
               library itself is loaded at runtime via dlopen, not linked, so
               the resulting binary runs on machines without JACK installed
               (JACK transport + MIDI simply stay unavailable until libjack is
               present). See src/jackShim.cpp.
      - ALSA   (libasound2-dev)
      - liblo  (liblo-dev)
      - Plus the X11/Wayland/cairo/pango/fontconfig stack FLTK builds
        against (libx11-dev libwayland-dev libxkbcommon-dev libcairo2-dev
        libpango1.0-dev libfontconfig1-dev libdbus-1-dev libgtk-3-dev)

On Debian/Ubuntu:

    sudo apt install build-essential g++-13 cmake ninja-build pkg-config git \
        libjack-jackd2-dev libasound2-dev liblo-dev \
        libx11-dev libwayland-dev libxkbcommon-dev libcairo2-dev libpango1.0-dev \
        libfontconfig1-dev libdbus-1-dev


Vendored dependencies
---------------------

FLTK 1.5, the LV2 headers, RtMidi and nlohmann/json are fetched and pinned
automatically by CMake (FetchContent) during the configure step -- there is no
separate dependency bootstrap. Pinned revisions live in the top-level
CMakeLists.txt; sources are cloned into build/_deps/ and FLTK and RtMidi are
built static in-tree.

nlohmann/json is fetched rather than taken from the system (it used to be found
with find_package) for two reasons: find_package accepted any system 3.x, so the
header being compiled against was not pinned; and the distro package ships no
licence file at all -- only an SPDX line inside json.hpp -- leaving nothing for
the licence gathering below to collect.

Each fetched dependency is declared EXCLUDE_FROM_ALL. That keeps their own
install rules out of `cmake --install`; without it FLTK and RtMidi deposit their
headers, static libs, pkg-config and CMake config files, man pages and a stray
fltk-options.desktop into the prefix alongside Luvie. They are still built on
demand, since our targets link them.

liblo and ALSA are host-provided system libraries, found via pkg-config. JACK is
provided the same way for its headers only -- the library is not linked but
dlopen'd at runtime, so JACK is a build-time (header) dependency yet only an
optional runtime one.


Step 1 -- Configure
-------------------

This step creates and populates the build/ directory (it generates
CMakeCache.txt and the Ninja files). You must run it before building. Do not
create build/ yourself -- CMake makes it. Running `cmake --build build` on a
missing or empty build/ fails with `Error: could not load cache`.

The simplest way is the bundled CMakePresets.json, which already encodes the
Ninja generator, the g++-13/gcc compilers, and the build/ directory:

    cmake --preset default            # Debug build (default)
    cmake --preset release            # optimized build

The equivalent without presets is the longer:

    cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_C_COMPILER=gcc

You only need to re-run configure after a clean (rm -rf build) or when you
change a CMakeLists.txt. Otherwise just rebuild (Step 2).

To override toolchain choices without editing the repo, create your own
CMakeUserPresets.json (git-ignored) that inherits from "default".

Notes:
  - The build is Debug by default (matches the old -g flags). For an optimized
    build add -DCMAKE_BUILD_TYPE=Release.
  - CMAKE_CXX_COMPILER is passed explicitly because the system default g++ may
    be too old for C++23. Omit it if your default compiler is new enough.
  - compile_commands.json is generated automatically at
    build/compile_commands.json (point your editor/LSP there -- replaces bear).


Step 2 -- Build
---------------

    cmake --build --preset default           # builds app + core lib + plugin bundle
    cmake --build build                      # equivalent without a preset
    cmake --build build --target luvie       # app only
    cmake --build build --target luvie_ui    # plugin UI only (also builds the bundle)

Run the standalone app:

    ./build/src/luvie


Licences
--------

Luvie itself is Apache-2.0 (see LICENSE and NOTICE at the top of the tree).
Third-party licence texts are NOT checked in: build/LICENSES/ is assembled
during configure, copying them out of the already-fetched dependency sources in
build/_deps/ so the text always matches the version actually linked. It holds
Luvie's LICENSE and NOTICE at the top, then FLTK/, LV2/, RtMidi/ and
nlohmann_json/ subdirectories, and is installed alongside the app and inside the
plugin bundle.

This matters for binary distribution: RtMidi and nlohmann/json are MIT and the
LV2 headers are ISC, and all three require their notice to appear in every copy,
including copies compiled into a binary.

If a dependency bump moves a licence file upstream, configure fails with an
error naming the file rather than silently omitting it; fix the path in
cmake/GatherLicenses.cmake.


Running under PipeWire
----------------------

On most current distros PipeWire is the session audio server and provides a
drop-in JACK implementation (pipewire-jack), so Luvie's JACK transport/MIDI
works without manually starting jackd -- PipeWire is always running. Luvie loads
libjack via dlopen, so it transparently uses whichever libjack.so.0 is on the
loader path.

  - Fedora-style systems install pipewire-jack's libjack on the normal library
    path, so a plain `./build/src/luvie` already routes JACK through PipeWire.

  - Debian/Ubuntu-style systems keep pipewire-jack's libjack off the default
    path (e.g. /usr/lib/x86_64-linux-gnu/pipewire-0.3/jack/) so it doesn't
    shadow real JACK. There, launch Luvie through the pw-jack wrapper to route
    JACK through PipeWire:

        pw-jack ./build/src/luvie

    (pw-jack just sets LD_LIBRARY_PATH to PipeWire's libjack; without it Luvie
    uses real jack2 if installed, or runs with JACK unavailable.)

The Native (ALSA) MIDI backend uses the kernel ALSA sequencer and works
regardless of PipeWire.


Installing
----------

Standalone app:

Installs the binary, .desktop entry, icon, and the licence texts under
share/doc/luvie/LICENSES. Use a user-local prefix:

    cmake --install build --prefix ~/.local

(Or system-wide with `sudo cmake --install build` -- default prefix
/usr/local.)

LV2 plugin:

The plugin bundle is assembled at build/luvie.lv2/ during the build. A single
`cmake --install build` installs both the app (under the prefix) and the plugin
bundle (into LV2_INSTALL_DIR, an absolute path independent of the prefix -- LV2
plugins must live on the LV2 search path, not under /usr/local).

By default the plugin installs to ~/.lv2/luvie.lv2. You can also just copy it:

    cp -r build/luvie.lv2 ~/.lv2/

Override the install location at configure time:

    cmake -S . -B build -DLV2_INSTALL_DIR=/usr/local/lib/lv2 ...

Verify the host sees it:

    lv2ls | grep luvie


Cleaning / rebuilding
---------------------

    rm -rf build                         # full clean -- then re-run Step 1 (configure)
    cmake --build build --target clean   # remove build outputs, keep config

After `rm -rf build` you must re-run the configure command (Step 1) before
`cmake --build build` -- the cleaned directory has no cache.


Troubleshooting
---------------

  - `Error: could not load cache` -- you ran `cmake --build build` without
    configuring first (or created an empty build/ by hand). Run the Step 1
    configure command; don't create build/ yourself.

  - FLTK fetch/build fails -- FLTK is fetched and built during configure; make
    sure you have network access and the X11/Wayland/cairo/pango/fontconfig
    -dev packages (see Prerequisites). Re-run configure after installing them.

  - `Could NOT find jack/alsa/liblo` -- install the corresponding -dev packages
    (see Prerequisites).

  - `Licence file for <lib> not found` during configure -- a dependency bump
    moved the file upstream. Correct the path in cmake/GatherLicenses.cmake;
    don't drop the entry.

  - C++23 errors -- your compiler is too old; pass -DCMAKE_CXX_COMPILER=g++-13.
