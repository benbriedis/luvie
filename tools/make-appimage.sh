#!/usr/bin/env bash
# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0
#
# Build an AppImage of the Luvie standalone application.
#
# An AppImage complements the .deb and the tarball: it is a single executable file that
# carries every shared library Luvie needs, so it runs on any distribution new enough for
# the glibc it was built against — no packaging per distro, no dependency install. That
# makes the *build host* part of the artifact's contract: build this on the oldest
# distribution you intend to support (CI uses ubuntu-22.04, glibc 2.35), because glibc is
# the one library an AppImage cannot bundle.
#
# JACK is deliberately not bundled, and could not be: it is dlopen'd, never linked (see
# src/jackShim.cpp), so the AppImage runs with or without JACK installed on the host and
# picks it up when present — which is the whole point of the shim.
#
# Usage:
#     tools/make-appimage.sh [build-dir]     # default: build-dist
#
# Requires linuxdeploy on PATH, or downloads it to the build directory. Writes
# Luvie-<version>-<arch>.AppImage into the build directory.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARCH="$(uname -m)"

[ -d "${1:-${ROOT}/build-dist}" ] || {
    echo "no build directory at ${1:-${ROOT}/build-dist} — configure first" >&2; exit 1; }
# Resolved to an absolute path: linuxdeploy has to be run from inside the build
# directory (it writes the AppImage to the working directory), and a relative BUILD
# would stop resolving the moment we cd there.
BUILD="$(cd "${1:-${ROOT}/build-dist}" && pwd)"

VERSION="$(git -C "${ROOT}" describe --tags --always --dirty 2>/dev/null || echo unknown)"

APPDIR="${BUILD}/AppDir"
rm -rf "${APPDIR}"

# Only the Standalone component: an AppImage is a single application, and the LV2 bundle
# has to land in the host's plug-in path to be of any use, so it ships separately.
# --prefix usr is what linuxdeploy expects to find inside an AppDir.
cmake --install "${BUILD}" --prefix "${APPDIR}/usr" --component Standalone

# linuxdeploy walks the binary's NEEDED entries and copies in the transitive closure of
# shared libraries, then writes AppRun and the top-level .desktop/icon symlinks.
LINUXDEPLOY="$(command -v linuxdeploy-${ARCH}.AppImage || true)"
if [ -z "${LINUXDEPLOY}" ]; then
    LINUXDEPLOY="${BUILD}/linuxdeploy-${ARCH}.AppImage"
    if [ ! -x "${LINUXDEPLOY}" ]; then
        echo "downloading linuxdeploy..."
        curl -fsSL -o "${LINUXDEPLOY}" \
            "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
        chmod +x "${LINUXDEPLOY}"
    fi
fi

# The icon is passed explicitly rather than left to discovery: linuxdeploy matches it
# against the desktop file's `Icon=luvie` by *filename*, and the source is luvie-256.png.
ICON="${BUILD}/luvie.png"
cp "${ROOT}/logo/png/luvie-256.png" "${ICON}"

# Containers (and GitHub runners) have no FUSE, which the AppImage runtime normally uses
# to mount itself; the tools honour APPIMAGE_EXTRACT_AND_RUN to unpack instead.
export APPIMAGE_EXTRACT_AND_RUN=1
export OUTPUT="Luvie-${VERSION}-${ARCH}.AppImage"
export VERSION

cd "${BUILD}"

# Two passes. The first populates the AppDir; the AppImage itself is only produced by
# the second, after the licence gathering below has added files to it.
"${LINUXDEPLOY}" \
    --appdir "${APPDIR}" \
    --executable "${APPDIR}/usr/bin/luvie" \
    --desktop-file "${APPDIR}/usr/share/applications/luvie.desktop" \
    --icon-file "${ICON}"

