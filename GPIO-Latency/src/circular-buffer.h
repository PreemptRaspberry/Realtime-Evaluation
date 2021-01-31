/*
 * circular-buffer.h
 *
 * Author : Daniel Willi
 */

#ifndef UTILS_CIRCULAR_BUFFER__H__
#define UTILS_CIRCULAR_BUFFER__H__

#include <stdbool.h>
#include <stdint.h>

#ifndef CIRCULAR_BUFFER_SIZE
#define CIRCULAR_BUFFER_SIZE 256 // Default size
#endif

typedef struct
{
	uint8_t data[CIRCULAR_BUFFER_SIZE];
	uint8_t read;
	uint8_t write;
} CircularBuffer;

/** Initialize the given CircularBuffer struct.
 * @param circularBuffer The CircularBuffer instance
 * @note Must be called before CircularBuffer is used.
*/
static inline void circularBufferInit(CircularBuffer *circularBuffer)
{
	circularBuffer->read = 0;
	circularBuffer->write = 0;
}

/** Returns the number of elements in the CircularBuffer.
 * @param circularBuffer The CircularBuffer instance
 * @return Number of elements
*/
static inline uint8_t circularBufferSize(const CircularBuffer *circularBuffer)
{
	return (circularBuffer->write - circularBuffer->read) % CIRCULAR_BUFFER_SIZE;
}

/** Returns the number of elements which can be stored at maximum.
 * @return Number of elements
*/
static inline uint8_t circularBufferCapacity()
{
	return CIRCULAR_BUFFER_SIZE - 1;
}

/** Checks if the CircularBuffer has no elements 
 * @param circularBuffer The CircularBuffer instance
 * @return  True if empty, false otherwise
*/
static inline bool circularBufferEmpty(const CircularBuffer *circularBuffer)
{
	return circularBuffer->read == circularBuffer->write;
}

/** Calculates the next write-index.
 * @param circularBuffer The CircularBuffer instance
 * @return index of next write position
*/
static inline uint8_t circularBufferNextWriteIndex(const CircularBuffer *circularBuffer)
{
	return (circularBuffer->write + 1) % CIRCULAR_BUFFER_SIZE;
}

/** Checks if the CircularBuffer is full.
 * @param circularBuffer The CircularBuffer instance
 * @return  True if full, false otherwise
*/
static inline bool circularBufferFull(const CircularBuffer *circularBuffer)
{
	return circularBufferNextWriteIndex(circularBuffer) == circularBuffer->read;
}

/** Pushes the given value to the CircularBuffer.
 * @param circularBuffer The CircularBuffer instance
 * @param[in] value The element to be added
 * @return  True if successful, false otherwise
*/
static inline bool circularBufferPush(CircularBuffer *circularBuffer, const uint8_t value)
{
	uint8_t circularBufferNext = circularBufferNextWriteIndex(circularBuffer);
	if (circularBuffer->read != circularBufferNext)
	{
		circularBuffer->data[circularBuffer->write] = value;
		circularBuffer->write = circularBufferNext;
		return true;
	}

	return false;
}

/** Returns the oldest value from the CircularBuffer, without removing it.
 * @param circularBuffer The CircularBuffer instance
 * @param[out] value The oldest value
 * @return  True if successful, false otherwise
*/
static inline bool circularBufferPeek(CircularBuffer *circularBuffer, uint8_t *value)
{
	if (!circularBufferEmpty(circularBuffer))
	{
		*value = circularBuffer->data[circularBuffer->read];
		return true;
	}

	return false;
}

/** Removes the oldest value from the CircularBuffer.
 * @param circularBuffer The CircularBuffer instance
 * @param[out] value The removed value
 * @return  True if successful, false otherwise
*/
static inline bool circularBufferPop(CircularBuffer *circularBuffer, uint8_t *value)
{
	if (circularBufferPeek(circularBuffer, value))
	{
		circularBuffer->read = (circularBuffer->read + 1) % CIRCULAR_BUFFER_SIZE;
		return true;
	}

	return false;
}

#endif
