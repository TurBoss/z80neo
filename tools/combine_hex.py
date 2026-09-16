#!/usr/bin/env python3
"""Combine bootloader binary + OS kernel (with romdisk) into a single Intel HEX file for z80neo.

Memory layout after loading:
  Bank 0: Bootloader code (0x0000-0x3FFF replicated) + LDIR'd kernel (from bank 1)
  Bank 1: Kernel code + padding (image bytes 0x0000-0x3FFF, at hex addr 0x4000)
  Bank 2: Romdisk data (image bytes 0x4000+, at hex addr 0x8000)
  Bank 3: Trampoline at 0xE000, kernel BSS at 0xC000

The z80neo hex loader (load_file) routes addresses as:
  - abs_addr < 0x4000  → replicated to ALL banks (ram[0..3])
  - abs_addr >= 0x4000 → page = (addr >> 14) & 3 → ram[page]
"""

import os
import sys


def bin_to_hex(data, base_addr=0, skip_zeros=False):
    """Convert binary data to Intel HEX format lines."""
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i : i + 16]
        if skip_zeros and not any(b != 0 for b in chunk):
            continue
        addr = base_addr + i
        if addr > 0xFFFF:  # Don't wrap past 64KB
            break
        byte_count = len(chunk)
        checksum = byte_count + (addr >> 8) + (addr & 0xFF) + sum(chunk)
        checksum = (-checksum) & 0xFF
        hex_data = "".join(f"{b:02X}" for b in chunk)
        lines.append(f":{byte_count:02X}{addr:04X}00{hex_data}{checksum:02X}")
    return lines


def main():
    project_root = "/home/turboss/Dev/PICO/z80neo"
    bootloader_path = os.path.join(
        project_root, "software/Zeal-Bootloader/build/bootloader.bin"
    )
    kernel_path = os.path.join(project_root, "software/Zeal-8-bit-OS/build/os.bin")
    romdisk_img_path = os.path.join(
        project_root, "software/Zeal-8-bit-OS/build/os_with_romdisk.img"
    )
    output_path = os.path.join(project_root, "sdcard/os_zeal.hex")

    # Read bootloader binary
    with open(bootloader_path, "rb") as f:
        bootloader = f.read()
    print(f"Bootloader: {len(bootloader)} bytes from {bootloader_path}")

    # Read kernel binary (code only, no romdisk)
    with open(kernel_path, "rb") as f:
        kernel = f.read()
    print(f"Kernel:     {len(kernel)} bytes from {kernel_path}")

    # Read full image with romdisk
    romdisk_img = None
    if os.path.exists(romdisk_img_path):
        with open(romdisk_img_path, "rb") as f:
            romdisk_img = f.read()
        print(f"Romdisk img:{len(romdisk_img)} bytes from {romdisk_img_path}")
        # Romdisk starts at offset 0x4000 in the image (after 16KB kernel+padding)
        romdisk_data = romdisk_img[0x4000:]
        print(f"Romdisk:    {len(romdisk_data)} bytes (offset 0x4000 in image)")
    else:
        print("WARNING: No romdisk image found, kernel will fail to boot")

    # Find end of bootloader code (below 0x4000)
    boot_end = 0
    for i in range(0x3FFF, -1, -1):
        if i < len(bootloader) and bootloader[i] != 0:
            boot_end = i + 1
            break
    print(f"Bootloader code ends at offset 0x{boot_end:04X}")

    # Trampoline range
    tramp_start = 0xE000
    tramp_end = tramp_start
    if tramp_start < len(bootloader):
        for i in range(tramp_start, min(tramp_start + 256, len(bootloader))):
            if bootloader[i] != 0:
                tramp_end = i + 1
    print(f"Trampoline at 0x{tramp_start:04X} - 0x{tramp_end:04X}")

    lines = []

    # 1. Bootloader code at 0x0000 (replicated to all banks)
    bl_chunk = bootloader[: min(boot_end, 0x4000)]
    lines.extend(bin_to_hex(bl_chunk, 0x0000, skip_zeros=True))

    # 2. Kernel code + padding at 0x4000 (goes to bank 1)
    #    Include padding to fill 16KB so romdisk aligns to bank 2 start
    KERNEL_PAGE_SIZE = 0x4000  # 16KB
    kernel_padded = kernel[:KERNEL_PAGE_SIZE].ljust(KERNEL_PAGE_SIZE, b"\x00")
    lines.extend(bin_to_hex(kernel_padded, 0x4000, skip_zeros=True))

    # 3. Romdisk at 0x8000 (goes to bank 2, aligned to page start)
    if romdisk_data and len(romdisk_data) > 0:
        # Truncate to fit in one bank (16KB)
        romdisk_chunk = romdisk_data[:KERNEL_PAGE_SIZE]
        lines.extend(bin_to_hex(romdisk_chunk, 0x8000, skip_zeros=True))
        print(f"Romdisk loaded at hex 0x8000 ({len(romdisk_chunk)} bytes in bank 2)")

    # 4. Trampoline at 0xE000 (goes to bank 3)
    if tramp_end > tramp_start:
        tramp_data = bootloader[tramp_start:tramp_end]
        lines.extend(bin_to_hex(tramp_data, tramp_start))
        print(f"Trampoline bytes: {' '.join(f'{b:02X}' for b in tramp_data)}")

    # 5. End record
    lines.append(":00000001FF")

    with open(output_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Output: {output_path} ({len(lines)} lines)")


if __name__ == "__main__":
    main()
