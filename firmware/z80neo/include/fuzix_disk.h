// fuzix_disk.h — raw 512-byte LBA block device for Fuzix
// z80neo — TurBoss 2026
#ifndef FUZIX_DISK_H
#define FUZIX_DISK_H

#include <stdbool.h>
#include <stdint.h>

// Open FUZIX.IMG and prepare the raw LBA interface.  Returns true on success.
bool fuzix_disk_init(void);

// Port handlers, called from the IORQ path when fuzix_mode is set.
uint8_t fuzix_disk_read_port(uint8_t port);
void    fuzix_disk_write_port(uint8_t port, uint8_t data);

#endif
