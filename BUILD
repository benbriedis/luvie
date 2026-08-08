Building Luvie
==============

Luvie builds with CMake (>= 3.28) and produces three artifacts:

  - build/src/luvie            the standalone application
  - build/src/libluvie_core.a  the shared core static library
  - build/luvie.lv2/           the LV2 plugin bundle (luvie_dsp.so,
                               luvie_ui.so, *.ttl)
  - build/LICENSES/            licence texts (see "Licences" below)


Prerequisites
-------------

  - A C++23 compiler (g++-13 or newer; clang 16+ also works)
  - CMake >= 3.28 and a generator (Ninja recommended; Make works too)
  - git and a network connection (the configure step fetches FLTK, LV2,
    RtMidi, nlohmann/json and the JACK headers)

Luvie builds on Linux, macOS and Windows. Linux is covered below; see "Building
on macOS" and "Building on Windows" further down for those two.

JACK is not a build dependency on any platform: its headers are fetched and
pinned like the other dependencies, and the library itself is loaded at runtime
via dlopen, never linked. The resulting binary therefore runs on machines with
no JACK installed -- JACK transport and JACK MIDI simply stay unavailable until
libjack is present. See src/jackShim.cpp.

System development libraries (Linux):

      - ALSA   (libasound2-dev) -- via RtMidi, for the Native MIDI backend
      - (liblo is no longer a system dependency -- it is fetched and linked
        statically; see "NSM and liblo" below)
      - Plus the X11/Wayland/cairo/pango/fontconfig stack FLTK builds
        against (libx11-dev libwayland-dev libxkbcommon-dev libcairo2-dev
        libpango1.0-dev libfontconfig1-dev libdbus-1-dev libgtk-3-dev)

On Debian/Ubuntu:

    sudo apt install build-essential g++-13 cmake ninja-build pkg-config git \
        libasound2-dev \
        libx11-dev libwayland-dev libxkbcommon-dev libcairo2-dev libpango1.0-dev \
        libfontconfig1-dev libdbus-1-dev


Vendored dependencies
---------------------

FLTK 1.5, the LV2 headers, the JACK headers, RtMidi, nlohmann/json and liblo are
fetched and pinned automatically by CMake (FetchContent) during the configure
step -- there is no separate dependency bootstrap. Pinned revisions live in the
top-level CMakeLists.txt; sources are cloned into build/_deps/ and FLTK, RtMidi
and liblo are built static in-tree.

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

ALSA is the only remaining host-provided library, and it arrives indirectly:
RtMidi finds and links libasound itself for the Native MIDI backend. JACK is a
build-time dependency in the weakest possible sense -- only its headers, and
those are fetched too; the library is dlopen'd at runtime, never linked.


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


Step 3 -- Test
--------------

    ctest --test-dir build --output-on-failure

There is one test, "smoke": it starts the real application with --test (internal
transport, MIDI to the console, no JACK) plus --exit-after, which closes the
window on a timer so the process exits through its normal teardown path rather
than being killed. That covers startup wiring and the widget/observable
destruction order main.cpp is careful about -- the two things most likely to
break silently.

It needs a display. On a headless machine, run it under Xvfb:

    xvfb-run -a ctest --test-dir build --output-on-failure


Building on macOS
-----------------

Prerequisites: Xcode command line tools (clang covers C++23) and CMake + Ninja:

    brew install cmake ninja

Then:

    cmake --preset macos-dist
    cmake --build --preset macos-dist
    ctest --preset macos-dist

The macos-dist preset builds a universal binary (x86_64 + arm64) with a
deployment target of macOS 11, and produces build-dist/src/Luvie.app -- a real
application bundle with its icon and Info.plist, not a bare executable. The LV2
bundle lands in build-dist/luvie.lv2/ as usual; copy it to
~/Library/Audio/Plug-Ins/LV2/.

No Homebrew packages beyond ninja are needed: liblo and the JACK headers are
both fetched by CMake. If JACK for macOS is installed at runtime, Luvie finds
it; if not, it uses its internal transport and CoreMIDI (via RtMidi) for output.


Building on Windows
-------------------

