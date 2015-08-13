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

int update_oven(int fd, int lamp, int fan, int *t1, int *t2, struct timeval *tv) {
	char buf='0' + (lamp ? 1 : 0) + (fan ? 2 : 0);
	char rxbuf[16];
	int i;
	int res = rs232_send(fd,&buf,1);
	//for(i=0;i
	gettimeofday(tv, NULL);
	res = rs232_read(fd,rxbuf,9);
	rxbuf[9]=0;
	res = sscanf(rxbuf,"%3d %3d",t1,t2);
	if(res < 2)
		return -1;
	//printf("%d\n",res);
	return 0;
}

//#define PCB_OFFSET	42 // PCB seems to be ~42°C hotter than air

static const xilinx[] = {
	32, 40, 47, 55, 62, 70, 77, 85, 92, 100, 107, 115, 122, 130, 137, 
	145, 152, 160, 167, 175, 182, 190, 197, 205, 212, 220, 227, 235, 
	240, 250, 235, 220, 210, 200, 190, 180, 170, 160, 150, 140, 
	130, 120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 25, 0};
	// Xilinx CSG324 Pbfree | peak: 250C 10s

static const int wave1[] = { 
	40,  50,  60,  73,  86, 100, 112, 124, 135, 138, 142, 145, 147, 
	148, 150, 153, 157, 160, 165, 170, 175, 185, 195, 205, 215, 225, 
	237, 230, 225, 218, 210, 195, 185, 170, 160, 140, 130, 120, 110, 
	100,  90,  80,  70,  60, 0 }; // wave1: Suitble:85Sn/15Pb 70Sn/30Pb|Solding Temp: 237C 10s

static const int wave3[] = { 
	40,  50,  60,  70,  80,  92, 105, 118, 132, 137, 142, 148, 151, 
	154, 156, 156, 158, 160, 164, 168, 172, 178, 185, 192, 202, 212, 
	224, 234, 244, 252, 255, 257, 252, 245, 238, 232, 212, 192, 172, 
	158, 141, 124, 116, 108, 100,  80,  60, 0 }; // wave3: Suitble:Ag3.5 Ag4.0/Cu.5 Cu.75|Solding Temp: 257C 10s

#define wave xilinx

int main(int argc, char **argv) {
	int fd, i, idx=0;
	int uart_fd;
	int lamp=0, fan=0;
	char buf[24];
	struct timeval tv;
	time_t t0;
	time_t last_update=0;
	float tmp;
	
	uart_fd = rs232_init("/dev/ttyUSB0", 19200, 0, 20);
	rs232_flush(uart_fd);
	
	t0 = time(NULL);
	while(t0 == time(NULL)) {}
	t0++;
	while(1) {
		struct timeval tv;
		int t1, t2, res;
		int t_ist, t_soll, t_next;
		int elapsed;
		int wave_idx;
		int time_ofs;
		
		elapsed = time(NULL) - t0;
		//printf("%d\n",elapsed);
		wave_idx = elapsed / 10;
		//t_soll = wave[wave_idx];
		t_next = wave[wave_idx+1];
		if(!t_next) {
			while(1) {
				update_oven(uart_fd, 0, 1, &t1, &t2, &tv);
				usleep(100000);
			}
			break;
		}
		int delta = t_next - wave[wave_idx];
		time_ofs = elapsed % 10;
		delta *= time_ofs;
		delta /= 10;
		t_soll = wave[wave_idx] + delta;
		
		res = update_oven(uart_fd, lamp, fan, &t1, &t2, &tv);
		if(res < 0)
			break;
		
		t_ist = (t1 + t2 + 40) / 2;
		tmp = t_ist;
		tmp += tmp*0.18;
		t_ist = tmp;

		//printf("%d\n",t_ist);
		if(elapsed != last_update) {
			if ( t_soll < t_ist ) {
				if ( (t_ist - t_soll) <= 10 ) {
					lamp=0;
					fan=0;
				}
				else {
					lamp=0;
					fan=1;
				}
			}
			else if ( (t_soll - t_ist) >= 5 ) {
				lamp=1;
				fan=0;
			}
			else {
				lamp=0;
				fan=0;
			}
			res = update_oven(uart_fd, lamp, fan, &t1, &t2, &tv);
			last_update=elapsed;
		}
		
		printf("%u.%06u %3d %3d %3d %3d %3d %3d\n",
			tv.tv_sec,tv.tv_usec,t_soll,t1,t2,t_ist,lamp*100,fan*100);
		fprintf(stderr,"%u.%06u %3d %3d %3d %3d %3d %3d\n",
			tv.tv_sec,tv.tv_usec,t_soll,t1,t2,t_ist,lamp*100,fan*100);
		fflush(stdout);
		
		/*
		printf("time %d wpos %d wt %d wt+1 %d delta %d t_soll %d\n",
			elapsed, wave_idx, wave[wave_idx], wave[wave_idx+1],
			delta, t_soll);
		*/
		usleep(10000);
	}
		
	return 0;
}
