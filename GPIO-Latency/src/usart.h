/*
 * usart.h
 *
 * Author : Daniel Willi
 */

#ifndef AVRHAL_USART__H__
#define AVRHAL_USART__H__

#include <stdint.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include "circular-buffer.h"

typedef enum
{
	B300 = 3,
	B1200 = 12,
	B9600 = 96,
	B19200 = 192,
	B38400 = 384,
	B57600 = 576,
	B115200 = 1152
} Baudrate;

typedef enum
{
	CONFIG_7N1,
	CONFIG_7E1,
	CONFIG_8N1
} UsartConfig;

/*
* Enable the transmit buffer empty interrupt.
*/
static inline void enableTransmitBufferEmptyInterrupt()
{
#ifdef __AVR_ATmega328P__
	UCSR0B |= (1 << UDRIE0);
#else
	UCSRB |= (1 << UDRIE);
#endif
}

/*
* Disable the transmit buffer empty interrupt.
*/
static inline void disableTransmitBufferEmptyInterrupt()
{
#ifdef __AVR_ATmega328P__
	UCSR0B &= ~(1 << UDRIE0);
#else
	UCSRB &= ~(1 << UDRIE);
#endif
}

/** Setup and enable the usart interface
 * @param[in] baud The desired baudrate
 * @param[in] config The interface configuration (example: 8 data bits, no parity, 1 stop bit)
 * @note Provides commonly used Baud rates 9600, 19200, 38400, 57600 and 115200.
 * @note Provides commonly used configs 7E1, 7N1 and 8N1. Defaults to 8N1 if config doesn't work.
*/
void usartSetup(const Baudrate, const UsartConfig);

/** Get the number of bytes available in the buffer for reading.
 *
 * @return number of bytes
*/
uint8_t usartAvailableForRead();

/** Get the number of bytes which can be written to the transmit buffer.
 *
 * @return number of bytes
*/
uint8_t usartAvailable();

/** Read the next byte from the buffered incoming usart data without removing it.
 * @param[out] byte The next byte
 * @return True if a byte is available, false otherwise
*/
bool usartPeek(uint8_t *);

/** Read the next byte from the buffered incoming usart data.
 * @param[out] byte The next byte
 * @return True if a byte is available, false otherwise
*/
bool usartRead(uint8_t *);

/** Read multiple bytes from the buffered incoming usart data. 
 * @param[out] bytes An array, at least of size maxRead
 * @param[in] maxRead The maximum number of bytes to read
 * @return The number of characters read
*/
uint8_t usartReadBytes(uint8_t *, const uint8_t);

/** Write a byte to the usart.
 * @param[in] byte The data byte to be transmitted 
 * @return True if the byte was written to the transmit buffer, false otherwise
*/
bool usartWrite(const uint8_t);

/** Write a string to the usart.
 * @note The string must be zero-terminated (`\0`). The trailing `\0` is not transmitted.
 * @param[in] str A zero-terminated string
 * @return The number of characters written
*/
uint8_t usartWriteString(const char *);

/** Print data to the usart. 
 * @note Internally va_list(), va_start(), vsnprintf() and va_end() are used.
 * @param[in] format A printf-style format string
 * @return The number of characters written of the resulting string
*/
uint8_t usartPrint(const char *, ...);

#endif