The Windows build uses the MSYS2 UCRT64 toolchain -- GCC, so the same compiler
family, the same C++23 support and the same linker features (the LV2 plugin's
symbol-hiding uses a GNU version script) as the Linux build. Install MSYS2 from
https://www.msys2.org, then from a UCRT64 shell:

    pacman -S git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
              mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf

    cmake --preset windows-dist
    cmake --build --preset windows-dist
    ctest --preset windows-dist

The windows-dist preset links the GCC runtime statically, so the resulting
luvie.exe and the two plugin DLLs run on a machine with no MSYS2 installed.
Building without that preset produces binaries that need libstdc++-6.dll,
libgcc_s_seh-1.dll and libwinpthread-1.dll from MSYS2's bin/ on PATH.

The LV2 plugin's modules are named luvie_dsp.dll / luvie_ui.dll here; the
bundle's manifest.ttl is generated per platform to match (src/lv2/
manifest.ttl.in). Copy build-dist/luvie.lv2 to %APPDATA%\LV2\.

Checking the Windows build from Linux
-------------------------------------

You do not need Windows to find out whether the Windows build still compiles.
mingw-w64 cross-compiles the whole tree, FLTK and RtMidi included, in a couple
of minutes:

    sudo apt install g++-mingw-w64-x86-64
    cmake -S . -B build-mingw -G Ninja \
          -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw.cmake \
          -DCMAKE_BUILD_TYPE=Release
    cmake --build build-mingw

This is a compile check, not a shipping build: it links the MSVCRT C runtime
where MSYS2/UCRT64 links UCRT. Use it before pushing anything that touches the
platform branches in jackShim.cpp, appWindow.cpp, cursors.cpp or
src/lv2/stateFile.hpp -- it is much faster than waiting for CI, and it is how
both the M_PI portability bug and the plugin over-export bug below were found.

Not yet ported: Luvie's window has custom (non-native) decorations, and the code
that lets you drag and resize it is X11-specific (src/appWindow.cpp,
src/modern/cursors.cpp, both behind #ifdef FLTK_USE_X11). On macOS and Windows
the application runs but its window cannot be moved or resized yet.


Licences
--------

Luvie itself is Apache-2.0 (see LICENSE and NOTICE at the top of the tree).
Third-party licence texts are NOT checked in: build/LICENSES/ is assembled
during configure, copying them out of the already-fetched dependency sources in
build/_deps/ so the text always matches the version actually linked. It holds
Luvie's LICENSE and NOTICE at the top, then FLTK/, LV2/, RtMidi/ and
nlohmann_json/ subdirectories.

Wherever an installation finishes on its own, LICENSES/ ends up at the top of
what was installed: inside luvie.lv2 for the plugin (a .deb or `cmake --install`
leaves it on the LV2 path, and that is the end of the procedure), and under
share/doc/luvie for the app. An archive is not an installation, so a .tar.gz or
.zip carries LICENSES/ at its top level instead, beside the luvie.lv2 directory
you then copy onward by hand -- taking the licences with it is your business at
that point, and for practical purposes nothing turns on it.

This matters for binary distribution: RtMidi and nlohmann/json are MIT and the
LV2 headers are ISC, and all three require their notice to appear in every copy,
including copies compiled into a binary.

If a dependency bump moves a licence file upstream, configure fails with an
error naming the file rather than silently omitting it; fix the path in
cmake/GatherLicenses.cmake.


NSM and liblo
-------------

The NSM (New/Non Session Manager) client is built on every platform, and liblo
is fetched and linked statically rather than taken from the system. Nothing
about NSM is Linux-specific: the protocol is OSC over UDP, and Fl::add_fd --
which src/nsm.cpp uses to watch the OSC socket -- is implemented by FLTK's
Darwin and WinAPI drivers as well as the Unix one.

In practice the servers that speak the protocol (nsmd, RaySession) are Linux
projects, so on macOS and Windows nothing sets NSM_URL and the client stays
inert, costing only a little binary size. That is a better trade than compiling
it out: Luvie simply works if a server ever does turn up.

Static linking is the deliberate choice here, because liblo is LGPL-2.1+ and
that has consequences worth understanding rather than stumbling into. Section 6
of the LGPL permits distributing the combined work "under terms of your choice"
-- Luvie's own source stays Apache-2.0 -- in exchange for three things: a
prominent notice, a copy of the LGPL, and the means to relink against a modified
liblo. All three are met (NOTICE explains exactly how), and the last one is
nearly free because Luvie's source is public and the liblo revision is pinned.

The upshot is one fewer dynamic dependency everywhere: no liblo-dev to install,
no liblo7 in the .deb, no liblo.so bundled into the AppImage, and no
liblo.dylib to carry inside Luvie.app.

To build without it -- no NSM client, no liblo in the binary at all:

    cmake --preset linux-dist -DLUVIE_NSM=OFF

src/nsmStub.cpp then stands in for src/nsm.cpp, and cmake/GatherLicenses.cmake
drops liblo's licence from the distribution, since the binary no longer contains
any of it.


Plugin symbol exports
---------------------

An LV2 module is dlopen'd into the host's address space, sharing one symbol
namespace, so it must export exactly one symbol: its entry point. Exporting our
copies of FLTK, libstdc++ or RtMidi risks the loader interposing them against
the host's, which crashes someone else's process in a way that is very hard to
trace back here.

Each binary format needs a different mechanism, so src/lv2/CMakeLists.txt
selects on the format -- not on whether a flag is accepted, which is not the
same question: MinGW's ld accepts -Wl,--version-script and -Wl,--exclude-libs
and then ignores them, because both are ELF concepts.

    ELF     -fvisibility=hidden + --exclude-libs,ALL + --version-script
    Mach-O  -fvisibility=hidden + -exported_symbols_list
    PE      --exclude-all-symbols, and RtMidi's stray RTMIDI_EXPORT stripped in
            the top-level CMakeLists (it marks the whole static archive
            __declspec(dllexport), which no linker flag can undo)

Because all of that fails silently, every build re-checks the finished module's
export table with src/lv2/CheckExports.cmake and fails if anything beyond the
entry point appears. If you see that error, the flags for your format stopped
working -- do not delete the check.


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


Packaging
---------

The dist presets (linux-dist / macos-dist / windows-dist) are what CI uses to
build release artifacts. They configure into build-dist/ -- a separate tree, so
they never fight your development build/ over CMakeCache.txt -- in Release, with
the LV2 bundle redirected inside the install prefix so packaging can capture it.

Every install rule is tagged with a component, so the app and the plugin can be
shipped independently. A plain `cmake --install` still installs both. The
plugin's licence texts are their own components (PluginLicense, inside the
bundle, and PluginArchiveLicense, at the top of an archive) purely so they can
be placed differently per format -- cmake/CPackGenerator.cmake picks one, and
they are packaged with the plugin either way.

    cmake --preset linux-dist
    cmake --build --preset linux-dist
    cd build-dist && cpack        # generators are chosen per platform:
                                  #   Linux   TGZ + DEB (one .deb per component)
                                  #   macOS   ZIP
                                  #   Windows ZIP

