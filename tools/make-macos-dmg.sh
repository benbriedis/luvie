#!/usr/bin/env bash
# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0
#
# Stage, optionally sign, and archive the macOS release: one disk image holding both
# Luvie.app and the luvie.lv2 plug-in bundle.
#
# This is done here rather than with CPack because the order matters and CPack cannot
# express it: a .app must be signed *after* it is fully assembled and *before* it is
# archived, and the signature covers every byte — so stripping or touching anything
# afterwards invalidates it.
#
# A disk image rather than a zip. Both are just containers as far as Gatekeeper is
# concerned, but a .dmg mounts into a window that can hold an /Applications symlink to drag
# onto and shows README.txt where someone will actually read it. It is also the only one of
# the two that can carry a stapled notarization ticket (see the end of this script). Note
# a .pkg would be a mistake: it needs a second certificate type ("Developer ID Installer")
# and an unsigned one cannot be opened at all on current macOS.
#
# Signing is optional and this script produces a working image without it — see README.txt
# below for what users then have to do. Set these to sign and notarize:
#
#     MACOS_SIGN_IDENTITY   e.g. "Developer ID Application: Your Name (TEAMID)"
#     MACOS_NOTARY_PROFILE  a notarytool keychain profile name (optional; enables
#                           submission + stapling, so first launch has no prompt at all
#                           and the plug-in needs no de-quarantining)
#
# Usage:
#     tools/make-macos-dmg.sh [build-dir]     # default: build-dist

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:-${ROOT}/build-dist}"

[ -d "${BUILD}" ] || { echo "no build directory at ${BUILD} — configure first" >&2; exit 1; }

VERSION="$(git -C "${ROOT}" describe --tags --always --dirty 2>/dev/null || echo unknown)"
STAGE="${BUILD}/macos-stage"
OUT="${BUILD}/Luvie-${VERSION}-macos-universal.dmg"

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

# The drag target. A symlink costs nothing in the image and turns "install the app" into
# one gesture inside the window that just opened.
ln -s /Applications "${STAGE}/Applications"

# The plug-in instruction comes first, and at length, because it is the only step here a
# user cannot work out for themselves. An unsigned app at least announces itself: macOS
# says it cannot verify the developer and Privacy & Security offers "Open Anyway". A
# quarantined plug-in announces nothing — the host simply does not list it, so there is no
# symptom to search for and nothing to click. Hence xattr, spelled out.
cat > "${STAGE}/README.txt" <<EOF
Luvie ${VERSION}

Luvie is not signed with an Apple Developer certificate (that needs a paid annual
subscription), so macOS holds both halves at arm's length until you say otherwise. Two
short steps, one for each.


1. The LV2 plug-in — do this or your DAW will not see it

   Copy luvie.lv2 into your LV2 folder, creating it if it does not exist:

       mkdir -p ~/Library/Audio/Plug-Ins/LV2
       cp -R "/Volumes/Luvie ${VERSION}/luvie.lv2" ~/Library/Audio/Plug-Ins/LV2/

   Then clear the quarantine flag macOS put on it when you downloaded this image:

       xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/LV2/luvie.lv2

   Without that second command the plug-in does not appear in your host's plug-in list
   at all, with no error and nothing to indicate why. It is not a broken install.


2. The standalone application

   Drag Luvie.app onto the Applications shortcut in this window.

   The first time you open it macOS will refuse, saying it cannot verify the developer.
   Go to System Settings -> Privacy & Security, scroll down to the message about Luvie,
   and click "Open Anyway". Only the first launch needs this.

   (On macOS 15 and later, right-clicking the app and choosing Open no longer works --
   Apple removed that shortcut. Use Privacy & Security as above.)


Luvie can drive MIDI hardware and software instruments through CoreMIDI with no extra
setup. JACK is optional: if it is installed, Luvie finds it at runtime and offers JACK
transport and JACK MIDI ports as well.

Source, issues and the licence: https://github.com/benbriedis/luvie
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
# UDZO: zlib-compressed and read-only, which is what every distributed image is. The volume
# name is what appears in Finder's sidebar and in the /Volumes path README.txt quotes.
hdiutil create -volname "Luvie ${VERSION}" -srcfolder "${STAGE}" \
        -ov -format UDZO "${OUT}"

if [ -n "${MACOS_NOTARY_PROFILE:-}" ]; then
    # Notarizing what was never signed is not a thing Apple accepts, and the failure it
    # produces further down is opaque. Say so here instead.
    [ -n "${MACOS_SIGN_IDENTITY:-}" ] || {
        echo "MACOS_NOTARY_PROFILE is set but MACOS_SIGN_IDENTITY is not — nothing to notarize" >&2
        exit 1
    }
    # The image is signed too, not just its contents: notarization requires it, and it is
    # what lets the ticket be stapled to the .dmg itself below.
    codesign --force --timestamp --sign "${MACOS_SIGN_IDENTITY}" "${OUT}"
    echo "submitting for notarization..."
    xcrun notarytool submit "${OUT}" --keychain-profile "${MACOS_NOTARY_PROFILE}" --wait
    # Staple to the image rather than to the app inside it. Either would do for the app,
    # but only this covers the plug-in bundle as well, and it survives being copied out.
    # Without a ticket the first launch needs a network round trip to Apple, and fails
    # outright if the machine is offline.
    xcrun stapler staple "${OUT}"
fi

echo "built ${OUT}"
