#!/bin/bash
# Guard de rebuild para la vitaGL vendorizada.
# Se invoca desde CMake en cada build; solo hace `make clean` cuando los
# flags cambian (comparando con el sello en build/vitagl.stamp).
# Uso: build_vitagl.sh "<flags>" "<stamp>" "<srcdir>" ["<make>"]
set -u

MAKE_FLAGS="${1:?falta arg 1 (flags)}"
STAMP="${2:?falta arg 2 (stamp)}"
SRC_DIR="${3:?falta arg 3 (vitaGL dir)}"
MAKE_BIN="${4:-make}"

if [ ! -d "$SRC_DIR" ]; then
    echo "build_vitagl.sh: ERROR: no existe el dir de vitaGL: $SRC_DIR" >&2
    exit 1
fi
if [ ! -f "$SRC_DIR/Makefile" ]; then
    echo "build_vitagl.sh: ERROR: sin Makefile en: $SRC_DIR" >&2
    exit 1
fi

echo "$MAKE_FLAGS" > "${STAMP}.tmp"
if ! cmp -s "${STAMP}.tmp" "$STAMP" 2>/dev/null; then
    echo "vitaGL flags changed, rebuilding..."
    "$MAKE_BIN" -C "$SRC_DIR" clean || exit 1
    cp "${STAMP}.tmp" "$STAMP"
else
    rm -f "${STAMP}.tmp"
fi
