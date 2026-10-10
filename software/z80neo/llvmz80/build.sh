#!/usr/bin/env bash
#
# build.sh - build the z80neo LLVM-Z80 example.
#
# Sets the z88dk environment (PATH, ZCCCFG) and LLVMZ80EXE, then runs make.
# Any arguments are passed through to make, e.g.:
#
#   ./build.sh                 # -> build/vdpdemo.ihx
#   ./build.sh bin             # -> build/vdpdemo (raw binary)
#   ./build.sh OPT=-O0         # debug build
#   ./build.sh clean
#
# Override the toolchain locations from the environment:
#
#   Z88DK=/path/to/z88dk LLVMZ80EXE=/path/to/clang ./build.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# --- z88dk ---------------------------------------------------------------
Z88DK="${Z88DK:-$(cd "$SCRIPT_DIR/../../z88dk" && pwd)}"
export ZCCCFG="$Z88DK/lib/config"
export PATH="$Z88DK/bin:$PATH"

if [ ! -x "$Z88DK/bin/zcc" ]; then
	echo "error: zcc not found at $Z88DK/bin/zcc (set Z88DK=...)" >&2
	exit 1
fi

# --- llvm-z80 clang ------------------------------------------------------
# `-compiler=llvmz80` needs a clang with the `z80-unknown-none-z88dk` target
# and the z88dk classic-library ABI (ravn/llvm-z80, branch `z88dk`).  Accept
# LLVMZ80EXE from the environment, otherwise probe the usual locations and
# confirm each candidate actually advertises a Z80 target.
find_llvmz80() {
	local cand
	for cand in \
		"${LLVMZ80EXE:-}" \
		"$(command -v llvmz80-clang 2>/dev/null || true)" \
		"$HOME/Dev/llvm-z80/build/bin/clang"
	do
		[ -n "$cand" ] || continue
		if [ -x "$cand" ] && "$cand" --print-targets 2>/dev/null | grep -qi 'z80'; then
			printf '%s\n' "$cand"
			return 0
		fi
	done
	return 1
}

if ! LLVMZ80EXE="$(find_llvmz80)"; then
	cat >&2 <<'EOF'
error: no llvm-z80 clang found.

Build one (ravn/llvm-z80, branch `z88dk`) with the commands in README.md,
then either put `llvmz80-clang` on PATH or set:

    LLVMZ80EXE=/path/to/llvm-z80/build/bin/clang ./build.sh
EOF
	exit 1
fi
export LLVMZ80EXE

echo "z88dk    : $Z88DK"
echo "llvm-z80 : $LLVMZ80EXE"

# --- build ---------------------------------------------------------------
exec make -C "$SCRIPT_DIR" "$@"
