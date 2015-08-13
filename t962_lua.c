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
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "rs232_if.h"

typedef struct t962_s {
	int fd;
} t962_t;

static t962_t *gethandle(lua_State *L)
{
	return *((t962_t**)luaL_checkudata(L,1,"t962"));
}

static int delete(lua_State *L)
{
	t962_t *t962 = gethandle(L);
	close(t962->fd);
	free(t962);
	lua_pushnil(L);
	lua_setmetatable(L,1);
	return 0;
}

static int update(lua_State *L)
{
	t962_t *t962 = gethandle(L);
	int lamp = 0, fan = 0;
	char buf;
	struct timeval tv;
	double rd_time;
	int t1, t2;
	char rxbuf[16];
	int res;
	
	if(lua_isboolean(L, 2)) {
		lamp = lua_toboolean(L, 2);
	}
	if(lua_isboolean(L, 3)) {
		fan = lua_toboolean(L, 3);
	}
	
	buf = '0' + (lamp ? 1 : 0) + (fan ? 2 : 0);
	
	res = rs232_send(t962->fd,(uint8_t*)&buf,1);
	gettimeofday(&tv, NULL);
	res = rs232_read(t962->fd,(uint8_t*)rxbuf,9);
	
	if(res < 3)
		return luaL_error(L, "comm error");
	
	rxbuf[9]=0;
	res = sscanf(rxbuf,"%3d %3d",&t1,&t2);
	
	if(res < 2)
		return luaL_error(L, "comm error");
	
	rd_time = tv.tv_usec;
	rd_time /= 1000000.0;
	rd_time += tv.tv_sec;
	
	lua_pushnumber(L, t1);
	lua_pushnumber(L, t2);
	lua_pushnumber(L, rd_time);
	
	return 3;
}

static int t962_open(lua_State *L)
{
	int uart_fd;
	const char *dev = luaL_checkstring(L, 1);
	if(!dev)
		return luaL_error(L, "no device given");
	
	uart_fd = rs232_init(dev, 19200, 0, 20);
	
	if(uart_fd < 0) {
		luaL_error(L, "cannot connect to oven");
	}
	else {
		t962_t **p=lua_newuserdata(L,sizeof(t962_t*));
		t962_t *t962 = malloc(sizeof(t962_t));
		t962->fd = uart_fd;
		*p=t962;
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_setmetatable(L, -2);
		rs232_flush(uart_fd);
	}
	return 1;
}

static const luaL_Reg R[] =
{
	{ "__gc",			delete },
	{ "update",		update },
	{ NULL,				NULL }
};

LUALIB_API int luaopen_t962(lua_State *L)
{
	luaL_newmetatable(L,"t962");
	lua_pushnil(L);
	lua_setmetatable(L, -2);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	luaL_setfuncs(L, R, 0);
	lua_pushcclosure(L, t962_open, 1);
	lua_setglobal(L, "t962");
	return 1;
}
