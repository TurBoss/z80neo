#!/bin/bash
# upload.sh — flash VoidVDP (z80neo VDP fork of VersaTerm) over a CMSIS-DAP debug probe.
#
# Usage:
#   ./upload.sh                       # RP2350: build2350/src/VoidVDP.elf, target/rp2350.cfg
#   ./upload.sh rp2350                # same as above
#   ./upload.sh rp2040                # RP2040: build/src/VoidVDP.elf, target/rp2040.cfg
#   ./upload.sh path/to/other.elf     # explicit ELF (board from TGT, default rp2350)
#   IF=interface/picoprobe.cfg ./upload.sh
#   TGT=target/rp2040.cfg SPEED=2000 ./upload.sh build/src/VoidVDP.elf
#
set -e

ELF="${ELF:-}"
BOARD=rp2350
for arg in "$@"; do
	case "$arg" in
		rp2040|pico)  BOARD=rp2040 ;;
		rp2350|pico2) BOARD=rp2350 ;;
		*)            ELF="$arg" ;;
	esac
done

case "$BOARD" in
	rp2040) DEFAULT_ELF=build/src/VoidVDP.elf;    DEFAULT_TARGET=target/rp2040.cfg ;;
	rp2350) DEFAULT_ELF=build2350/src/VoidVDP.elf; DEFAULT_TARGET=target/rp2350.cfg ;;
esac

ELF="${ELF:-$DEFAULT_ELF}"
IFACE="${IF:-interface/cmsis-dap.cfg}"
TARGET="${TGT:-$DEFAULT_TARGET}"
SPEED="${SPEED:-5000}"

if [ ! -f "$ELF" ]; then
    echo "ELF not found at $ELF" >&2
    echo "Build it first:  ./build.sh $BOARD" >&2
    exit 1
fi

if ! command -v openocd >/dev/null 2>&1; then
    echo "openocd not found in PATH" >&2
    exit 1
fi

echo "Flashing $ELF"
echo "  interface: $IFACE"
echo "  target:    $TARGET"
echo "  speed:     $SPEED kHz"

exec openocd \
    -f "$IFACE" \
    -f "$TARGET" \
    -c "adapter speed $SPEED" \
    -c "program $ELF verify reset exit"
