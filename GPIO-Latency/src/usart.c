/*
 * usart.c
 *
 * Author : Daniel Willi
 */

#include <stdint.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "circular-buffer.h"
#include "usart.h"

CircularBuffer sendBuffer;
CircularBuffer recvBuffer;

/*
* ISR for Transmit Buffer Empty: empties the sendBuffer and sends it out - disables itself if there is no data to send anymore
*/
ISR(USART_UDRE_vect)
{
	// Get character and send it
	uint8_t data;
	if (circularBufferPop(&sendBuffer, &data))
#ifdef __AVR_ATmega328P__
		UDR0 = data;
#else
		UDR = data;
#endif
	// Disable the interrupt if we have nothing to write
	if (circularBufferEmpty(&sendBuffer))
		disableTransmitBufferEmptyInterrupt();
}

/*
* ISR for receive complete: Adds received bytes to the receiveBuffer
*/
#ifdef __AVR_ATmega328P__
ISR(USART_RX_vect)
{
	const uint8_t data = UDR0;
#else
ISR(USART_RXC_vect)
{
	const uint8_t data = UDR;
#endif

	circularBufferPush(&recvBuffer, data);
}

void usartSetup(const Baudrate baud, const SerialConfig config)
{
	circularBufferInit(&sendBuffer);
	circularBufferInit(&recvBuffer);
	//configure baudrate
	const uint16_t baudValue = F_CPU / (16l * baud * 100) - 1;
#ifdef __AVR_ATmega328P__
	UBRR0H = baudValue >> 8;
	UBRR0L = baudValue;
#else
	UBRRH = baudValue >> 8;
	UBRRL = baudValue;
#endif
	//configure interface-configuration
	switch (config)
	{
	case CONFIG_7E1:
#ifdef __AVR_ATmega328P__
		UCSR0C = (1 << UCSZ01) | (1 << UPM01);
#else
		UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UPM1);
#endif
		break;
	case CONFIG_7N1:
#ifdef __AVR_ATmega328P__
		UCSR0C = (1 << UCSZ01);
#else
		UCSRC = (1 << URSEL) | (1 << UCSZ1);
#endif
		break;
	case CONFIG_8N1:
	default:
#ifdef __AVR_ATmega328P__
		UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
#else
		UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
#endif
		break;
	}
//activate tx and rx unit and ISRs for rx and tx
#ifdef __AVR_ATmega328P__
	UCSR0B = (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0) | (1 << UDRIE0);
#else
	UCSRB = (1 << TXEN) | (1 << RXEN) | (1 << RXCIE) | (1 << UDRIE);
#endif
	//activate global interrupts
	sei();

	return;
}

uint8_t usartAvailableForRead()
{
	return circularBufferSize(&recvBuffer);
}

uint8_t usartAvailable()
{
	return circularBufferCapacity() - circularBufferSize(&sendBuffer);
}

bool usartPeek(uint8_t *byte)
{
	return circularBufferPeek(&recvBuffer, byte);
}

bool usartRead(uint8_t *byte)
{
	return circularBufferPop(&recvBuffer, byte);
}

uint8_t usartReadBytes(uint8_t *bytes, const uint8_t maxRead)
{
	uint8_t i = 0;

	while (i < maxRead && usartRead(&bytes[i]))
		i++;

	return i;
}

bool usartWrite(const uint8_t byte)
{
	bool result = circularBufferPush(&sendBuffer, byte);
	if (result)
		enableTransmitBufferEmptyInterrupt();

	return result;
}

uint8_t usartWriteString(const char *str)
{
	uint8_t i = 0;
	char c;
	while ((c = str[i]) != '\0')
	{
		i++;
		if (!usartWrite(c))
			break;
	}

	return i;
}

uint8_t usartPrint(const char *format, ...)
{
	char buffer[CIRCULAR_BUFFER_SIZE];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, CIRCULAR_BUFFER_SIZE, format, args);
	va_end(args);

	return usartWriteString(buffer);
}