# ---- Licences for the bundled libraries ------------------------------------------
# An AppImage is the one format where Luvie *redistributes* its shared-library
# dependencies rather than depending on the host's, which changes its obligations. The
# LICENSES/ directory built by cmake/GatherLicenses.cmake covers only what is compiled
# in, and its note that liblo is "linked dynamically and never bundled by us" stops
# being true right here: linuxdeploy copies liblo.so into the AppDir, so LGPL-2.1
# section 6 applies and the licence and a source offer have to travel with it. The
# permissively-licensed libraries alongside it (libpng, libbz2, the X11 libraries, ...)
# require their notices to be retained in binary redistributions too.
#
# The list is derived from what linuxdeploy actually bundled, not written by hand, so a
# new dependency cannot slip through undocumented.
BUNDLED="${APPDIR}/usr/share/doc/luvie/LICENSES/bundled"
mkdir -p "${BUNDLED}"

{
    echo "Libraries bundled inside this AppImage"
    echo "======================================"
    echo
    echo "Unlike the tarball or .deb, an AppImage carries copies of the shared libraries"
    echo "Luvie needs. Each is a separate work under its own terms, reproduced in this"
    echo "directory; none of them alters the terms on which Luvie itself is offered (see"
    echo "../NOTICE). All are dynamically linked and can be replaced: extract the AppImage"
    echo "with --appimage-extract, substitute the library in usr/lib/, and repack."
    echo
    echo "liblo is LGPL-2.1-or-later. Its complete corresponding source is available from"
    echo "https://github.com/radarsat1/liblo, and the version bundled here is the one"
    echo "packaged by the distribution named in its copyright file below."
    echo
} > "${BUNDLED}/README.txt"

for lib in "${APPDIR}"/usr/lib/*.so*; do
    [ -f "${lib}" ] || continue
    base="$(basename "${lib}")"
    # Map the copy back to the host file it came from, then to its package, then to the
    # copyright file the distribution ships for it.
    # No `exit` in the awk program: leaving the pipe early kills ldconfig with SIGPIPE,
    # which `set -o pipefail` then reports as a failed assignment. Read it all and keep
    # the first match instead.
    host="$(ldconfig -p | awk -v n="${base}" '$1 == n && !seen { print $NF; seen = 1 }')"
    # On a usr-merged system ldconfig reports /lib/... while dpkg's database records
    # /usr/lib/..., so the raw path finds nothing; try the merged form and the symlink
    # target as well. `dpkg -S` prints "pkg:arch: /path", hence the cut.
    pkg=""
    for cand in "${host}" "/usr${host}" "$(readlink -f "${host}" 2>/dev/null)"; do
        [ -n "${cand}" ] || continue
        # `|| true` because a miss is expected and normal here: dpkg -S exits non-zero
        # when the path is unknown, which `set -euo pipefail` would otherwise treat as
        # a fatal error rather than as "try the next candidate". awk rather than
        # `head -1 | cut` for the same reason: head closing the pipe early kills dpkg
        # with SIGPIPE, which pipefail reports as another spurious failure.
        pkg="$(dpkg -S "${cand}" 2>/dev/null | awk -F: 'NR == 1 { print $1 }' || true)"
        if [ -n "${pkg}" ]; then break; fi
    done
    if [ -n "${pkg}" ] && [ -f "/usr/share/doc/${pkg}/copyright" ]; then
        mkdir -p "${BUNDLED}/${pkg}"
        cp "/usr/share/doc/${pkg}/copyright" "${BUNDLED}/${pkg}/copyright"
        echo "  ${base} -> ${pkg}" >> "${BUNDLED}/README.txt"
    else
        # Better to fail the build than to ship a library with no licence beside it.
        echo "ERROR: no licence found for bundled library ${base}" >&2
        echo "       (host=${host:-not found} pkg=${pkg:-not found})" >&2
        exit 1
    fi
done

"${LINUXDEPLOY}" --appdir "${APPDIR}" --output appimage

echo "built ${BUILD}/${OUTPUT}"
