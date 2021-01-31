/*
 * main.c
 *
 * Author : Daniel Willi
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "usart.h"

volatile uint32_t overflowCntr = 0;
volatile uint16_t signalIn;

void timer1Init()
{
	//disable InputCaptureNoiseCanceler; InputCaptureEdgeSelect - set falling edge as trigger; set cpu-frequency as clock
	TCCR1B = (0 << ICNC1) | (0 << ICES1) | (0 << CS12) | (0 << CS11) | (1 << CS10);
#ifdef __AVR_ATmega328P__
	TIMSK1 = (1 << ICIE1) | (1 << TOIE1); //Input Capture Interrupt Enable, Overflow Interrupt Enable
#else
	TIMSK = (1 << TICIE1) | (1 << TOIE1); //Input Capture Interrupt Enable, Overflow Interrupt Enable
#endif
}

ISR(TIMER1_CAPT_vect)
{
	signalIn = ICR1;
	PORTD |= (1 << PIND4); //turn on the output pin here as we can be sure that the raspi noticed the signal
}

ISR(TIMER1_OVF_vect)
{
	overflowCntr++;
}

int main(void)
{
	DDRD = (1 << PIND1) | (1 << PIND4); //set TX-pin and "pin which is connected to the RPi" to output
#ifdef __AVR_ATmega328P__
	PORTB |= (1 << PINB0); //activate pullup on icp pin
#else
	PORTD |= (1 << PIND6); //activate pullup on icp pin
#endif
	PORTD |= (1 << PIND4);

	usartSetup(B9600, CONFIG_8N1);
	timer1Init();
	uint16_t signalOut;

	sei(); //enable global interrupt

	while (1)
	{
		overflowCntr = 0l;
		signalIn = 0;
		PORTD &= ~(1 << PIND4); //create a falling edge
		signalOut = TCNT1;		//store current time

		while (signalIn == 0)
			; //wait till a timerValue is copied by input capture isr

		if (signalIn > signalOut && overflowCntr != 0)
			overflowCntr--;

		double difference = (double)(overflowCntr * 65536 + signalIn - signalOut) / F_CPU; //timerCycles
		char buffer[32];

		dtostrf(difference, 12, 10, buffer);

		usartPrint("%s\r\n", buffer);
		while (usartAvailable() != ringBufferCapacity())
			;
	}
}