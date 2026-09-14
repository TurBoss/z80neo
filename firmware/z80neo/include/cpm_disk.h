// cpm_disk.h — CP/M disk emulation (async version)
// z80neo — TurBoss 2026
#ifndef CPM_DISK_H
#define CPM_DISK_H

#include <stdbool.h>
#include <stdint.h>

#define CPM_TRACKS         254
#define CPM_SECTORS        26
#define CPM_SEC_SIZE       128
#define CPM_IMAGE_MAX_SIZE (8UL * 1024 * 1024)

// Number of emulated drives (A:..O:).  Must match DISK_DRIVES in z80neo.inc
// and the DPH count in disk.asm.
#define CPM_DRIVES         15
// Write the drive number (0=A) here to select the active image.
#define CPM_DRIVE_PORT     0xE3

bool cpm_disk_init(void);
uint8_t cpm_disk_count(void);
void cpm_disk_state(uint8_t *drive, uint8_t *track, uint8_t *sector);
void cpm_disk_write_port(uint8_t port, uint8_t data);
uint8_t cpm_disk_read_port(uint8_t port);
void cpm_disk_poll(void);    // call from main loop to process pending I/O

#endif
