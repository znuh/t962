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

#include "lpc214x.h"		/* LPC21xx definitions */
#include "target.h"
#include "serial.h"

// testing
//#define MAXTEMP	200
#define MAXTEMP       300

#define FAN_BIT		(1<<8)
#define LAMP_BIT	(1<<9)
#define BEEP_BIT	(1<<21)

#define BEEP_ON		IOSET0 = BEEP_BIT
#define BEEP_OFF	IOCLR0 = BEEP_BIT

#define LAMP_ON		IOCLR0 = LAMP_BIT
#define LAMP_OFF	IOSET0 = LAMP_BIT

#define FAN_ON		IOCLR0 = FAN_BIT
#define FAN_OFF		IOSET0 = FAN_BIT

typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

/*
static inline char nibble2hex(uint8_t v)
{
	char nibble = v & 0xf;
	return nibble + ((nibble > 9) ? ('a' - 10) : '0');
}

static void tohex(uint32_t val, char *dst)
{
	int i;
	val =
	    (val >> 24) | (val << 24) | ((val >> 8) & 0xff00) | ((val & 0xff00)
								 << 8);
	for (i = 0; i < 4; i++, dst += 2) {
		dst[0] = nibble2hex((val >> (i << 3)) >> 4);
		dst[1] = nibble2hex((val >> (i << 3)) & 0xf);
	}
}
*/

static inline void delay_ms(const uint32_t n)
{
	for(T0TC = 0; T0TC < n; ) {}
}

static uint32_t read_adc(const uint8_t idx)
{
	uint32_t res;
	AD0CR = (1 << idx) | (10 << 8) | (1 << 21) | (1 << 24);
	delay_ms(1);
	res = AD0GDR;
	res = (res & (1 << 31)) ? ((res >> 6) & 0x3ff) : 0xffffffff;
	return res;
}

static void print_temp(char *dst, uint32_t v)
{
	uint32_t tmp = v / 100;
	dst[0] = tmp > 0 ? (tmp + '0') : ' ';
	v -= tmp * 100;

	tmp = v / 10;
	dst[1] = tmp + '0';
	v -= tmp * 10;

	dst[2] = v + '0';
}

struct wave_s {
	uint16_t id;
	uint16_t unknown;
	uint16_t pt[48];
	char *name;
	char *number;
	char *desc;
} __attribute__((packed));

const struct wave_s *waves = (const struct wave_s *)0x52c4;

void dump_waves(void) {
  int i,j;
  serial_puts("{\n");
  for(i=0;i<8;i++) {
    const struct wave_s *wv = waves+i;
    char ebuf[] = "] },\n";
    serial_puts("  \"");
    serial_puts(wv->name);
    serial_puts("\": {\n    \"desc\": \"");
    serial_puts(wv->desc);
    serial_puts("\",\n    \"pts\": [");
    for(j=0;(j<48) && (wv->pt[j]);j++) {
       char buf[]="123, ";
       print_temp(buf,wv->pt[j]);
       buf[3] = ((j<47) && (wv->pt[j+1])) ? ',' : 0;
       serial_puts(buf);
    }
    ebuf[3] = i<7 ? ',' : ' ';
    serial_puts(ebuf);
  }
  serial_puts("}\n");
}

int main(void)
{
	char str[] = "123 123\n";
	uint32_t adc[2];

	serial_init(19200);	/* baud rate setting */

	T0PR = (Fosc/1000)-1;	/* 1ms granularity for both timers */
	T1PR = (Fosc/1000)-1;
	T0TCR = 1;
	T1TCR = 0;
	T1TC = 0;

	// AD0.1, AD0.2
	PINSEL1 = (1 << 24) | (1 << 26);

	IODIR0 = FAN_BIT | LAMP_BIT | BEEP_BIT;
	IOSET0 = FAN_BIT | LAMP_BIT | BEEP_BIT;

	delay_ms(100);
	BEEP_OFF;
	delay_ms(100);
	BEEP_ON;
	delay_ms(100);
	BEEP_OFF;

	do {
		uint8_t rb=0;
		for(; U0LSR & 0x01; rb=U0RBR) {}
		if ((rb >= '0') && (rb <= '3')) {
			T1TC = 0;
			if (rb & 1) {
				LAMP_ON;
				T1TCR = 1;
			} else {
				LAMP_OFF;
			}
			if (rb & 2) {
				FAN_ON;
				T1TCR = 1;
			} else {
				FAN_OFF;
			}
		}
		else if(rb == 'W')
		  dump_waves();

		// 3 seconds timeout
		if ((T1TCR) && (T1TC >= 3000)) {
		    IOSET0 = LAMP_BIT | FAN_BIT;
		    T1TCR = 0;
		    T1TC = 0;
		}
		
		adc[0] = read_adc(1);
		adc[1] = read_adc(2);
		if((rb) && (rb!='W')) {
		  print_temp(str, adc[0]);
		  print_temp(str + 4, adc[1]);
		  serial_puts(str);
		}
	} while ((adc[0] < MAXTEMP) && (adc[1] < MAXTEMP));

	/* safeguard */

	IODIR0 = LAMP_BIT | FAN_BIT | BEEP_BIT;
	// lamp off, beeper on, fan on
	IOSET0 = LAMP_BIT | BEEP_BIT;
	IOCLR0 = FAN_BIT;

	while (1) {
	}

	return 0;		/* never reached */
}
