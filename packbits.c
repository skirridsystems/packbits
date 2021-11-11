/*****************************************************************************
packbits.c  -  run length encoding and decoding using MacPaint / TIFF format.

This is a very simple form of run length encoding which packs repeated bytes.
It is not the most efficient packing method, but it takes very little resource
to unpack the data and all the data is inline with no header structure.
This makes it usable on both large and small objects.

It is particularly useful for packing low colour graphics objects up to
8-bits per pixel, where it can efficiently compress runs of identical pixels.

The output stream consists of a header byte which indicates the uncompressed
length and whether the data is repeated or differing, followed by either
a single byte to be repeated or the set of differeing data.

The maximum number of bytes with a single header byte is 128.

Header = 0..127     (1 + n) literal bytes
Header = 129..255   (257 - n) repeated bytes
Header = 128        No operation

https://en.wikipedia.org/wiki/PackBits

Source on GitHub:
https://github.com/skirridsystems/packbits
******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "packbits.h"

#define MIN_REPT            3       // Minimum run to compress between differ blocks
#define MAX_REPT            128     // Maximum run of repeated byte
#define MAX_DIFF            128     // Maximum run of differing bytes

// Encoding for header byte based on number of bytes represented.
#define ENCODE_DIFF(n)      (uint8_t)((n) - 1)
#define ENCODE_REPT(n)      (uint8_t)(1 - (n))

// Decoding for header byte to give output run length
#define IS_DIFF(h)          ((h) < 128)
#define IS_REPT(h)          ((h) > 128)
#define DECODE_DIFF(h)      (uint8_t)((h) + 1)
#define DECODE_REPT(h)      (uint8_t)(1 - (h))

/*----------------------------------------------------------------------------
packbits compresses the source buffer to the destination buffer.
Compression is not guaranteed if there are not enough runs.
In the pathological case where there are no runs (e.g. an incrementing
byte counter) then there is an overhead of 1 byte for each 128 bytes
of source. If the destination buffer is not large enough to handle this,
the function returns 0 and the destination buffer content is incomplete.

If unconditional compression is required, the size of the destination
buffer must be at least srcCount + (srcCount + 127) / 128.

Return value is the size of destination buffer actually used.
----------------------------------------------------------------------------*/
uint16_t packbits(const uint8_t *srcPtr, uint8_t *destPtr, uint16_t srcCount, uint16_t destLimit)
{
    bool inRun = false;
    uint16_t destCount = 0;             // Destination buffer used
    uint16_t bytesPending = 0;          // Bytes looked at but not yet output
    const uint8_t *pendingPtr;          // Pointer to first pending byte
    uint16_t runStart = 0;              // Distance into pending bytes that a run starts
    uint8_t currByte;                   // Byte currently being considered
    uint8_t lastByte;                   // Previous byte
    
    // Need at least one byte to compress
    if (srcCount == 0) return 0;
    
    pendingPtr = srcPtr;
    
    // Prime compressor with first character.
    lastByte = *srcPtr++;
    ++bytesPending;
    
    while (--srcCount != 0)
    {
        currByte = *srcPtr++;
        ++bytesPending;
        if (inRun)
        {
            if ((currByte != lastByte) || (bytesPending > MAX_REPT))
            {
                // End of run or maximum run length reached.
                if (destCount + 2 > destLimit) return 0;
                destCount += 2;
                *destPtr++ = ENCODE_REPT(bytesPending - 1);
                *destPtr++ = lastByte;
                bytesPending = 1;
                pendingPtr = srcPtr - 1;
                runStart = 0;
                inRun = false;
            }
        }
        else
        {
            if (bytesPending > MAX_DIFF)
            {
                // We have as much differing data as we can output in one chunk.
                // Output MAX_DIFF leaving one byte.
                if (destCount + 1 + MAX_DIFF > destLimit) return 0;
                destCount += 1 + MAX_DIFF;
                *destPtr++ = ENCODE_DIFF(MAX_DIFF);
                memcpy(destPtr, pendingPtr, MAX_DIFF);
                destPtr += MAX_DIFF;
                pendingPtr += MAX_DIFF;
                bytesPending -= MAX_DIFF;
                runStart = bytesPending - 1;    // A run could start here
            }
            else if (currByte == lastByte)
            {
                if ((bytesPending - runStart >= MIN_REPT) || (runStart == 0))
                {
                    // This is a worthwhile run
                    if (runStart != 0)
                    {
                        // Flush differing data out of input buffer
                        if (destCount + 1 + runStart > destLimit) return 0;
                        destCount += 1 + runStart;
                        *destPtr++ = ENCODE_DIFF(runStart);
                        memcpy(destPtr, pendingPtr, runStart);
                        destPtr += runStart;
                    }
                    bytesPending -= runStart;  // Length of run
                    inRun = true;
                }
            }
            else
            {
                runStart = bytesPending - 1;    // A run could start here
            }
        }
        lastByte = currByte;
    }
    
    // Output the remainder
    if (inRun)
    {
        if (destCount + 2 > destLimit) return 0;
        destCount += 2;
        *destPtr++ = ENCODE_REPT(bytesPending);
        *destPtr++ = lastByte;
    }
    else
    {
        if (destCount + 1 + bytesPending > destLimit) return 0;
        destCount += 1 + bytesPending;
        *destPtr++ = ENCODE_DIFF(bytesPending);
        memcpy(destPtr, pendingPtr, bytesPending);
    }
    return destCount;
}

/*----------------------------------------------------------------------------
unpackbits decompresses the source buffer to the destination buffer.
It is not possible to predict the unpack size, although it may well be known.
Both source and destination buffer sizes are given and unpacking stops when
either the source runs out or the destination is full.
Return value is the unpacked size.
Unpacking is a lot simpler than packing.
----------------------------------------------------------------------------*/
uint16_t unpackbits(const uint8_t *srcPtr, uint8_t *destPtr, uint16_t srcCount, uint16_t destLimit)
{
    uint8_t hdr;                    // Header byte indicating run length and type
    uint8_t count;                  // Run length derived from header
    uint16_t destRemaining;         // Buffer size still available for unpacking into
    
    destRemaining = destLimit;
    while ((srcCount != 0) && (destRemaining != 0))
    {
        // Read header byte
        hdr = *srcPtr++;
        --srcCount;
        if (IS_DIFF(hdr))
        {
            // This is a run of differing bytes
            count = DECODE_DIFF(hdr);
            // Check for overrun
            if (count > destRemaining)
            {
                count = destRemaining;
            }
            if (count > srcCount)
            {
                count = srcCount;
            }
            if (count != 0)
            {
                // Copy the differing byte run
                memcpy(destPtr, srcPtr, count);
                srcPtr += count;
                srcCount -= count;
                destPtr += count;
                destRemaining -= count;
            }
        }
        else if (IS_REPT(hdr))
        {
            // This is a run of repeated bytes
            count = DECODE_REPT(hdr);
            // Check for overrun
            if (count > destRemaining)
            {
                count = destRemaining;
            }
            if ((count != 0) && (srcCount != 0))
            {
                // Copy the repeated byte run
                memset(destPtr, *srcPtr, count);
                srcPtr++;
                srcCount--;
                destPtr += count;
                destRemaining -= count;
            }
        }
    }
    return destLimit - destRemaining;   // Number of bytes actually output
}
