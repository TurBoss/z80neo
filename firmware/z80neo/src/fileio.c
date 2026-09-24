// fileio.c — Z80 SD-card file API (ports 0xA0-0xA7)
// z80neo — TurBoss 2026
//
// The Pico owns the SD card; the Z80 gets a single buffered read stream.  See
// include/fileio.h for the register map.  Kept deliberately small: one open
// file, one directory scan, no writes.

#include "fileio.h"
#include "cpm_disk.h"
#include "fuzix_disk.h"
#include "midi.h"

#include <string.h>
#include <pico/stdlib.h>

#include "ff.h"

#define FILEIO_BUF_SIZE 512

// This FatFS build exposes AM_DIR/AM_ARC but keeps the volume-label bit
// internal, so define it here to skip that entry in a directory listing.
#ifndef AM_VOL
#define AM_VOL 0x08
#endif

static FATFS   fio_fs;
static FIL     fio_fil;
static DIR     fio_dir;
static FILINFO fio_fno;

static bool     fio_mounted;
static bool     fio_open;
static bool     fio_dir_open;
static bool     fio_err;
static bool     fio_eof;
static bool     fio_midi;
static bool     fio_latch_valid;
static uint8_t  fio_latch;
static uint8_t  fio_buf[FILEIO_BUF_SIZE];
static uint16_t fio_buf_len;
static uint16_t fio_buf_pos;

static char     fio_name[FF_LFN_BUF + 1];
static uint16_t fio_name_len;
static uint16_t fio_name_rd;
static bool     fio_name_avail;
static bool     fio_name_is_dir;   // current DIRNEXT entry is a directory

static uint8_t  fio_arg[4];

bool fileio_ensure_mount(void) {
    if (fio_mounted) return true;
    if (f_mount(&fio_fs, "", 1) != FR_OK) {
        fio_err = true;
        return false;
    }
    fio_mounted = true;
    return true;
}

// Make the latch hold the next byte if one can be read; clears it and sets EOF
// when the file is exhausted (or on error).
static void fio_fill(void) {
    if (fio_latch_valid) return;
    if (!fio_open) { fio_eof = true; return; }

    if (fio_buf_pos >= fio_buf_len) {
        UINT br = 0;
        if (f_read(&fio_fil, fio_buf, sizeof fio_buf, &br) != FR_OK) {
            fio_err = true;
            fio_eof = true;
            return;
        }
        if (br == 0) { fio_eof = true; return; }
        fio_buf_len = (uint16_t)br;
        fio_buf_pos = 0;
    }

    fio_latch = fio_buf[fio_buf_pos++];
    fio_latch_valid = true;
}

// Drop buffered data after a seek so the next fill comes from the new offset.
static void fio_reset_stream(void) {
    fio_buf_len = 0;
    fio_buf_pos = 0;
    fio_latch_valid = false;
    fio_eof = false;
    fio_err = false;
}

static void fio_close_file(void) {
    if (fio_open) {
        f_close(&fio_fil);
        fio_open = false;
    }
    fio_reset_stream();
}

static void fio_close_dir(void) {
    if (fio_dir_open) {
        f_closedir(&fio_dir);
        fio_dir_open = false;
    }
}

uint8_t fileio_status_read(void) {
    if (fio_midi) {
        // The record stream ends with a 0xFF sentinel, so READY is enough.
        uint8_t s = FILE_ST_OPEN;
        if (fio_err) s |= FILE_ST_ERR;
        return s;
    }
    fio_fill();
    uint8_t s = 0;
    if (fio_latch_valid)   s |= FILE_ST_READY;
    if (fio_eof)           s |= FILE_ST_EOF;
    if (fio_err)           s |= FILE_ST_ERR;
    if (fio_open)          s |= FILE_ST_OPEN;
    if (fio_name_avail)    s |= FILE_ST_NAME;
    if (fio_name_avail && fio_name_is_dir) s |= FILE_ST_DIR;
    if (fio_name_len >= FF_LFN_BUF) s |= FILE_ST_NAMEFULL;
    return s;
}

uint8_t fileio_data_read(void) {
    if (fio_midi)
        return midi_read_byte();
    fio_fill();
    if (!fio_latch_valid) return 0;
    uint8_t v = fio_latch;
    fio_latch_valid = false;
    fio_fill();                 // prime the following byte for the next read
    return v;
}

void fileio_name_write(uint8_t c) {
    if (fio_name_len < FF_LFN_BUF)
        fio_name[fio_name_len++] = (char)c;
    fio_name[fio_name_len] = '\0';
}

// Successive name bytes for DIRNEXT; returns 0 past the end of the name.
uint8_t fileio_name_read(void) {
    if (fio_name_rd < fio_name_len)
        return (uint8_t)fio_name[fio_name_rd++];
    return 0;
}

void fileio_name_reset(void) {
    fio_name_len = 0;
    fio_name_rd = 0;
    fio_name[0] = '\0';
    fio_name_avail = false;
    fio_name_is_dir = false;
}

