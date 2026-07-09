// flash_disk.h
#ifndef FLASH_DISK_H
#define FLASH_DISK_H
#include <stdbool.h>
#include <stdint.h>
void fd_write_port(uint8_t port, uint8_t data);
uint8_t fd_read_port(uint8_t port);
void fd_process_flush(void);  // call from core 1
#endif
