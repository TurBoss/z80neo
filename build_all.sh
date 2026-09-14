#!/bin/bash
# build_all.sh — build firmware + OS, deploy to SD
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FW_DIR="${SCRIPT_DIR}/firmware"
OS_DIR="${SCRIPT_DIR}/software/Zeal/Zeal-8-bit-OS"
SD_HEX="${SCRIPT_DIR}/software/Zeal/sdcard/os_zeal.hex"
# udisks2/GNOME auto-mount point for the SD card (filesystem label Z80NEO)
SD_MOUNT="/run/media/turboss/Z80NEO"

echo "=== Build Firmware ==="
cd "${FW_DIR}"
rm -rf build
bash build.sh

echo "=== Flash Firmware ==="
bash upload.sh

echo "=== Build OS ==="
cd "${OS_DIR}"
. ./export.sh
bash build.sh

echo "=== Deploy to SD ==="
if [ -f "${SD_HEX}" ]; then
    # Mount by label if the desktop hasn't already auto-mounted it.
    if [ ! -d "${SD_MOUNT}" ]; then
        udisksctl mount -b /dev/disk/by-label/Z80NEO 2>/dev/null || true
        sleep 1
    fi
    if [ -d "${SD_MOUNT}" ]; then
        cp "${SD_HEX}" "${SD_MOUNT}/os_zeal.hex"
        sync
        umount "${SD_MOUNT}" 2>/dev/null || true
        echo "Deployed to ${SD_MOUNT}"
    else
        echo "SD not mounted at ${SD_MOUNT}, hex at: ${SD_HEX}"
    fi
else
    echo "Hex not found: ${SD_HEX}"
fi

echo "=== Done ==="
