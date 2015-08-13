
all:
	gcc -O2 -fPIC -Wall -c rs232_if.c
	gcc -O2 -fPIC -Wall -I/usr/include/lua5.2 -c t962_lua.c
	gcc -O -shared -fPIC rs232_if.o t962_lua.o -o t962.so -llua5.2 -lpthread
	gcc -O2 -fPIC -Wall -I/usr/include/lua5.2 -c dmm_lua.c
	gcc -O -shared -fPIC dmm_lua.o -o dmm.so -llua5.2 -lpthread
clean:
	rm -f *.o *.so *~

