/*
 * 
 * Copyright (C) 2015 Benedikt Heinz <Zn000h AT gmail.com>
 * 
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this code.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <alloca.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include "rs232_if.h"

int vc_init(char *dev, int baud, int parity, int time)
{
	int res, fd = open(dev, O_RDWR|O_NOCTTY);
	struct termios termio;

	if (fd < 0) {
		printf("rs232_init open %s: %d\n", dev, fd);
		return fd;
	}

	if ((res = tcgetattr(fd, &termio)) < 0) {
		printf("rs232_init tcgetattr: %d\n", res);
		close(fd);
		return res;
	}

	termio.c_cflag = B2400;

	termio.c_cflag |= CS7 | CREAD | CLOCAL | PARENB | PARODD;
	termio.c_iflag = IGNBRK | IGNPAR;
	termio.c_oflag = 0;
	termio.c_lflag = 0;

	termio.c_cc[VMIN] = 1;
	termio.c_cc[VTIME] = 0;

	if ((res = tcsetattr(fd, TCSANOW, &termio)) < 0) {
		printf("rs232_init tcsetattr: %d\n", res);
		close(fd);
		return res;
	}
	
	rs232_set_rts(fd, 0);

	return fd;
}


int read_adc(int fd) {
	char buf[16];
	int res;
	if(fd < 0) return fd;
	//res = rs232_read(fd,buf,20);
	res = read(fd,buf,12);
	printf("%d\n",res);
	if(res < 5) return -1;
	buf[5]=0;
	return atoi(buf+2);
}

int main(int argc, char **argv) {
	int fd, i, idx=0;
	int uart_fd;
	char buf[24];
	struct timeval tv;

	assert(argc>1);
	uart_fd = vc_init(argv[1], -1, 0, 0);
	//rs232_flush(uart_fd);
	//printf("argh\n");
	while(1) {
		i = read(uart_fd,buf+idx,1);
		if(!idx)
			gettimeofday(&tv, NULL);
		if(buf[idx] == 0x0a) {
			idx=0;
			buf[4]=0;
			printf("%u.%u %s\n",tv.tv_sec,tv.tv_usec,buf+1);
			fprintf(stderr,"%u.%u %s\n",tv.tv_sec,tv.tv_usec,buf+1);
			fflush(stdout);
		}
		else
			idx++;
	}
	
	if(fd>=0)
		close(fd);
	
	return 0;
}
