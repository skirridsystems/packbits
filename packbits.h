/*****************************************************************************
packbits.h  -  run length encoding and decoding using MacPaint / TIFF format.
******************************************************************************/

#ifndef _PACKBITS_H_
#define _PACKBITS_H_

/*----------------------------------------------------------------------------
packbits compresses the source buffer to the destination buffer.
Compression is not guaranteed if there are not enough runs.
In the pathological case where there are no runs (e.g. an incrementing
byte counter) then there is an overhead of 1 byte for each 128 bytes
of source. The destination buffer must be large enough to handle this.
Required destination buffer size is srcCount + (srcCount + 127) / 128.
Return value is the size of destination buffer used.
----------------------------------------------------------------------------*/
uint16_t packbits(const uint8_t *srcPtr, uint8_t *destPtr, uint16_t srcCount, uint16_t destLimit);

/*----------------------------------------------------------------------------
unpackbits decompresses the source buffer to the destination buffer.
It is not possible to predict the unpack size, although it may well be known.
Both source and destination buffer sizes are given and unpacking stops when
either the source runs out or the destination is full.
Return value is the unpacked size.
Unpacking is a lot simpler than packing.
----------------------------------------------------------------------------*/
uint16_t unpackbits(const uint8_t *srcPtr, uint8_t *destPtr, uint16_t srcCount, uint16_t destLimit);

#endif