Two formats need more than CPack can express, so each has a script:

    tools/make-appimage.sh        # Linux AppImage (standalone app)
    tools/make-macos-zip.sh       # macOS: Luvie.app + luvie.lv2 in one zip

The AppImage is the only format that redistributes Luvie's shared-library
dependencies rather than depending on the host's, which changes its licensing
obligations; the script assembles a LICENSES/bundled/ directory from the
libraries actually bundled to meet them. See NOTICE. Build it on the oldest
distribution you intend to support -- glibc is the one library an AppImage
cannot bundle, so the build host sets the floor. CI uses Ubuntu 22.04.

The macOS script stages, optionally signs and notarizes, then archives with
ditto, in that order: a .app's signature covers every byte, so it has to be
signed after assembly and before archiving.

Icons are pre-generated and committed (logo/luvie.ico, logo/luvie.icns), so no
icon toolchain is needed to build. Regenerate them after changing the logo:

    python3 tools/make-icons.py

Releases are automated -- see NOTES/RELEASING.txt and .github/workflows/.


Cleaning / rebuilding
---------------------

    rm -rf build build-dist              # full clean -- then re-run Step 1 (configure)
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

  - `Could NOT find jack/liblo` -- these checks no longer exist. Both are
    fetched by CMake now: the JACK headers because the library is dlopen'd, and
    liblo because it is linked statically.

  - `Licence file for <lib> not found` during configure -- a dependency bump
    moved the file upstream. Correct the path in cmake/GatherLicenses.cmake;
    don't drop the entry.

  - C++23 errors -- your compiler is too old; pass -DCMAKE_CXX_COMPILER=g++-13.
