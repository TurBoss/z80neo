#!/bin/bash
# build.sh — build VoidVDP (z80neo VDP fork of VersaTerm) using the system pico-sdk.
#
# Usage:
#   ./build.sh              # RP2350 (Pico 2) -> build2350/
#   ./build.sh rp2350       # RP2350 (Pico 2) -> build2350/
#   ./build.sh rp2040       # RP2040 (Pico)   -> build/
set -e

TARGET="${1:-rp2350}"

case "$TARGET" in
	rp2040|pico)
		BUILD_DIR=build
		BOARD=pico
		PLATFORM=rp2040
		;;
	rp2350|pico2)
		BUILD_DIR=build2350
		BOARD=pico2
		PLATFORM=rp2350
		;;
	*)
		echo "unknown target '$TARGET' (use rp2040 or rp2350)" >&2
		exit 1
		;;
esac

cmake -B "$BUILD_DIR" -S . \
  -DPICO_SDK_PATH="$HOME/Dev/PICO/pico-sdk" \
  -DPICO_BOARD="$BOARD" \
  -DPICO_PLATFORM="$PLATFORM" \
  -DPICO_COPY_TO_RAM=1 \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_POLICY_VERSION_MINIMUM=4.3

cmake --build "$BUILD_DIR" --parallel $(nproc)
