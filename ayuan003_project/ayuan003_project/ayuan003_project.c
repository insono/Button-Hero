/*
 * ayuan003_project.c
 *
 * Created: 3/8/2018 11:24:44 AM
 *  Author: student
 */ 
//work on joystick and game logic
//init hit miss score so far...
//whammy if time, maybe bluetooth
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "io.h"
#include "shift.h"
#include "scheduler.h"
#include "bit.h"
#include "timer.h"
#include <avr/eeprom.h>

// 0.954 hz is lowest frequency possible with this function,
// based on settings in PWM_on()
// Passing in 0 as the frequency will stop the speaker from generating sound
void set_PWM(double frequency) {
	static double current_frequency; // Keeps track of the currently set frequency
	// Will only update the registers when the frequency changes, otherwise allows
	// music to play uninterrupted.
	if (frequency != current_frequency) {
		if (!frequency) { TCCR3B &= 0x08; } //stops timer/counter
		else { TCCR3B |= 0x03; } // resumes/continues timer/counter
		
		// prevents OCR3A from overflowing, using prescaler 64
		// 0.954 is smallest frequency that will not result in overflow
		if (frequency < 0.954) { OCR3A = 0xFFFF; }
		
		// prevents OCR0A from underflowing, using prescaler 64					// 31250 is largest frequency that will not result in underflow
		else if (frequency > 31250) { OCR3A = 0x0000; }
		
		// set OCR3A based on desired frequency
		else { OCR3A = (short)(8000000 / (128 * frequency)) - 1; }

		TCNT3 = 0; // resets counter
		current_frequency = frequency; // Updates the current frequency
	}
}

void PWM_on() {
	TCCR3A = (1 << COM3A0);
	// COM3A0: Toggle PB3 on compare match between counter and OCR0A
	TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);
	// WGM02: When counter (TCNT0) matches OCR0A, reset counter
	// CS01 & CS30: Set a prescaler of 64
	set_PWM(0);
}

