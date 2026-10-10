#!/bin/bash
# sysgen.sh — CP/M "sysgen" for z80neo: rebuild the system image, rebuild the
# collection disk images, and install a chosen set onto the SD card.
#
# The z80neo does not boot CP/M from disk: the firmware loads cpmz80neo.hex
# directly.  "sysgen" here means (re)generate the CP/M system image and the
# CPMDISK*.IMG data disks the firmware emulates as drives A:..O:.
#
# Usage:
#   ./sysgen.sh system                 rebuild build/cpmz80neo.hex
#   ./sysgen.sh images                 rebuild software/cpm/CPM_IMAGES
#   ./sysgen.sh install A0 B0 C0 ...   install disks onto the SD card
#   ./sysgen.sh deploy --disks "A0 B0 ..."   system + images + install + hex
#   ./sysgen.sh all --disks "A0 B0 ..."      alias for deploy
#
# Options (apply to images/install/deploy/all):
#   --sd DIR          SD mount point (default: /run/media/$USER/Z80NEO)
#   --collection DIR  collection source (default: auto-detect)
#   --filter MODE     default | broad | all   (default: broad)
#   --disks "N N .."  disk names for install, in drive order (max 15)
#   --no-backup       skip backing up existing CPMDISK*.IMG before install
#
# Examples:
#   ./sysgen.sh deploy --disks "A0 B0 C0 D0 E0 F0 G0 H0 I0 J0 K0 L0 M0 N0 O1"
#   ./sysgen.sh images --filter default
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CPM_DIR="${SCRIPT_DIR}/software/cpm/z80neo"
OUT_DIR="${SCRIPT_DIR}/software/cpm/CPM_IMAGES"
HEX="${CPM_DIR}/build/cpmz80neo.hex"

SD_MOUNT_DEFAULT="/run/media/${USER}/Z80NEO"
SD_MOUNT=""
COLLECTION=""
FILTER="broad"
DISKS=""
BACKUP=1

# Broad set: everything a CP/M user is likely to want from the collection.
INCLUDE_DEFAULT=".com,.sub,.txt,.doc"
SKIP_DEFAULT=".pdf"
INCLUDE_BROAD=".com,.sub,.txt,.doc,.asm,.mac,.z80,.bas,.c,.h,.lib,.rel,.pas,.bbc,.ovr,.ovl,.def,.hlp"
SKIP_BROAD=".pdf,.zip"

die() { echo "error: $*" >&2; exit 1; }

find_collection() {
    local c
    if [ -d "${SCRIPT_DIR}/software/cpm/Collection/CPM_DISKS" ]; then
        echo "${SCRIPT_DIR}/software/cpm/Collection/CPM_DISKS"
        return
    fi
    # Fall back to an extracted drive-download under software/OLD.
    for c in "${SCRIPT_DIR}"/software/OLD/drive-download-*/; do
        [ -d "${c}A" ] || continue
        echo "${c%/}"
        return
    done
    die "collection source not found (looked for software/cpm/Collection/CPM_DISKS and software/OLD/drive-download-*)"
}

build_system() {
    echo "=== CP/M system image ==="
    [ -d "${CPM_DIR}" ] || die "missing ${CPM_DIR}"
    make -C "${CPM_DIR}" clean
    make -C "${CPM_DIR}"
    [ -f "${HEX}" ] || die "build produced no ${HEX}"
    echo "-> ${HEX}"
}

build_images() {
    local inc skip
    case "${FILTER}" in
        default) inc="${INCLUDE_DEFAULT}"; skip="${SKIP_DEFAULT}" ;;
        broad)   inc="${INCLUDE_BROAD}";   skip="${SKIP_BROAD}" ;;
        all)     inc="";                  skip="${SKIP_BROAD}" ;;
        *) die "unknown --filter ${FILTER}" ;;
    esac
    [ -n "${COLLECTION}" ] || COLLECTION="$(find_collection)"
    [ -d "${COLLECTION}" ] || die "collection not found: ${COLLECTION}"

    echo "=== CP/M disk images (filter=${FILTER}) ==="
    echo "collection: ${COLLECTION}"
    echo "output:     ${OUT_DIR}"
    if [ "${BACKUP}" = 1 ] && [ -d "${OUT_DIR}" ]; then
        local bak="${OUT_DIR}.bak.$(date +%Y%m%d_%H%M%S)"
        rm -rf "${bak}"
        mv "${OUT_DIR}" "${bak}"
        echo "backed up old images -> ${bak}"
    fi
    python3 "${SCRIPT_DIR}/tools/pack_cpm_collection.py" \
        --mode disk \
        --collection "${COLLECTION}" \
        --out "${OUT_DIR}" \
        --include-ext "${inc}" \
        --skip-ext "${skip}"
}

resolve_sd() {
    [ -n "${SD_MOUNT}" ] || SD_MOUNT="${SD_MOUNT_DEFAULT}"
    [ -d "${SD_MOUNT}" ] || die "SD not mounted at ${SD_MOUNT} (pass --sd DIR)"
}

install_disks() {
    [ -n "${DISKS}" ] || die "no disks given (pass --disks \"A0 B0 ...\")"
    resolve_sd
    echo "=== install disks -> ${SD_MOUNT} ==="
    if [ "${BACKUP}" = 1 ]; then
        local bak="${SCRIPT_DIR}/software/cpm/SDCARD_BACKUP_$(date +%Y%m%d_%H%M%S)"
        mkdir -p "${bak}"
        cp "${SD_MOUNT}"/CPMDISK*.IMG "${bak}"/ 2>/dev/null || true
        echo "backed up SD images -> ${bak}"
    fi
    # shellcheck disable=SC2086
    python3 "${SCRIPT_DIR}/tools/install_cpm_disks.py" \
        --src "${OUT_DIR}" --sd "${SD_MOUNT}" ${DISKS}
    sync
}

copy_hex() {
    resolve_sd
    [ -f "${HEX}" ] || die "system image missing; run './sysgen.sh system' first"
    cp "${HEX}" "${SD_MOUNT}/cpmz80neo.hex"
    sync
    echo "-> ${SD_MOUNT}/cpmz80neo.hex"
}

usage() {
    sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
}

CMD=""
while [ $# -gt 0 ]; do
    case "$1" in
        system|images|install|deploy|all) CMD="$1"; shift ;;
        --sd) SD_MOUNT="$2"; shift 2 ;;
        --collection) COLLECTION="$2"; shift 2 ;;
        --filter) FILTER="$2"; shift 2 ;;
        --disks) DISKS="$2"; shift 2 ;;
        --no-backup) BACKUP=0; shift ;;
        -h|--help) usage ;;
        -*) die "unknown option: $1" ;;
        *)
            # bare arguments after `install` are disk names
            [ "${CMD}" = install ] || die "unexpected argument: $1"
            DISKS="${DISKS} $1"; shift ;;
    esac
done

[ -n "${CMD}" ] || usage

case "${CMD}" in
    system)  build_system ;;
    images)  build_images ;;
    install) install_disks ;;
    deploy|all)
        build_system
        build_images
        install_disks
        copy_hex
        ;;
esac

echo "=== done ==="
