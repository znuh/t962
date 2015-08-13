/*
    RAM firmware for T962 reflow oven
    Copyright (C) 2015 Benedikt Heinz (hunz AT mailbox.org)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. The name of the author may not be used to endorse or promote products
       derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, 
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "lpc214x.h"
#include "target.h"

#define BUSYWAIT	while (!(U0LSR & 0x20)) {}

char serial_getc(void)
{
	return (U0LSR & 0x01) ? U0RBR : 0;
}

static void serial_putc(const char c)
{
	if (c == 0x0a)  {
		BUSYWAIT;
		U0THR = 0x0d;
	}
	BUSYWAIT;
	U0THR = c;
}

void serial_puts(const char *s)
{
	for(;*s;s++)
		serial_putc(*s);
}

void serial_init(unsigned int baud)  
{
	unsigned int div = (Fpclk/16)/baud;
	
	PINSEL0 = 5;
	U0LCR = 0x83;
	U0DLM = div>>8;
	U0DLL = div&0xff;
	U0LCR = 3;
    U0IER = 0;
    U0FCR = 7;
}
