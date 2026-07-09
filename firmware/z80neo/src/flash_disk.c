// flash_disk.c — RAM-backed block device on ports 0xE0-0xE3
// 4KB RAM buffer. Reads/writes work. Flash persistence TBD (needs Z80-safe flush).

#include <stdint.h>

#define FD_BUF_SIZE 4096
static uint32_t fd_addr = 0;
static uint8_t  fd_buf[FD_BUF_SIZE];

void fd_write_port(uint8_t port, uint8_t data) {
    switch(port) {
        case 0xE1: fd_addr = (fd_addr & 0xFFFF00) | data; break;
        case 0xE2: fd_addr = (fd_addr & 0xFF00FF) | ((uint32_t)data << 8); break;
        case 0xE3: fd_addr = (fd_addr & 0x00FFFF) | ((uint32_t)data << 16); break;
        case 0xE0: fd_buf[fd_addr % FD_BUF_SIZE] = data; fd_addr++; break;
    }
}

uint8_t fd_read_port(uint8_t port) {
    if(port == 0xE0) { uint8_t v = fd_buf[fd_addr % FD_BUF_SIZE]; fd_addr++; return v; }
    return 0;
}

void fd_process_flush(void) {} // stub for core 1
