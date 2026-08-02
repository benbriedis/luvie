#!/usr/bin/env bash
# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0
#
# Run Luvie standalone under Valgrind Memcheck.
#
# Usage:  tools/run-valgrind.sh [mode] [-- extra luvie args]
#   mode = test    (default) --test: internal transport + debug MIDI, no JACK
#          jack             real JACK backend
#
# Prefer 'test'. Under Memcheck the process runs 20-50x slower, so a real JACK
# client misses every RT deadline and JACK drops it; --test swaps in the internal
# transport and debug MIDI output, which has no deadline to miss. The trade-off is
# that the JACK RT path (and its no-allocation rule) is then not exercised at all
# -- Memcheck cannot cover that path regardless; it needs a different tool.
#
# Close the window normally when done: the leak report is only produced on a
# clean exit, so killing the process discards the result.
#
# Logs land in build/valgrind/ (already git-ignored via build/).

set -u
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LUVIE="$REPO/build/src/luvie"
LOGDIR="$REPO/build/valgrind"

MODE="${1:-test}"
[ $# -gt 0 ] && shift
[ "${1:-}" = "--" ] && shift

case "$MODE" in
  test) APP_ARGS=(--test) ;;
  jack) APP_ARGS=() ;;
  *)    echo "unknown mode: $MODE (use 'test' or 'jack')" >&2; exit 2 ;;
esac

if [ ! -x "$LUVIE" ]; then
    echo "luvie not built at $LUVIE -- run: cmake --build build" >&2
    exit 1
fi

mkdir -p "$LOGDIR"
LOG="$LOGDIR/valgrind-$MODE-$(date +%Y%m%d-%H%M%S).log"

# Make the allocators honest: glib pools blocks and Mesa caches shaders, both of
# which hide real malloc/free traffic from Memcheck.
export G_SLICE=always-malloc
export G_DEBUG=gc-friendly
export MESA_GLSL_CACHE_DISABLE=true

echo "Logging to $LOG"
echo "Close the Luvie window normally to get the leak report (do NOT kill it)."

valgrind \
  --tool=memcheck \
  --leak-check=full \
  --show-leak-kinds=definite,possible \
  --errors-for-leak-kinds=definite \
  --track-origins=yes \
  --num-callers=40 \
  --error-limit=no \
  --keep-debuginfo=yes \
  --suppressions="$REPO/tools/valgrind.supp" \
  --gen-suppressions=all \
  --log-file="$LOG" \
  "$LUVIE" "${APP_ARGS[@]}" "$@"

echo
echo "=== summary ==="
sed -n '/LEAK SUMMARY/,/suppressed:/p;/ERROR SUMMARY/p' "$LOG"

echo
echo "=== memory-safety errors ==="
if grep -qE "^==[0-9]+== (Invalid|Use of|Conditional|Uninitialised|Mismatched|Syscall)" "$LOG"; then
    grep -E "^==[0-9]+== (Invalid|Use of|Conditional|Uninitialised|Mismatched|Syscall)" "$LOG" | sort | uniq -c
    echo "(see $LOG for full stacks)"
else
    echo "none"
fi

echo
echo "Full log: $LOG"
