#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>

int serial_open(const char *path, int baud);
void serial_close(int fd);
int serial_read(int fd, void *buf, size_t len);
int serial_write(int fd, const void *buf, size_t len);
int serial_baud_constant(int baud);

#endif
