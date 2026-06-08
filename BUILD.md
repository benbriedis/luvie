# Building Luvie

Luvie builds with **CMake** (≥ 3.28) and produces three artifacts:

- `build/src/luvie` — the standalone application
- `build/src/libluvie_core.a` — the shared core static library
- `build/luvie.lv2/` — the LV2 plugin bundle (`luvie_dsp.so`, `luvie_ui.so`, `*.ttl`)

## Prerequisites

- A C++23 compiler (**g++-13** or newer; clang 16+ also works)
- CMake ≥ 3.28 and a generator (**Ninja** recommended; Make works too)
- **git** and a network connection (the configure step fetches FLTK + LV2)
- System development libraries (Linux):
  - **JACK** (`libjack-dev` / `jack2`)
  - **ALSA** (`libasound2-dev`)
  - **liblo** (`liblo-dev`)
  - Plus the X11/Wayland/cairo/pango/fontconfig stack FLTK builds against
    (`libx11-dev libwayland-dev libxkbcommon-dev libcairo2-dev libpango1.0-dev
    libfontconfig1-dev libdbus-1-dev libgtk-3-dev`)

On Debian/Ubuntu:

```sh
sudo apt install build-essential g++-13 cmake ninja-build pkg-config git \
    libjack-jackd2-dev libasound2-dev liblo-dev \
    libx11-dev libwayland-dev libxkbcommon-dev libcairo2-dev libpango1.0-dev \
    libfontconfig1-dev libdbus-1-dev
```

## Vendored dependencies

FLTK 1.5 and the LV2 headers are fetched and pinned automatically by CMake
(`FetchContent`) during the configure step — there is no separate dependency
bootstrap. Pinned revisions live in the top-level `CMakeLists.txt`; sources are
cloned into `build/_deps/` and FLTK is built static in-tree. liblo, JACK, and
ALSA are host-provided system libraries (found via `pkg-config`).

## Step 1 — Configure

This step creates and populates the `build/` directory (it generates
`CMakeCache.txt` and the Ninja files). **You must run it before building.** Do
**not** create `build/` yourself — CMake makes it. Running `cmake --build build`
on a missing or empty `build/` fails with `Error: could not load cache`.

```sh
cmake -S . -B build -G Ninja \
    -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_C_COMPILER=gcc
```

You only need to re-run configure after a clean (`rm -rf build`) or when you
change a `CMakeLists.txt`. Otherwise just rebuild (Step 2).

Notes:
- The build is `Debug` by default (matches the old `-g` flags). For an optimized
  build add `-DCMAKE_BUILD_TYPE=Release`.
- `CMAKE_CXX_COMPILER` is passed explicitly because the system default `g++` may
  be too old for C++23. Omit it if your default compiler is new enough.
- `compile_commands.json` is generated automatically at
  `build/compile_commands.json` (point your editor/LSP there — replaces `bear`).

## Step 2 — Build

```sh
cmake --build build              # builds app + core lib + plugin bundle
cmake --build build --target luvie       # app only
cmake --build build --target luvie_ui    # plugin UI only (also builds the bundle)
```

Run the standalone app:

```sh
./build/src/luvie
```

## Installing

### Standalone app

Installs the binary, `.desktop` entry, and icon. Use a user-local prefix:

```sh
cmake --install build --prefix ~/.local
```

(Or system-wide with `sudo cmake --install build` — default prefix `/usr/local`.)

### LV2 plugin

The plugin bundle is assembled at `build/luvie.lv2/` during the build. A single
`cmake --install build` installs **both** the app (under the prefix) and the
plugin bundle (into `LV2_INSTALL_DIR`, an absolute path independent of the
prefix — LV2 plugins must live on the LV2 search path, not under `/usr/local`).

By default the plugin installs to `~/.lv2/luvie.lv2`. You can also just copy it:

```sh
cp -r build/luvie.lv2 ~/.lv2/
```

Override the install location at configure time:

```sh
cmake -S . -B build -DLV2_INSTALL_DIR=/usr/local/lib/lv2 ...
```

Verify the host sees it:

```sh
lv2ls | grep luvie
```

## Cleaning / rebuilding

```sh
rm -rf build        # full clean — then re-run Step 1 (configure) before building
cmake --build build --target clean   # remove build outputs, keep config
```

After `rm -rf build` you must re-run the **configure** command (Step 1) before
`cmake --build build` — the cleaned directory has no cache.

## Troubleshooting

- **`Error: could not load cache`** — you ran `cmake --build build` without
  configuring first (or created an empty `build/` by hand). Run the Step 1
  configure command; don't create `build/` yourself.
- **FLTK fetch/build fails** — FLTK is fetched and built during configure; make
  sure you have network access and the X11/Wayland/cairo/pango/fontconfig `-dev`
  packages (see Prerequisites). Re-run configure after installing them.
- **`Could NOT find jack/alsa/liblo`** — install the corresponding `-dev`
  packages (see Prerequisites).
- **C++23 errors** — your compiler is too old; pass `-DCMAKE_CXX_COMPILER=g++-13`.
