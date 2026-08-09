#!/usr/bin/env bash
# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0
#
# Stage and build the Windows installer: luvie.exe plus the luvie.lv2 plug-in bundle, in one
# per-user setup program. The script is the counterpart of tools/make-macos-dmg.sh and does
# the same job for the same reason -- CPack cannot express it. (CPack does have an INNOSETUP
# generator, but only from CMake 3.30, and this project requires 3.28.)
#
# Run from an MSYS2 shell after building the windows-dist preset. Inno Setup's compiler is
# found on PATH or in its usual Program Files location; it is preinstalled on GitHub's
# windows runners.
#
# Usage:
#     tools/make-windows-installer.sh [build-dir]     # default: build-dist

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:-${ROOT}/build-dist}"

[ -d "${BUILD}" ] || { echo "no build directory at ${BUILD} — configure first" >&2; exit 1; }

VERSION="$(git -C "${ROOT}" describe --tags --always --dirty 2>/dev/null || echo unknown)"
# The .iss needs a plain X.Y.Z as well, for the fields Windows itself parses: a description
# like v0.0.5-3-gabc1234 is fine as a display name but not as a VersionInfoVersion. Strip
# the leading v and anything from the first dash, and fall back to 0.0.0 the same way
# cmake/ProjectVersion.cmake does when there is no tag to read.
NUMERIC="$(printf '%s' "${VERSION#v}" | sed -n 's/^\([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/p')"
NUMERIC="${NUMERIC:-0.0.0}"

STAGE="${BUILD}/win-stage"

rm -rf "${STAGE}"
mkdir -p "${STAGE}"

# Both components, kept in the layout the install rules produce (bin/, lib/lv2/,
# share/doc/): the .iss reads from those paths and redistributes them to where each half
# belongs on Windows, so there is nothing to flatten here.
cmake --install "${BUILD}" --prefix "${STAGE}" --component Standalone
cmake --install "${BUILD}" --prefix "${STAGE}" --component Plugin

# ISCC.exe: on PATH, or where the installer puts it. Inno Setup 5 is not enough (the script
# uses WizardStyle and x64compatible), so 6 and 5 are not interchangeable here.
ISCC="$(command -v iscc || true)"
if [ -z "${ISCC}" ]; then
    for candidate in \
        "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" \
        "/c/Program Files/Inno Setup 6/ISCC.exe"
    do
        [ -x "${candidate}" ] && { ISCC="${candidate}"; break; }
    done
fi
if [ -z "${ISCC}" ]; then
    echo "Inno Setup 6 not found. Install it with:  choco install innosetup -y" >&2
    exit 1
fi

# Native paths: ISCC is a Windows program and does not understand MSYS2's /c/... form.
winpath() { cygpath -w "$1"; }

# MSYS2_ARG_CONV_EXCL: without it MSYS2 mangles every /D switch on its way to ISCC, which
# is a native Windows program -- an argument starting with a slash is assumed to be a POSIX
# path and is rewritten to C:\msys64\DAppVersion=... . ISCC then sees arguments that no
# longer begin with /, treats each as a script filename, and stops with "You may not
# specify more than one script filename". Excluding * disables the rewriting for this one
# command; the paths below are already converted explicitly by winpath.
MSYS2_ARG_CONV_EXCL='*' \
"${ISCC}" \
    "/DAppVersion=${VERSION}" \
    "/DNumericVersion=${NUMERIC}" \
    "/DStageDir=$(winpath "${STAGE}")" \
    "/DOutDir=$(winpath "${BUILD}")" \
    "$(winpath "${ROOT}/packaging/windows/luvie.iss")"

echo "built ${BUILD}/luvie-${VERSION}-windows-x86_64-setup.exe"
