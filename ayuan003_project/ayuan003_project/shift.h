/*
 * shift.c
 *
 * Created: 3/13/2018 11:58:18 AM
 *  Author: student
 */ 

// D3 data, D4 latch, D5 clock
// requires outputs on D3-D5
#ifndef _shift_h_
#define _shift_h_

#include <avr/io.h>

void clockLow() {
	PORTD = PORTD & 0b11011111;
}
void clockHigh() {
	PORTD = PORTD | 0b00100000;
}

void clock() {
	clockHigh();
	clockLow();
}

void latchLow() {
	PORTD = PORTD & 0b11101111;
}
void latchHigh() {
	PORTD = PORTD | 0b00010000;
}

void latch() {
	latchHigh();
	latchLow();
}

void dataLow() {
	PORTD = PORTD & 0b11110111;
}
void dataHigh() {
	PORTD = PORTD | 0b00001000;
}

void rows(unsigned char temp) {
	for (unsigned char i = 0; i < 8; ++i) {
		if ((temp << i & 0x80) == 0x80) {
			dataHigh();
		}
		else {
			dataLow();
		}
		clock();
	}
}

void columns(unsigned char temp) {
	for (unsigned char i = 0; i < 8; ++i) {
		if (temp >> i & 0x01) {
			dataHigh();
		}
		else {
			dataLow();
		}
		clock();
	}
}

/*
columns to pwr
colors  to gnd
7  
  RGB
0
7     0 columns
*/
void RGBmatrix(unsigned char column, unsigned char red, unsigned char green, unsigned char blue) {
	red = ~red;
	green = ~green;
	blue = ~blue;
	columns(column);
	rows(green);
	rows(blue);
	rows(red);
	latch();
}

#endif