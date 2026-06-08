# Building Luvie

Luvie builds with **CMake** (≥ 3.28) and produces three artifacts:

- `build/src/luvie` — the standalone application
- `build/src/libluvie_core.a` — the shared core static library
- `build/luvie.lv2/` — the LV2 plugin bundle (`luvie_dsp.so`, `luvie_ui.so`, `*.ttl`)

## Prerequisites

- A C++23 compiler (**g++-13** or newer; clang 16+ also works)
- CMake ≥ 3.28 and a generator (**Ninja** recommended; Make works too)
- **FLTK 1.5** — vendored and built locally under `deps/` (see below)
- System development libraries (Linux):
  - **JACK** (`libjack-dev` / `jack2`)
  - **ALSA** (`libasound2-dev`)
  - **liblo** (`liblo-dev`)
  - Plus the X11/Wayland/cairo/pango/fontconfig stack pulled in by FLTK

On Debian/Ubuntu:

```sh
sudo apt install build-essential g++-13 cmake ninja-build pkg-config \
    libjack-jackd2-dev libasound2-dev liblo-dev
```

## Step 0 — Build the vendored dependencies (one time)

FLTK is built from source into `deps/installation/`. If that directory is empty:

```sh
cd deps
./downloadAll.sh          # clones FLTK + LV2 sources into deps/sources/
./installScripts/fltk.sh  # builds + installs FLTK (static) into deps/installation/
cd ..
```

The LV2 headers used by the plugin come from `deps/sources/lv2/include` (header
only — nothing to compile).

## Step 1 — Configure

```sh
cmake -S . -B build -G Ninja \
    -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_C_COMPILER=gcc
```

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
rm -rf build        # full clean
cmake --build build --target clean   # remove build outputs, keep config
```

## Troubleshooting

- **`Could NOT find FLTK`** — you haven't built the vendored FLTK yet. Run
  Step 0. The build looks for it under `deps/installation/`.
- **`Could NOT find jack/alsa/liblo`** — install the corresponding `-dev`
  packages (see Prerequisites).
- **C++23 errors** — your compiler is too old; pass `-DCMAKE_CXX_COMPILER=g++-13`.