void PWM_off() {
	TCCR3A = 0x00;
	TCCR3B = 0x00;
}

	
void ADC_init()
{
	ADMUX = (1<<REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t ReadADC(uint8_t ch) {
	ch &= 0x07;
	ADMUX = (ADMUX & 0xF8) | ch;
	ADCSRA |= (1 << ADSC);
	while(ADCSRA & (1 << ADSC));
	ADCSRA |= (1 << ADIF);
	return (ADC);
}

//0x01 down, 0x02 up, 0x04 right, 0x08 left
unsigned char joyInput() {
	unsigned char inp = 0x00;
	int temp = ReadADC(5) - 560; //up down
	if (temp > 0 + 50) {inp |= 0x01;}
	if (temp < 0 - 50) {inp |= 0x02;}
	temp = ReadADC(6) - 543; //left right
	if (temp > 0 + 50) {inp |= 0x04;}
	if (temp < 0 - 50) {inp |= 0x08;}
	return inp;
}

void lcdWrite(unsigned char cursor, unsigned short score) {
	for (unsigned i = 0; i < 4; ++i) {
		LCD_Cursor(cursor + 4);
		unsigned char digit = score%10 + '0';
		LCD_WriteData(digit);
		score = score / 10;
		--cursor;
	}
	LCD_Cursor(32);
}

#define E 659.25
#define G 783.99
#define F 698.46
#define D 587.33

#define B 493.88
#define A 880.00
#define C 523.25

unsigned char numberOfSongs = 2;
unsigned char hit = 0;
unsigned char miss = 0;
unsigned short oldScore = 0;
unsigned short newScore = 0;

//tone deaf lugia
//									   {R, G, B,GB, R,RGB,R, G, B,RGB,R, G,RG};
double	lugiaNotes[] =            {0.0, E, G, F, D, E, B, E, G, F, B, E, G, A};
unsigned char lugiaHeldHigh[] =	    {8, 5, 5, 6, 3, 5, 7, 5, 5, 6, 2, 2, 2, 8};
unsigned char lugiaBlankHigh[] =      {1, 1, 1, 3, 1, 1, 4, 1, 1, 1, 1, 1, 1};

//get actual naruto notes
double        FireNFlamesNotes[] =     {0.0, E, G, E, G, F, F, G, G, E, G, E, G, F, F, G, G, E, G, E, G, F, F, G, G, E, G, E, G, F, F, G, G, E, G, E, G, F, F, G, G, E, E, E, E, E, E, E, F, F, F, F, F, G, G, G, G, G, G, E, E, E, G, G, F};
unsigned char FireNFlamesHeldHigh[] =  {8,   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 1, 1, 1, 5};
unsigned char FireNFlamesBlankHeld[] =   {  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0};

//s1.notes = lugiaNotes;

unsigned char i = 0; //note to play
unsigned char j = 0; //count for how long
double*	songNotes[] =             {lugiaNotes,     FireNFlamesNotes};
unsigned char* songHeldHigh[] =   {lugiaHeldHigh,  FireNFlamesHeldHigh};
unsigned char* songBlankHigh[] =  {lugiaBlankHigh, FireNFlamesBlankHeld};
unsigned char songLength[] =      {14,             65};

unsigned char k = 0; //which song to play
unsigned char lugia[] = "Lugia's Song";
unsigned char FireNFlames[] = "Fire N Flames";
unsigned char* songs[] = {lugia, FireNFlames};

unsigned char dDefault[] = {0xFF, 0x81, 0x00, 0x81};
unsigned char dRed[] =	   {0xFF, 0x60, 0x00, 0x00};
unsigned char dGreen[] =   {0xF0, 0x00, 0x18, 0x00};
unsigned char dBlue[] =    {0x0F, 0x00, 0x00, 0x06};
unsigned char dbRed[] =	   {0x00, 0x60, 0x00, 0x00};
unsigned char dbGreen[] =  {0x00, 0x00, 0x18, 0x00};
unsigned char dbBlue[] =   {0x00, 0x00, 0x00, 0x06};
	
unsigned char* mDefault[] = {dDefault, dRed, dGreen, dBlue, dbRed, dbGreen, dbBlue};
void mMatrix() {
	for (unsigned i = 0; i < 7; ++i) {
		RGBmatrix(mDefault[i][0], mDefault[i][1], mDefault[i][2], mDefault[i][3]);
	}
}

double noteOn(unsigned char k, unsigned char time) {
	unsigned char index = 0;
	double current;
	unsigned char count = 0;
	while (1) {
		current = songNotes[k][index];
		count += songHeldHigh[k][index];
		if (count >= time) {break;}
		
		current = -1;
		count += songBlankHigh[k][index];
		if (count >= time) {break;}

		++index;
	}
	return current;
}
void mConvert(unsigned char kSong, unsigned char elapsedTime) {
	unsigned char r = 0x00;
	unsigned char g = 0x00;
	unsigned char b = 0x00;

	for (unsigned char i = 0; i < 7; ++i) {
		double temp = noteOn(kSong, elapsedTime + i);
		if (temp == E) {
			r |= 0x02 << i;
		}
		else if (temp == G) {
			g |= 0x02 << i;
		}
		else if (temp == F) {
			b |= 0x02 << i;
		}
		else if (temp == D) {
			g |= 0x02 << i;
			b |= 0x02 << i;
		}
		else if (temp == A) {
			r |= 0x02 << i;
			g |= 0x02 << i;
		}
		else if (temp == C) {
			r |= 0x02 << i;
			b |= 0x02 << i;
		}
		else if (temp == B) {
			r |= 0x02 << i;
			g |= 0x02 << i;
			b |= 0x02 << i;
		}	
	}
	
	dRed[0] = r;
	dGreen[0] = g;
	dBlue[0] = b;
}

unsigned char displayToggle = 0;
unsigned char songToggle = 0;
unsigned char gameToggle = 0;
enum menu {init, start, selector, song} menuState;
void menuSM() {
	unsigned temp = ~PINA & 0x07;
	switch(menuState) {
		case init:
			menuState = start;
			break;
		case start:
			if (temp == 0x01 || temp == 0x02 || temp == 0x04) {
				menuState = selector;
				displayToggle = 0;
			}
			else {
				menuState = start;
			}
			break;
		case selector:
			if (temp == 0x01) {
				LCD_DisplayString(1, songs[k]);
				songToggle = 1;
				menuState = song;
				displayToggle = 0;
			}
			else {
				menuState = selector;
			}
			break;
		case song:
			if (songToggle == 0) {
				menuState = selector;
			}
			else if (songToggle == 1) {
				menuState = song;
			}
			break;
	}
	switch(menuState) {
		case init:
			k = 0;
			displayToggle = 0;
			songToggle = 0;
			break;
		case start:
			if (displayToggle == 0) {LCD_DisplayString(1, "Button Hero     Press to Start");}
			displayToggle = 1;
			break;
		case selector:
			if (temp == 0x02) {
				//go right
				if (k < numberOfSongs - 1) {
					++k;
					displayToggle = 0;
					}
			}
			else if (temp == 0x04) {
				//go left
				if (k > 0) {
					--k;
					displayToggle = 0;
				}
			}
			if (displayToggle == 0) {LCD_DisplayString(1, songs[k]);}
			displayToggle = 1;
			break;
		case song:
			
			if (displayToggle == 0) {
				LCD_Cursor(17);
				LCD_WriteData('S');
				LCD_Cursor(18);
				LCD_WriteData('c');
				LCD_Cursor(19);
				LCD_WriteData('o');
				LCD_Cursor(20);
				LCD_WriteData('r');
				LCD_Cursor(21);
				LCD_WriteData('e');
				LCD_Cursor(22);
				LCD_WriteData(':');
				LCD_Cursor(23);
				LCD_WriteData(' ');
				lcdWrite(24, 0);
				displayToggle = 1;
			}
			if (oldScore != newScore) {
				lcdWrite(24, newScore);
				oldScore = newScore;
			}
			break;
	}
	};
unsigned char count = 0;
unsigned char notePlay = 0;
unsigned char starPowerTime = 0;
unsigned char starPower = 0;
enum player {init2, wait, play, pause} playerState;
void playerSM() {
	switch(playerState) {
		case init2:
			playerState = wait;
			break;
		case wait:
			if (songToggle == 1) {
				playerState = play;
				++count;
				++starPowerTime;
			}
			else {
				playerState = wait;
			}
			break;
		case play:
			if (starPowerTime > 25) {starPower = 0;}
			++j;
			if (i < songLength[k]) {
				if (j > songHeldHigh[k][i]) {
					++i;
					playerState = pause;
					j = 0;
				}
				else {
					playerState = play;
					++count;
					++starPowerTime;
				}
			}
			else {
				songToggle = 0;
				playerState = init2;
			}
			break;
		case pause:
			if (starPowerTime > 25) {starPower = 0;}
			++j;
			if (j > songBlankHigh[k][i-1]) {
				playerState = play;
				j = 0;
			}
			else {
				playerState = pause;
				++count;
			}
			break;
	}
	switch(playerState) {
		case init2:
			i = 0;
			j = 0;
			count = 0;
			starPowerTime = 0;
			set_PWM(0.0);
			break;
		case wait:
			break;
		case play:
			if (notePlay == 1) {
				set_PWM(songNotes[k][i]);
			}
			else {
				set_PWM(0.0);
			}
			mConvert(k, count);
			break;
		case pause:
			set_PWM(0.0);
			mConvert(k, count);
			break;
	}
	};

void clearDBs() {
	dbRed[0] = 0x00;
	dbGreen[0] = 0x00;
	dbBlue[0] = 0x00;
}
enum input {initi, waiti, playi} inputState;
unsigned char previousButton = 0;
unsigned char joyToggle = 0;
unsigned char joyIn;
void inputSM() {
	unsigned char temp = 0x07 & ~PINA;
	switch(inputState) {
		case initi:
			inputState = waiti;
			break;
		case waiti:
			if (songToggle == 1) {
				inputState = playi;
			}
			else {
				inputState = waiti;
			}
			break;
		case playi:
			if (songToggle == 0) {
				if (k == 0) {
					if (newScore > eeprom_read_word(0x00)) {
						eeprom_update_word(0x00, newScore);
						LCD_DisplayString(1, "New High Score");
						lcdWrite(21, newScore);
						LCD_Cursor(17);
						LCD_WriteData(0);
						LCD_Cursor(32);
						LCD_WriteData(0);
					}
					else {
						LCD_DisplayString(1, "High Score ");
						lcdWrite(12, eeprom_read_word(0x00));
						LCD_Cursor(17);
						LCD_WriteData('Y');
						LCD_Cursor(18);
						LCD_WriteData('o');
						LCD_Cursor(19);
						LCD_WriteData('u');
						LCD_Cursor(20);
						LCD_WriteData('r');
						LCD_Cursor(21);
						LCD_WriteData('s');
						LCD_Cursor(22);
						LCD_WriteData(' ');
						lcdWrite(28, newScore);
					}
				}
				else if (k == 1) {
					if (newScore > eeprom_read_word(0x0F)) {
						eeprom_update_word(0x0F, newScore);
						LCD_DisplayString(1, "New High Score");
						lcdWrite(21, newScore);
						LCD_Cursor(17);
						LCD_WriteData(0);
						LCD_Cursor(32);
						LCD_WriteData(0);
					}
					else {
						LCD_DisplayString(1, "High Score ");
						lcdWrite(12, eeprom_read_word(0x0F));
						LCD_Cursor(17);
						LCD_WriteData('Y');
						LCD_Cursor(18);
						LCD_WriteData('o');
						LCD_Cursor(19);
						LCD_WriteData('u');
						LCD_Cursor(20);
						LCD_WriteData('r');
						LCD_Cursor(21);
						LCD_WriteData('\'');
						LCD_Cursor(22);
						LCD_WriteData('s');
						LCD_Cursor(23);
						LCD_WriteData(' ');
						lcdWrite(28, newScore);
					}
				}
				inputState = initi;
			}
			else {
				inputState = playi;
			}
			break;
	}
	switch(inputState) {
		case initi:
			previousButton = 0;
			newScore = 0;
			joyToggle = 0;
			notePlay = 0;
			starPower = 0;
		break;
		case waiti:
		break;
		case playi: //add a red blue button/note later
			joyIn = joyInput();
			if (joyIn == 0x01 | joyIn == 0x02 | k == 1) {
				joyToggle = 1;
			}
			else {
				joyToggle = 0;
			}
			if (joyIn == 0x04 | joyIn == 0x08) {
				starPower = 1;
				dRed[2]  =  0x60;
				dbRed[2] =  0x60;
				dGreen[3] = 0x18;
				dbGreen[3]= 0x18;
				dBlue[2]  = 0x06;
				dbBlue[2] = 0x06;
				dBlue[1]  = 0x06;
				dbBlue[1] = 0x06;
			}
			if (starPower == 0) {
				dRed[2]  =  0x00;
				dbRed[2] =  0x00;
				dGreen[3] = 0x00;
				dbGreen[3]= 0x00;
				dBlue[2]  = 0x00;
				dbBlue[2] = 0x00;
				dBlue[1]  = 0x00;
				dbBlue[1] = 0x00;
			}
			if (temp == 0x04) {
				if (previousButton != 0x04) {clearDBs();}
				//red button
				dbRed[0] = 0x01;
				previousButton = 0x04;
				double check = noteOn(k, count);
				if (check == E & joyToggle) {newScore = newScore + 10; if (starPower == 1) {newScore += 10;} notePlay = 1;}
				else {notePlay = 0;}
			}
			else if (temp == 0x02) {
				if (previousButton != 0x02) {clearDBs();}
				//green button
				dbGreen[0] = 0x01;
				previousButton = 0x02;
				double check = noteOn(k, count);
				if (check == G & joyToggle) {newScore = newScore + 10; if (starPower == 1) {newScore += 10;}notePlay = 1;}
				else {notePlay = 0;}
			}
			else if (temp == 0x01) {
				if (previousButton != 0x01) {clearDBs();}
				//blue button
				dbBlue[0] = 0x01;
				previousButton = 0x01;
				double check = noteOn(k, count);
				if (check == F & joyToggle) {newScore = newScore + 10; if (starPower == 1) {newScore += 10;}notePlay = 1;}
				else {notePlay = 0;}
			}
			else if (temp == 0x03) {
				if (previousButton != 0x03) {clearDBs();}
				//green blue button
				dbGreen[0] = 0x01;dbBlue[0] = 0x01;
				previousButton = 0x03;
				double check = noteOn(k, count);
				if (check == D & joyToggle) {newScore = newScore + 10; if (starPower == 1) {newScore += 10;}notePlay = 1;}
				else {notePlay = 0;}
			}
			else if (temp == 0x06) {
				if (previousButton != 0x06) {clearDBs();}
				//red green button
				dbRed[0] = 0x01;dbGreen[0] = 0x01;
				previousButton = 0x06;
				double check = noteOn(k, count);
				if (check == A & joyToggle) {newScore = newScore + 10; if (starPower == 1) {newScore += 10;}notePlay = 1;}
				else {notePlay = 0;}
			}
			else if (temp == 0x05) {
				if (previousButton != 0x05) {clearDBs();}
				//red blue button
				dbRed[0] = 0x01;dbBlue[0] = 0x01;
				previousButton = 0x05;
				double check = noteOn(k, count);
				if (check == C & joyToggle) {newScore = newScore + 10; if (starPower == 1) {newScore += 10;}notePlay = 1;}
				else {notePlay = 0;}
			}
			else if (temp == 0x07) {
				if (previousButton != 0x07) {clearDBs();}
				//red green blue button
				dbRed[0] = 0x01;dbGreen[0] = 0x01;dbBlue[0] = 0x01;
				previousButton = 0x07;
				double check = noteOn(k, count);
				if (check == B & joyToggle) {newScore = newScore + 10; if (starPower == 1) {newScore += 10;}notePlay = 1;}
				else {notePlay = 0;}
			}
			else {clearDBs();}
		break;
	}
	};

int main(void)
{//up down to a5, left right to a6
	DDRA = 0x00; PORTA = 0xFF;
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xFF; PORTD = 0x00;
	ADC_init();
	LCD_init();
	PWM_on();

	if(eeprom_read_word( (uint16_t *) 0x00) == 0xFFFF) {
		eeprom_update_word( (uint16_t*) 0x00, 0);
	}
	if (eeprom_read_word( (uint16_t *) 0x0F) == 0xFFFF) {
		eeprom_update_word( (uint16_t*) 0x0F, 0);
	}
	
	// Period for the tasks
	
	unsigned long int SMTick1_calc = 200;
	unsigned long int SMTick2_calc = 100;
	unsigned long int SMTick3_calc = 100;
	
	//Calculating GCD
	unsigned long int tmpGCD = 1;
	tmpGCD = findGCD(SMTick1_calc, SMTick2_calc);
	tmpGCD = findGCD(tmpGCD, SMTick3_calc);

	//Greatest common divisor for all tasks or smallest time unit for tasks.
	unsigned long int GCD = tmpGCD;

	//Recalculate GCD periods for scheduler
	unsigned long int SMTick1_period = SMTick1_calc/GCD;
	unsigned long int SMTick2_period = SMTick2_calc/GCD;
	unsigned long int SMTick3_period = SMTick3_calc/GCD;

	//Declare an array of tasks
	static task task1, task2, task3;
	task *tasks[] = { &task1, &task2, &task3};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

	// Task 1
	task1.state = init;//Task initial state.
	task1.period = SMTick1_period;//Task Period.
	task1.elapsedTime = SMTick1_period;//Task current elapsed time.
	task1.TickFct = &menuSM;//Function pointer for the tick.

	// Task 2
	task2.state = init2;//Task initial state.
	task2.period = SMTick2_period;//Task Period.
	task2.elapsedTime = SMTick2_period;//Task current elapsed time.
	task2.TickFct = &playerSM;//Function pointer for the tick.

	// Task 3
	task3.state = initi;//Task initial state.
	task3.period = SMTick3_period;//Task Period.
	task3.elapsedTime = SMTick3_period; // Task current elasped time.
	task3.TickFct = &inputSM; // Function pointer for the tick.

	// Set the timer and turn it on
	TimerSet(GCD);
	TimerOn();

	unsigned short i; // Scheduler for-loop iterator
	
    while(1)
    {		
		mMatrix();

		// Scheduler code
		for ( i = 0; i < numTasks; i++ ) {
			// Task is ready to tick
			if ( tasks[i]->elapsedTime == tasks[i]->period ) {
				// Setting next state for task
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				// Reset the elapsed time for next tick.
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += 1;
		}
		while(!TimerFlag) {mMatrix();}
		TimerFlag = 0;

    }
    }