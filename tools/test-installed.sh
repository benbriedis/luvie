#!/usr/bin/env bash
# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0
#
# Verify an *installed* Luvie -- from a .deb, .rpm or Arch package -- actually works.
#
# The build-tree smoke test (`ctest`, see src/CMakeLists.txt) proves the code runs; this
# proves the package does, which is a different claim. Everything checked below is
# something only packaging can get wrong: a file installed to the wrong prefix, a shared
# library the package forgot to depend on, a plugin bundle whose manifest a host cannot
# find. None of it would show up in a build-tree test, and all of it has to be checked on
# the target distribution rather than the build host.
#
# Deliberately distribution-agnostic: it takes no package manager, no build tree and no
# source, only an installed system. The caller installs the package and the handful of
# test tools first -- see the package-test jobs in .github/workflows/release.yml, or run
# it by hand after `pacman -U` / `dpkg -i` while checking a packaging change.
#
# Requires: xvfb-run (with xauth), jalv, a JACK server (jackd). lv2ls (from lilv) is used
# if present.
#
# Usage:
#     tools/test-installed.sh
#
# Environment:
#     LUVIE_EXPECT_VERSION   if set, `luvie --version` must contain this string

set -euo pipefail

PLUGIN_URI="https://github.com/benbriedis/luvie/luvie_dsp"
TMP="$(mktemp -d)"
JACKD_PID=""

cleanup() {
    [ -n "$JACKD_PID" ] && kill "$JACKD_PID" 2>/dev/null || true
    rm -rf "$TMP"
}
trap cleanup EXIT

say()  { printf '\n== %s\n' "$*"; }
fail() { printf '\nFAILED: %s\n' "$*" >&2; exit 1; }


# ---- The standalone application ------------------------------------------------
say "Standalone: on PATH and reporting its version"

command -v luvie >/dev/null || fail "luvie is not on PATH after installing the package"
VERSION="$(luvie --version)"
echo "$VERSION"
if [ -n "${LUVIE_EXPECT_VERSION:-}" ]; then
    case "$VERSION" in
        *"$LUVIE_EXPECT_VERSION"*) ;;
        # A package built from a shallow or tagless checkout reports a bare commit hash
        # and gets a 0.0.0 file name, which is easy not to notice until someone reports
        # it. Checking the tag survived into the binary is a one-line guard against it.
        *) fail "expected version to contain '$LUVIE_EXPECT_VERSION', got '$VERSION'" ;;
    esac
fi


# ---- Installed layout ----------------------------------------------------------
# The LV2 bundle is the only path that legitimately varies: /usr/lib/lv2 on Debian and
# Arch, and also on our .rpm (see cmake/Packaging.cmake for why it is not %{_libdir}).
say "Layout: files landed where they are supposed to"

LV2_BUNDLE=""
for dir in /usr/lib/lv2 /usr/lib64/lv2 /usr/local/lib/lv2; do
    if [ -d "$dir/luvie.lv2" ]; then LV2_BUNDLE="$dir/luvie.lv2"; break; fi
done
[ -n "$LV2_BUNDLE" ] || fail "no luvie.lv2 bundle under any of the standard LV2 directories"
echo "LV2 bundle: $LV2_BUNDLE"

for f in "$LV2_BUNDLE/manifest.ttl" "$LV2_BUNDLE/luvie_dsp.ttl" "$LV2_BUNDLE/luvie_dsp.so" \
         "$LV2_BUNDLE/luvie_ui.ttl" "$LV2_BUNDLE/luvie_ui.so" \
         /usr/share/applications/luvie.desktop \
         /usr/share/icons/hicolor/scalable/apps/luvie.svg \
         /usr/share/metainfo/com.benbriedis.luvie.metainfo.xml \
         /usr/share/doc/luvie/LICENSES/LICENSE \
         /usr/share/doc/luvie/LICENSES/NOTICE; do
    [ -f "$f" ] || fail "missing from the installed package: $f"
done
echo "all expected files present"

# Unresolved libraries are the classic packaging bug: the build host had a -dev package
# installed, the dependency never made it into the package's metadata, and the binary
# only fails on someone else's machine. `ldd` names them here instead.
say "Dependencies: every shared library resolves"

