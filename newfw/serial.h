#ifndef SERIAL_H
#define SERIAL_H

char serial_getc(void);
void serial_puts(const char *s);
void serial_init(unsigned int baud);

#endif
