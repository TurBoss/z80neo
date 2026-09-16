#!/usr/bin/env python3
"""pack_cpm_collection.py — pack a CP/M disk collection into z80neo images.

Collection layout:  <collection>/<letter>/<number>/<files>  (letters A..O,
numbers 0..9 and A).

Modes:
  --mode disk    (default) one image per <letter>/<number> -> <letter><number>.IMG
  --mode letter  one image per letter, each number -> a CP/M user area
                 (0..9 and A=10).  Note letters A and C exceed one image and get
                 truncated in this mode.

Both skip PDFs, non-8.3 names and (optionally) anything outside --include-ext,
and pad each image out to the full z80neo geometry.

Usage:
    python pack_cpm_collection.py [--mode disk] [--include-ext .com,.sub,.txt,.doc]
        [--out DIR] [--collection DIR] [--diskdefs FILE] [--letters A-O]
"""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DEFAULT_COLLECTION = REPO / "software" / "cpm" / "Collection" / "CPM_DISKS"
DEFAULT_DISKDEFS = REPO / "software" / "cpm" / "z80neo" / "diskdefs"
DEFAULT_OUT = REPO / "software" / "cpm" / "Collection" / "CPM_IMAGES"
FORMAT = "z80neo"

# z80neo geometry (must match diskdefs / the BIOS DPB).
FULL_SIZE = 254 * 26 * 128

VALID = re.compile(
    r"^[A-Za-z0-9!#$%&'()+\-@^_{}~`]{1,8}(\.[A-Za-z0-9!#$%&'()+\-@^_{}~`]{1,3})?$"
)


def ext_set(text: str) -> set[str]:
    return {
        e if e.startswith(".") else "." + e
        for e in (s.strip().lower() for s in text.split(","))
        if e
    }


def run(args: list[str], cwd: Path) -> tuple[int, str]:
    proc = subprocess.run(args, cwd=str(cwd), capture_output=True, text=True)
    return proc.returncode, (proc.stderr or proc.stdout).strip()


def add_folder(
    img: Path, folder: Path, user: int, cwd: Path, skip: set[str], include: set[str] | None
) -> tuple[int, int]:
    added = skipped = 0
    for f in sorted(folder.iterdir()):
        if not f.is_file():
            continue
        if f.suffix.lower() in skip or not VALID.match(f.name):
            skipped += 1
            continue
        if include is not None and f.suffix.lower() not in include:
            skipped += 1
            continue
        rc, msg = run(
            ["cpmcp", "-f", FORMAT, str(img), str(f), f"{user}:{f.name.upper()}"], cwd
        )
        if rc:
            skipped += 1
            print(f"    skip {f.name}: {msg}")
        else:
            added += 1
    return added, skipped


def pad(img: Path) -> None:
    with img.open("r+b") as handle:
        handle.truncate(FULL_SIZE)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mode", choices=("disk", "letter"), default="disk")
    ap.add_argument("--collection", type=Path, default=DEFAULT_COLLECTION)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--diskdefs", type=Path, default=DEFAULT_DISKDEFS)
    ap.add_argument("--skip-ext", default=".pdf")
    ap.add_argument(
        "--include-ext",
        default=".com,.sub,.txt,.doc",
        help="only these extensions are packed (empty = everything not skipped)",
    )
    ap.add_argument("--letters", default="ABCDEFGHIJKLMNO")
    args = ap.parse_args()

    cwd = args.diskdefs.parent
    args.out.mkdir(parents=True, exist_ok=True)
    skip = ext_set(args.skip_ext)
    include = ext_set(args.include_ext) or None

    total_added = total_skipped = images = 0
    for letter in args.letters:
        ldir = args.collection / letter
        if not ldir.is_dir():
            print(f"{letter}: no folder, skipping")
            continue

        if args.mode == "disk":
            for num in sorted(ldir.iterdir()):
                if not num.is_dir():
                    continue
                img = args.out / f"{letter}{num.name}.IMG"
                rc, msg = run(["mkfs.cpm", "-f", FORMAT, str(img)], cwd)
                if rc:
                    print(f"{letter}{num.name}: mkfs failed: {msg}")
                    continue
                added, skipped = add_folder(img, num, 0, cwd, skip, include)
                pad(img)
                total_added += added
                total_skipped += skipped
                images += 1
                print(f"{letter}{num.name} -> {img.name}: {added} files, {skipped} skipped")
        else:  # letter mode: number -> user area
            img = args.out / f"CPMDISK{args.letters.index(letter)}.IMG"
            rc, msg = run(["mkfs.cpm", "-f", FORMAT, str(img)], cwd)
            if rc:
                print(f"{letter}: mkfs failed: {msg}")
                continue
            added = skipped = 0
            for num in sorted(ldir.iterdir()):
                if not num.is_dir():
                    continue
                try:
                    user = int(num.name, 16)
                except ValueError:
                    continue
                a, s = add_folder(img, num, user, cwd, skip, include)
                added += a
                skipped += s
            pad(img)
            total_added += added
            total_skipped += skipped
            images += 1
            print(f"{letter} -> {img.name}: {added} files, {skipped} skipped")

    print(f"done: {images} images, {total_added} files added, {total_skipped} skipped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
