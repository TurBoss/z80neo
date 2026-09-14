// cpm_disk.c — CP/M disk emulation (multi-drive)
// z80neo — TurBoss 2026
#include "cpm_disk.h"
#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include "ff.h"
#include "memory.h"

#define CMD_IDLE   0x00
#define CMD_READ   0x10
#define CMD_WRITE  0x11

typedef struct {
    FIL      file;
    bool     ready;
    uint32_t img_size;
    uint8_t  track;
    uint8_t  sector;
    uint8_t  buf[CPM_SEC_SIZE];
    uint8_t  idx;
    uint8_t  cmd;
    uint8_t  error;
} cpm_drive_t;

static FATFS       cpm_fs;
static cpm_drive_t cpm[CPM_DRIVES];
static uint8_t     cpm_cur = 0;
static uint8_t     cpm_sectors_per_track = CPM_SECTORS;

// Images are CPMDISK0.IMG (A:) .. CPMDISK3.IMG (D:).
static void drive_name(uint8_t d, char *out, size_t n) {
    snprintf(out, n, "CPMDISK%u.IMG", (unsigned)d);
}

static uint32_t sector_to_offset(uint8_t track, uint8_t sector) {
    return ((uint32_t)track * cpm_sectors_per_track + sector) * CPM_SEC_SIZE;
}

static bool seek_to_sector(cpm_drive_t *dr, uint8_t track, uint8_t sector) {
    uint32_t off = sector_to_offset(track, sector);
    if (off >= dr->img_size) { dr->error = 1; return false; }
    if (f_lseek(&dr->file, off) != FR_OK) { dr->error = 1; return false; }
    dr->error = 0;
    return true;
}

bool cpm_disk_init(void) {
    FRESULT fr;
    FILINFO fi;
    uint8_t mounted = 0;

    fr = f_mount(&cpm_fs, "0:", 1);
    if (fr != FR_OK) return false;

    for (uint8_t d = 0; d < CPM_DRIVES; d++) {
        cpm_drive_t *dr = &cpm[d];
        char name[16];
        memset(dr, 0, sizeof(*dr));
        drive_name(d, name, sizeof(name));
        if (f_stat(name, &fi) != FR_OK) continue;
        if (fi.fsize == 0 || fi.fsize > CPM_IMAGE_MAX_SIZE) continue;
        fr = f_open(&dr->file, name, FA_READ | FA_WRITE);
        if (fr != FR_OK) fr = f_open(&dr->file, name, FA_READ);
        if (fr != FR_OK) continue;
        dr->img_size = fi.fsize;
        dr->ready    = true;
        dr->cmd      = CMD_IDLE;
        mounted++;
    }
    cpm_cur = 0;
    return mounted > 0;
}

uint8_t cpm_disk_count(void) {
    uint8_t n = 0;
    for (uint8_t d = 0; d < CPM_DRIVES; d++)
        if (cpm[d].ready) n++;
    return n;
}

void cpm_disk_state(uint8_t *drive, uint8_t *track, uint8_t *sector) {
    if (drive)  *drive  = cpm_cur;
    if (track)  *track  = cpm[cpm_cur].track;
    if (sector) *sector = cpm[cpm_cur].sector;
}

void cpm_disk_write_port(uint8_t port, uint8_t data) {
    if (port == CPM_DRIVE_PORT) {
        if (data < CPM_DRIVES) cpm_cur = data;
        return;
    }

    cpm_drive_t *dr = &cpm[cpm_cur];
    if (!dr->ready) return;

    switch (port) {
    case 0xE1: dr->track = data; break;
    case 0xE2: dr->sector = data; break;
    case 0xE0:
        if (dr->cmd == CMD_IDLE) {
            if (data == 0x00) {
                dr->cmd = CMD_READ; dr->idx = 0;
                if (seek_to_sector(dr, dr->track, dr->sector)) {
                    UINT br;
                    FRESULT r = f_read(&dr->file, dr->buf, CPM_SEC_SIZE, &br);
                    dr->error = (r == FR_OK && br == CPM_SEC_SIZE) ? 0 : 1;
                }
            } else if (data == 0x01) {
                dr->cmd = CMD_WRITE; dr->idx = 0; dr->error = 0;
            }
        } else if (dr->cmd == CMD_WRITE) {
            if (dr->idx < CPM_SEC_SIZE) dr->buf[dr->idx++] = data;
            if (dr->idx >= CPM_SEC_SIZE) {
                if (seek_to_sector(dr, dr->track, dr->sector)) {
                    UINT bw; FRESULT r = f_write(&dr->file, dr->buf, CPM_SEC_SIZE, &bw);
                    dr->error = (r == FR_OK && bw == CPM_SEC_SIZE) ? 0 : 1;
                    if (!dr->error) f_sync(&dr->file);
                } else { dr->error = 1; }
                dr->cmd = CMD_IDLE;
            }
        }
        break;
    }
}

uint8_t cpm_disk_read_port(uint8_t port) {
    cpm_drive_t *dr = &cpm[cpm_cur];
    if (!dr->ready) return 0xFF;
    if (port == 0xE2) return dr->error;
    if (port == 0xE0) {
        if (dr->cmd == CMD_READ && dr->idx < CPM_SEC_SIZE) {
            uint8_t b = dr->buf[dr->idx++];
            if (dr->idx >= CPM_SEC_SIZE) dr->cmd = CMD_IDLE;
            return b;
        }
        return 0;
    }
    return 0;
}
