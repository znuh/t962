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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <alloca.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>
#include <lua.h>
#include <lauxlib.h>

typedef struct dmm_s {
	int fd;
	int errorcode;
	int valid;
	double val;
	struct timeval tv;
	pthread_mutex_t mtx;
	pthread_t dmm_thread;
	int quit;
} dmm_t;

static dmm_t *gethandle(lua_State *L)
{
	return *((dmm_t**)luaL_checkudata(L,1,"dmm"));
}

static int delete(lua_State *L)
{
	dmm_t *dmm = gethandle(L);
	
	pthread_mutex_lock(&dmm->mtx);
	dmm->quit = 1;
	pthread_mutex_unlock(&dmm->mtx);

	pthread_join(dmm->dmm_thread, NULL);

	close(dmm->fd);
	
	free(dmm);
	lua_pushnil(L);
	lua_setmetatable(L,1);
	return 0;
}

int vc_init(const char *dev, int baud, int parity, int time)
{
	int status, res, fd = open(dev, O_RDWR|O_NOCTTY);
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
	
	if ((res = ioctl(fd, TIOCMGET, &status)) < 0) {
		printf("term_reset ioctl(TIOCMGET): %d\n", res);
		return res;
	}

	status &= ~TIOCM_RTS;

	if ((res = ioctl(fd, TIOCMSET, &status)) < 0) {
		printf("term_reset ioctl(TIOCMSET): %d\n", res);
		return res;
	}

	return fd;
}

/*
int bla(int argc, char **argv) {
	int fd, i, idx=0;
	int uart_fd;
	char buf[24];
	struct timeval tv;
	
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
*/

static void *dmm_loop(void *priv)
{
	dmm_t *dmm = priv;
	struct pollfd pfd;
	struct timeval tv;
	int res, idx=0;
	char buf[24];

	pfd.fd = dmm->fd;
	pfd.events = POLLIN;

	while (1) {
		// quit requested?
		pthread_mutex_lock(&dmm->mtx);
		if (dmm->quit) {
			pthread_mutex_unlock(&dmm->mtx);
			break;
		}
		pthread_mutex_unlock(&dmm->mtx);

		res = poll(&pfd, 1, 100);
		
		if (!res)
			continue;
		else if (res < 0) {
			pthread_mutex_lock(&dmm->mtx);
			dmm->errorcode = res;
			pthread_mutex_unlock(&dmm->mtx);
			fprintf(stderr, "[DMM] poll error: %s\n",
				strerror(errno));
			break;
		}

		if(!idx)
			gettimeofday(&tv, NULL);
			
		res = read(dmm->fd, buf+idx, 1);
		if(res < 0) {
			fprintf(stderr, "[DMM] read error: %s\n",
				strerror(errno));
			break;
		}
			
		
		if(buf[idx] == 0x0a) {
			buf[idx]=0;
			//puts(buf);
			//printf("%d\n",idx);
			if(idx == 10) {
				buf[5]=0;
				int val = strtoul(buf+1, NULL, 10);
				pthread_mutex_lock(&dmm->mtx);
				dmm->val = val;
				dmm->val /= 10;
				dmm->tv = tv;
				dmm->valid = 1;
				pthread_mutex_unlock(&dmm->mtx);
			}
			idx=0;
			memset(buf,0,sizeof(buf));
		}
		else {
			idx = (idx+1) % 24;
		}
	}

	return NULL;
}

static int dmm_read(lua_State *L) {
	dmm_t *dmm = gethandle(L);
	dmm_t copy;
	double tstamp;
	pthread_mutex_lock(&dmm->mtx);
	memcpy(&copy,dmm,sizeof(copy));
	pthread_mutex_unlock(&dmm->mtx);
	if(copy.errorcode) 
		return luaL_error(L, "DMM error");
	else if(!copy.valid)
		return 0;
	lua_pushnumber(L, copy.val);
	tstamp = copy.tv.tv_usec;
	tstamp /= 1000000.0;
	tstamp += copy.tv.tv_sec;
	lua_pushnumber(L, tstamp);
	return 2;
}

static int dmm_open(lua_State *L)
{
	int uart_fd, res;
	const char *dev = luaL_checkstring(L, 1);
	if(!dev)
		return luaL_error(L, "no device given");
	
	uart_fd = vc_init(dev, -1, 0, 0);
	
	if(uart_fd < 0) {
		luaL_error(L, "cannot connect to DMM");
	}
	else {
		dmm_t **p=lua_newuserdata(L,sizeof(dmm_t*));
		dmm_t *dmm = malloc(sizeof(dmm_t));
		bzero(dmm, sizeof(dmm_t));
		dmm->fd = uart_fd;
		pthread_mutex_init(&dmm->mtx, NULL);
		res = pthread_create(&dmm->dmm_thread, NULL, dmm_loop, dmm);
		if (res) {
			close(dmm->fd);
			free(dmm);
			return luaL_error(L, "pthread_create error");
		}
		*p=dmm;
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_setmetatable(L, -2);
	}
	return 1;
}

static const luaL_Reg R[] =
{
	{ "__gc",			delete },
	{ "read",		dmm_read },
	{ NULL,				NULL }
};

LUALIB_API int luaopen_dmm(lua_State *L)
{
	luaL_newmetatable(L,"dmm");
	lua_pushnil(L);
	lua_setmetatable(L, -2);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	luaL_setfuncs(L, R, 0);
	lua_pushcclosure(L, dmm_open, 1);
	lua_setglobal(L, "dmm");
	return 1;
}