for bin in /usr/bin/luvie "$LV2_BUNDLE/luvie_dsp.so" "$LV2_BUNDLE/luvie_ui.so"; do
    if ldd "$bin" | grep 'not found'; then
        fail "$bin has unresolved shared libraries (missing package dependency)"
    fi
done
echo "no unresolved libraries"


# ---- The standalone application, running -------------------------------------------
# Same invocation as the build-tree smoke test: start up, show the window, quit through
# the normal teardown path. --test keeps it off JACK and routes MIDI to the console.
say "Standalone: starts, shows its window and exits cleanly"

xvfb-run -a luvie --test --exit-after 5 \
    || fail "luvie --test did not start and exit cleanly"
echo "standalone OK"


# ---- The LV2 plugin ----------------------------------------------------------------
say "Plugin: discoverable by an LV2 host"

# lv2ls walks the host's default LV2 search path, so this asks the question that
# matters -- is the bundle somewhere hosts look? -- rather than merely whether the files
# exist, which the layout check above already covered. The two differ exactly when a
# distribution's path and the package's install prefix disagree: a lib64 distribution
# searching /usr/lib64/lv2 will not find a bundle in /usr/lib/lv2, and the plugin is then
# invisible to every host on the system while looking perfectly well installed.
if command -v lv2ls >/dev/null; then
    if ! lv2ls | grep -Fxq "$PLUGIN_URI"; then
        echo "LV2_PATH=${LV2_PATH:-<host default>}"
        echo "installed at: $LV2_BUNDLE"
        fail "no LV2 host will find this plugin: lv2ls does not list $PLUGIN_URI"
    fi
    echo "lv2ls lists the plugin"
else
    echo "lv2ls not installed - skipping the discovery check"
fi

say "Plugin: loads and runs in jalv"

# jalv needs a JACK server. The dummy driver needs no sound card and no realtime
# privileges, which is what makes this runnable in a container.
jackd -r -d dummy -r 48000 -p 1024 >"$TMP/jackd.log" 2>&1 &
JACKD_PID=$!
if command -v jack_wait >/dev/null; then
    jack_wait -w -t 15 >/dev/null 2>&1 || { cat "$TMP/jackd.log"; fail "jackd did not start"; }
else
    # jack_wait lives in a separate package on some distributions; without it there is
    # nothing to ask, so give the server a moment and let jalv report the failure.
    sleep 3
fi
kill -0 "$JACKD_PID" 2>/dev/null || { cat "$TMP/jackd.log"; fail "jackd exited immediately"; }
echo "jackd (dummy driver) running"

# Both outcomes below are a pass, and which one you get depends on the jalv version:
# 1.6 reads commands from stdin and quits at EOF, so /dev/null makes it instantiate,
# activate, run and shut down on its own; 1.8 keeps running regardless and has to be
# stopped, which proves the same thing the long way round. Anything else -- a plugin it
# cannot find, cannot instantiate, or that dies once running -- ends the run early with
# a status that is neither.
set +e
timeout -k 5 30 jalv -n luvie-test "$PLUGIN_URI" </dev/null >"$TMP/jalv.log" 2>&1
JALV_STATUS=$?
set -e
cat "$TMP/jalv.log"

case $JALV_STATUS in
    0)   echo "jalv loaded and closed the plugin cleanly" ;;
    124) echo "jalv ran the plugin for the full 30s and was stopped by the timeout" ;;
    *)   fail "jalv exited $JALV_STATUS" ;;
esac

# Worth saying out loud rather than leaving in the output above for someone to notice.
# Not a failure: jalv 1.10 (Arch) crashes on shutdown with unrelated third-party plugins
# too -- x42's balance reproduces it -- so it is jalv's teardown, not the plugin's. Say so
# every time all the same, because if it ever starts happening on jalv 1.6 or 1.8, where
# it does not today, that would be worth looking at.
if grep -q 'dumped core' "$TMP/jalv.log"; then
    echo "note: jalv dumped core on shutdown; known jalv 1.10 behaviour, not a Luvie fault"
fi

# Some jalv failures are only a message on stderr. Match the decisive ones rather than
# anything containing "error", so an unrelated JACK warning does not fail the run.
if grep -Eqi 'not found|failed to instantiate|requires feature|unknown plugin' "$TMP/jalv.log"; then
    fail "jalv reported a problem loading the plugin (see its output above)"
fi

say "All package tests passed"
