#!/usr/bin/env python3
"""install_cpm_disks.py — copy chosen collection disk images onto the SD card
as CPMDISK0..14.IMG, the names the firmware expects for drives A:..O:.

Usage:
    python install_cpm_disks.py --list                 # show available disks
    python install_cpm_disks.py A0 A1 B0 C3 ...        # install (max 15)
    python install_cpm_disks.py A0 A1 --src DIR --sd DIR

The i-th name becomes CPMDISK<i>.IMG, i.e. drive A + i.  Stale CPMDISK<i>.IMG
beyond the given list are removed so old drives don't linger.
"""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DEFAULT_SRC = REPO / "software" / "cpm" / "Collection" / "CPM_IMAGES"
DEFAULT_SD = REPO / "sdcard"
DRIVES = 15  # must match CPM_DRIVES in cpm_disk.h / DISK_DRIVES in z80neo.inc


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("disks", nargs="*", help="disk names, e.g. A0 A1 B0")
    ap.add_argument("--src", type=Path, default=DEFAULT_SRC)
    ap.add_argument("--sd", type=Path, default=DEFAULT_SD)
    ap.add_argument("--list", action="store_true", help="list available disks")
    args = ap.parse_args()

    if args.list or not args.disks:
        names = sorted(p.stem for p in args.src.glob("*.IMG"))
        print(f"{len(names)} disks in {args.src}:")
        print("  " + " ".join(names))
        return 0

    if len(args.disks) > DRIVES:
        print(f"at most {DRIVES} disks (got {len(args.disks)})")
        return 1

    for i, name in enumerate(args.disks):
        src = args.src / f"{name.upper()}.IMG"
        if not src.exists():
            print(f"missing: {src}")
            continue
        dst = args.sd / f"CPMDISK{i}.IMG"
        shutil.copy(src, dst)
        print(f"drive {chr(65 + i)}: {dst.name} <- {src.name}")

    for i in range(len(args.disks), DRIVES):
        stale = args.sd / f"CPMDISK{i}.IMG"
        if stale.exists():
            stale.unlink()
            print(f"removed stale {stale.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
