#!/usr/bin/env bash
# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0
#
# Stage, optionally sign, and archive the macOS release: one zip holding both Luvie.app
# and the luvie.lv2 plug-in bundle.
#
# This is done here rather than with CPack because the order matters and CPack cannot
# express it: a .app must be signed *after* it is fully assembled and *before* it is
# archived, and the signature covers every byte — so stripping or touching anything
# afterwards invalidates it. `ditto` is used instead of `zip` for the same reason: it is
# the only archiver Apple guarantees round-trips a bundle's symlinks, resource forks and
# extended attributes, and a plain `zip` can quietly produce an app that fails to launch.
#
# Signing is optional. With no certificate configured the script still produces a working
# zip — users just get the Gatekeeper "unidentified developer" prompt on first launch and
# have to right-click -> Open once. Set these to sign and notarize:
#
#     MACOS_SIGN_IDENTITY   e.g. "Developer ID Application: Your Name (TEAMID)"
#     MACOS_NOTARY_PROFILE  a notarytool keychain profile name (optional; enables
#                           submission + stapling, so first launch has no prompt at all)
#
# Usage:
#     tools/make-macos-zip.sh [build-dir]     # default: build-dist

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:-${ROOT}/build-dist}"

[ -d "${BUILD}" ] || { echo "no build directory at ${BUILD} — configure first" >&2; exit 1; }

VERSION="$(git -C "${ROOT}" describe --tags --always --dirty 2>/dev/null || echo unknown)"
STAGE="${BUILD}/macos-stage"
OUT="${BUILD}/Luvie-${VERSION}-macos-universal.zip"

rm -rf "${STAGE}"
mkdir -p "${STAGE}"

# Both components into one tree: Luvie.app at the top (BUNDLE DESTINATION .) and the
# plug-in under lib/lv2 (LV2_INSTALL_DIR, relative in the dist presets).
cmake --install "${BUILD}" --prefix "${STAGE}" --component Standalone
cmake --install "${BUILD}" --prefix "${STAGE}" --component Plugin

# Flatten the two payloads to the top: nobody wants to dig through lib/lv2/ to find the
# one directory they are meant to copy into ~/Library/Audio/Plug-Ins/LV2, and the XDG-ish
# share/doc/ path the licences install to means nothing on macOS.
mv "${STAGE}/lib/lv2/luvie.lv2" "${STAGE}/luvie.lv2"
mv "${STAGE}/share/doc/luvie/LICENSES" "${STAGE}/LICENSES"
rm -rf "${STAGE}/lib" "${STAGE}/share"

cat > "${STAGE}/README.txt" <<EOF
Luvie ${VERSION}

  Luvie.app   drag to /Applications
  luvie.lv2   copy to ~/Library/Audio/Plug-Ins/LV2/ (create it if it does not exist)

Luvie can drive MIDI hardware and software instruments through CoreMIDI with no extra
setup. JACK is optional: if it is installed, Luvie finds it at runtime and offers JACK
transport and JACK MIDI ports as well.
EOF

if [ -n "${MACOS_SIGN_IDENTITY:-}" ]; then
    echo "signing with: ${MACOS_SIGN_IDENTITY}"
    # Plug-in modules first: signing is inside-out, and re-signing the app afterwards
    # would not cover a bundle that sits outside it anyway.
    for module in "${STAGE}"/luvie.lv2/*.so; do
        codesign --force --timestamp --options runtime \
                 --sign "${MACOS_SIGN_IDENTITY}" "${module}"
    done
    # --options runtime opts into the hardened runtime, which notarization requires.
    codesign --force --timestamp --options runtime --deep \
             --sign "${MACOS_SIGN_IDENTITY}" "${STAGE}/Luvie.app"
    codesign --verify --deep --strict --verbose=2 "${STAGE}/Luvie.app"
else
    echo "MACOS_SIGN_IDENTITY not set — producing an unsigned build"
fi

rm -f "${OUT}"
ditto -c -k --sequesterRsrc --keepParent "${STAGE}" "${OUT}"

if [ -n "${MACOS_NOTARY_PROFILE:-}" ]; then
    echo "submitting for notarization..."
    xcrun notarytool submit "${OUT}" --keychain-profile "${MACOS_NOTARY_PROFILE}" --wait
    # The ticket has to be stapled to the .app, not the zip (a zip cannot hold one), so
    # staple in the staging tree and re-archive. Without this the first launch needs a
    # network round trip to Apple, and fails outright if the machine is offline.
    xcrun stapler staple "${STAGE}/Luvie.app"
    rm -f "${OUT}"
    ditto -c -k --sequesterRsrc --keepParent "${STAGE}" "${OUT}"
fi

echo "built ${OUT}"
