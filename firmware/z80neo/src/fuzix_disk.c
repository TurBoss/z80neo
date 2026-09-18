// fuzix_disk.c — raw 512-byte LBA block device for Fuzix
//
// Exposes FUZIX.IMG on the SD card as a flat block device.  The Z80 side
// sets a 24-bit LBA on ports 0xE4-0xE6, starts a transfer with a command on
// 0xE7 and streams 512 bytes through 0xE0.  A read is serviced when the
// command is issued; a write commits on the 512th data byte.
//
// Port map (only active when fuzix_mode is set):
//   0xE0  W/R  data
//   0xE4  W    LBA bits 0-7
//   0xE5  W    LBA bits 8-15
//   0xE6  W    LBA bits 16-23
//   0xE7  W    command: 0x01 = read 512, 0x02 = write 512
//   0xE7  R    status:  0x00 = ok, 0xFF = no device / error
//
// z80neo — TurBoss 2026

#include "fuzix_disk.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>

#include "ff.h"

#define FZ_SEC_SIZE   512
#define FZ_CMD_IDLE   0x00
#define FZ_CMD_READ   0x01
#define FZ_CMD_WRITE  0x02

static FATFS   fz_fs;
static FIL     fz_file;
static bool    fz_ready = false;
static uint8_t fz_buf[FZ_SEC_SIZE];
static uint32_t fz_lba = 0;
static uint16_t fz_idx = 0;
static uint8_t fz_cmd = FZ_CMD_IDLE;
static uint8_t fz_status = 0xFF;

bool fuzix_disk_init(void) {
    fz_ready = false;
    if (f_mount(&fz_fs, "0:", 1) != FR_OK) return false;
    if (f_open(&fz_file, "FUZIX.IMG", FA_READ | FA_WRITE) != FR_OK) return false;
    fz_lba = 0;
    fz_idx = 0;
    fz_cmd = FZ_CMD_IDLE;
    fz_status = 0x00;
    fz_ready = true;
    return true;
}

static bool fz_seek(void) {
    FSIZE_t off = (FSIZE_t)fz_lba * FZ_SEC_SIZE;
    return f_lseek(&fz_file, off) == FR_OK;
}

void fuzix_disk_write_port(uint8_t port, uint8_t data) {
    if (!fz_ready) { fz_status = 0xFF; return; }

    switch (port) {
    case 0xE4: fz_lba = (fz_lba & 0xFFFF00UL) | data; break;
    case 0xE5: fz_lba = (fz_lba & 0xFF00FFUL) | ((uint32_t)data << 8); break;
    case 0xE6: fz_lba = (fz_lba & 0x00FFFFUL) | ((uint32_t)data << 16); break;

    case 0xE7:
        fz_idx = 0;
        if (data == FZ_CMD_READ) {
            UINT br = 0;
            if (!fz_seek() ||
                f_read(&fz_file, fz_buf, FZ_SEC_SIZE, &br) != FR_OK ||
                br != FZ_SEC_SIZE) {
                fz_status = 0xFF;
                fz_cmd = FZ_CMD_IDLE;
            } else {
                fz_status = 0x00;
                fz_cmd = FZ_CMD_READ;
            }
        } else if (data == FZ_CMD_WRITE) {
            fz_status = 0x00;
            fz_cmd = FZ_CMD_WRITE;
        } else {
            fz_cmd = FZ_CMD_IDLE;
        }
        break;

    case 0xE0:
        if (fz_cmd == FZ_CMD_WRITE) {
            if (fz_idx < FZ_SEC_SIZE) fz_buf[fz_idx++] = data;
            if (fz_idx >= FZ_SEC_SIZE) {
                UINT bw = 0;
                if (!fz_seek() ||
                    f_write(&fz_file, fz_buf, FZ_SEC_SIZE, &bw) != FR_OK ||
                    bw != FZ_SEC_SIZE) {
                    fz_status = 0xFF;
                } else {
                    f_sync(&fz_file);
                    fz_status = 0x00;
                }
                fz_cmd = FZ_CMD_IDLE;
            }
        }
        break;

    default:
        break;
    }
}

uint8_t fuzix_disk_read_port(uint8_t port) {
    if (!fz_ready && port == 0xE7) return 0xFF;
    switch (port) {
    case 0xE0:
        if (fz_cmd == FZ_CMD_READ && fz_idx < FZ_SEC_SIZE)
            return fz_buf[fz_idx++];
        return 0x00;
    case 0xE7:
        return fz_status;
    default:
        return 0x00;
    }
}
