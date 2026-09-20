#include "serial.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int serial_baud_constant(int baud) {
	switch (baud) {
	case 1200:
		return B1200;
	case 2400:
		return B2400;
	case 4800:
		return B4800;
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	case 230400:
		return B230400;
#ifdef B460800
	case 460800:
		return B460800;
#endif
#ifdef B921600
	case 921600:
		return B921600;
#endif
	default:
		return -1;
	}
}

int serial_open(const char *path, int baud) {
	int speed = serial_baud_constant(baud);
	if (speed < 0) {
		fprintf(stderr, "unsupported baud rate %d\n", baud);
		return -1;
	}

	int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
		return -1;
	}

	struct termios tio;
	if (tcgetattr(fd, &tio) != 0) {
		fprintf(stderr, "tcgetattr(%s): %s\n", path, strerror(errno));
		close(fd);
		return -1;
	}

	cfmakeraw(&tio);
	tio.c_cflag |= CLOCAL | CREAD;
	tio.c_cflag &= ~CRTSCTS;
	tio.c_cflag &= ~CSTOPB;
	tio.c_cflag &= ~PARENB;
	tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS8;
	tio.c_cc[VMIN] = 0;
	tio.c_cc[VTIME] = 0;
	cfsetispeed(&tio, speed);
	cfsetospeed(&tio, speed);

	if (tcsetattr(fd, TCSANOW, &tio) != 0) {
		fprintf(stderr, "tcsetattr(%s): %s\n", path, strerror(errno));
		close(fd);
		return -1;
	}
	tcflush(fd, TCIOFLUSH);
	return fd;
}

void serial_close(int fd) {
	if (fd >= 0)
		close(fd);
}

int serial_read(int fd, void *buf, size_t len) {
	ssize_t n = read(fd, buf, len);
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return 0;
		return -1;
	}
	return (int)n;
}

int serial_write(int fd, const void *buf, size_t len) {
	const char *p = buf;
	size_t left = len;
	while (left > 0) {
		ssize_t n = write(fd, p, left);
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				usleep(1000);
				continue;
			}
			if (errno == EINTR)
				continue;
			return -1;
		}
		p += n;
		left -= (size_t)n;
	}
	return (int)len;
}