void fileio_arg_write(uint8_t index, uint8_t data) {
    if (index < 4) fio_arg[index] = data;
}

uint8_t fileio_arg_read(uint8_t index) {
    return index < 4 ? fio_arg[index] : 0;
}

static uint32_t fio_arg_u32(void) {
    return (uint32_t)fio_arg[0] | ((uint32_t)fio_arg[1] << 8) |
           ((uint32_t)fio_arg[2] << 16) | ((uint32_t)fio_arg[3] << 24);
}

void fileio_cmd_write(uint8_t cmd) {
    if (fio_midi && cmd != FILE_OPEN_MIDI && cmd != FILE_CLOSE) {
        midi_close();
        fio_midi = false;
    }
    switch (cmd) {
    case FILE_OPEN_MIDI:
        fio_close_file();
        fio_close_dir();
        fio_err = false;
        fio_name[fio_name_len] = '\0';
        fio_midi = midi_open(fio_name);
        if (!fio_midi) fio_err = true;
        break;

    case FILE_REMOUNT_CPM:
        // `fio_fs` and `cpm_fs` share the same logical SD volume, so mounting
        // ours (any file/dir access) displaces the CP/M disk layer's mount and
        // invalidates its open image handles.  Drop our cached mount and re-open
        // the emulated disk images so disk I/O works again once the program that
        // borrowed the volume (the CP/M MIDI player) is done with it.
        fio_close_file();
        fio_close_dir();
        fio_mounted = false;
        cpm_disk_init();
        break;

    case FILE_REMOUNT_FUZIX:
        // Same story for the Fuzix raw block device: a Fuzix program that used
        // the file API must hand the volume back by re-opening FUZIX.IMG, or
        // the kernel's handle (fz_file) can go stale and filesystem I/O fails.
        fio_close_file();
        fio_close_dir();
        fio_mounted = false;
        fuzix_disk_init();
        break;

    case FILE_OPEN: {
        fio_close_file();
        fio_err = false;
        if (!fileio_ensure_mount()) break;
        fio_name[fio_name_len] = '\0';
        if (fio_name_len == 0) { fio_err = true; break; }
        if (f_open(&fio_fil, fio_name, FA_READ) != FR_OK) { fio_err = true; break; }
        fio_open = true;
        fio_name_avail = false;
        fio_fill();
        break;
    }

    case FILE_CLOSE:
        if (fio_midi) {
            midi_close();
            fio_midi = false;
        }
        fio_close_file();
        fio_name_avail = false;
        break;

    case FILE_REWIND:
        if (fio_open && f_lseek(&fio_fil, 0) == FR_OK) {
            fio_reset_stream();
            fio_fill();
        } else {
            fio_err = true;
        }
        break;

    case FILE_SIZE: {
        if (!fio_open) { fio_err = true; break; }
        uint32_t sz = (uint32_t)f_size(&fio_fil);
        fio_arg[0] = (uint8_t)sz;
        fio_arg[1] = (uint8_t)(sz >> 8);
        fio_arg[2] = (uint8_t)(sz >> 16);
        fio_arg[3] = (uint8_t)(sz >> 24);
        break;
    }

    case FILE_SEEK:
        if (fio_open && f_lseek(&fio_fil, fio_arg_u32()) == FR_OK) {
            fio_reset_stream();
            fio_fill();
        } else {
            fio_err = true;
        }
        break;

    case FILE_DIRRESET:
        fio_close_dir();
        fio_name_avail = false;
        fio_err = false;
        fio_eof = false;
        if (!fileio_ensure_mount()) break;
        // The name buffer holds the directory to scan (empty = root), so a
        // program can list e.g. "/MIDI".
        fio_name[fio_name_len] = '\0';
        if (f_opendir(&fio_dir, fio_name_len ? fio_name : "/") != FR_OK) {
            fio_err = true;
            break;
        }
        fio_dir_open = true;
        break;

    case FILE_DIRNEXT:
        if (!fio_dir_open) { fio_err = true; fio_name_avail = false; break; }
        for (;;) {
            if (f_readdir(&fio_dir, &fio_fno) != FR_OK || fio_fno.fname[0] == '\0') {
                fio_name_avail = false;
                fio_eof = true;
                break;
            }
            // Directories are returned too (FILE_ST_DIR tells them apart) so a
            // loader can walk a tree; only the volume label is skipped.
            if (fio_fno.fattrib & AM_VOL) continue;
            strncpy(fio_name, fio_fno.fname, FF_LFN_BUF);
            fio_name[FF_LFN_BUF] = '\0';
            fio_name_len = (uint16_t)strlen(fio_name);
            fio_name_rd = 0;
            fio_name_avail = true;
            fio_name_is_dir = (fio_fno.fattrib & AM_DIR) != 0;
            fio_eof = false;
            break;
        }
        break;

    default:
        break;
    }
}

void fileio_reset(void) {
    if (fio_midi) {
        midi_close();
        fio_midi = false;
    }
    fio_close_file();
    fio_close_dir();
    fileio_name_reset();
    fio_mounted = false;
    fio_err = false;
    fio_eof = false;
}
