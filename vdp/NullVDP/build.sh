#!/bin/bash
# Build NullVDP with the open-source Gowin toolchain (oss-cad-suite).
#
#   ./build.sh            # synthesize + place/route + pack -> nullvdp.fs
#   ./build.sh program    # also flash over the on-board USB-JTAG
#
# Sources oss-cad-suite from a few common locations if yosys is not already
# in PATH.  Override with OSS_CAD_SUITE=/path/to/oss-cad-suite.
set -e

cd "$(dirname "$0")"

if ! command -v yosys >/dev/null 2>&1; then
    for cand in "${OSS_CAD_SUITE:-}" \
                "$HOME/Apps/oss-cad-suite" \
                "$HOME/oss-cad-suite" \
                "/opt/oss-cad-suite"; do
        if [ -n "$cand" ] && [ -f "$cand/environment" ]; then
            # shellcheck disable=SC1090
            source "$cand/environment"
            break
        fi
    done
fi

if ! command -v yosys >/dev/null 2>&1; then
    echo "ERROR: oss-cad-suite not found. Set OSS_CAD_SUITE, or run:" >&2
    echo "  source ~/Apps/oss-cad-suite/environment" >&2
    exit 1
fi

exec make "$@"
