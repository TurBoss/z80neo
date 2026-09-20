#!/bin/sh
# Copy the BBC BASIC demo listings onto a z80neo CP/M disk image.
#
#   ./install.sh                 # installs onto ../../sdcard/CPMDISK0.IMG (A:)
#   ./install.sh /path/to.IMG    # installs onto a different image
#
# The demos are plain ASCII listings; on the Z80 they are loaded with
# *EXEC (not LOAD, which expects a tokenised program).  BBCBASIC.COM must be
# on the same disk -- it already ships on CPMDISK0.IMG.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
DISKDEFS="$REPO/software/cpm/z80neo"
IMAGE=${1:-$REPO/sdcard/CPMDISK0.IMG}

if [ ! -f "$IMAGE" ]; then
	echo "disk image not found: $IMAGE" >&2
	exit 1
fi

# cpmtools looks for a file literally named "diskdefs" in the cwd.
cd "$DISKDEFS"

for src in "$HERE"/*.bas; do
	[ -e "$src" ] || continue
	name=$(basename "$src" .bas | tr '[:lower:]' '[:upper:]')
	printf '  %s.BBC\n' "$name"
	cpmrm -f z80neo "$IMAGE" "0:$name.BBC" 2>/dev/null || true
	# -t translates LF to CP/M CRLF, which is what *EXEC needs.
	cpmcp -f z80neo -t "$IMAGE" "$src" "0:$name.BBC"
done

echo "installed demos into $IMAGE"
