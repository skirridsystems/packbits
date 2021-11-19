/*****************************************************************************
PackBits.cs  -  run length encoding and decoding using MacPaint / TIFF format.

This is a very simple form of run length encoding which packs repeated bytes.
It is not the most efficient packing method, but it takes very little resource
to unpack the data and all the data is inline with no header structure.
This makes it usable on both large and small objects.

It is particularly useful for packing low colour graphics objects up to
8-bits per pixel, where it can efficiently compress runs of identical pixels.

The output stream consists of a header byte which indicates the uncompressed
length and whether the data is repeated or differing, followed by either
a single byte to be repeated or the set of differing data.

The maximum number of bytes with a single header byte is 128.

Header = 0..127     (1 + n) literal bytes
Header = 129..255   (257 - n) repeated bytes
Header = 128        No operation

https://en.wikipedia.org/wiki/PackBits

Source on GitHub:
https://github.com/skirridsystems/packbits
******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Compression
{
    public static class PackBits
    {
        private const int minRepeat = 3;                // Min run length worth packing
        private const int maxRepeat = 128;              // Max run length allowed by the format
        private const int maxDiff = 128;                // Max diff length allowed by the format
        private const int maxUnpackBytes = 0x800000;    // Nominal unpack limit of 8MiB
        private static byte EncodeRepeat(int count)
        {
            return (byte)(0x101 - count);
        }
        private static byte EncodeDiff(int count)
        {
            return (byte)(count - 1);
        }
        private static bool IsDiff(byte hdr)
        {
            return (hdr < 128);
        }
        private static bool IsRepeat(byte hdr)
        {
            return (hdr > 128);
        }
        private static int DecodeDiff(byte hdr)
        {
            return hdr + 1;
        }
        private static int DecodeRepeat(byte hdr)
        {
            return 0x101 - hdr;
        }

        /*----------------------------------------------------------------------------
        Pack() compresses the source array, returning the packed data as an array.
        Compression is not guaranteed if there are not enough runs.
        In the pathological case where there are no runs (e.g. an incrementing
        byte counter) then there is an overhead of 1 byte for each 128 bytes
        of source.
        ----------------------------------------------------------------------------*/
        public static byte[] Pack(byte[] src)
        {
            bool inRun = false;
            int srcIndex = 0;           // Index of current byte in src array
            int pendingIndex = 0;       // Index of first pending byte
            int bytesPending = 0;       // Bytes looked at but not yet output
            int runStart = 0;           // Distance into pending bytes that a run starts
            byte currByte;              // Byte currently being considered
            byte lastByte;              // Previous byte

            // Need at least one byte to compress
            if (src.Length == 0)
            {
                return Array.Empty<byte>();
            }
            List<byte> destList = new();

            // Prime compressor with first character.
            lastByte = src[srcIndex++];
            ++bytesPending;

            while (srcIndex < src.Length)
            {
                currByte = src[srcIndex++];
                ++bytesPending;
                if (inRun)
                {
                    if ((currByte != lastByte) || (bytesPending > maxRepeat))
                    {
                        // End of run or maximum run length reached.
                        destList.Add(EncodeRepeat(bytesPending - 1));
                        destList.Add(lastByte);
                        bytesPending = 1;
                        pendingIndex = srcIndex - 1;
                        runStart = 0;
                        inRun = false;
                    }
                }
                else
                {
                    if (bytesPending > maxDiff)
                    {
                        // We have as much differing data as we can output in one chunk.
                        // Output maxDiff leaving one byte.
                        destList.Add(EncodeDiff(maxDiff));
                        for (int i = 0; i < maxDiff; i++)
                        {
                            destList.Add(src[pendingIndex++]);
                        }
                        bytesPending -= maxDiff;
                        runStart = bytesPending - 1;    // A run could start here
                    }
                    else if (currByte == lastByte)
                    {
                        if ((bytesPending - runStart >= minRepeat) || (runStart == 0))
                        {
                            // This is a worthwhile run
                            if (runStart != 0)
                            {
                                // Flush differing data out of input buffer
                                destList.Add(EncodeDiff(runStart));
                                for (int i = 0; i < runStart; i++)
                                {
                                    destList.Add(src[pendingIndex++]);
                                }
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
                destList.Add(EncodeRepeat(bytesPending));
                destList.Add(lastByte);
            }
            else
            {
                destList.Add(EncodeDiff(bytesPending));
                for (int i = 0; i < bytesPending; i++)
                {
                    destList.Add(src[pendingIndex++]);
                }
            }
            return destList.ToArray();
        }

        /*----------------------------------------------------------------------------
        Unpack decompresses the source array, returning the unpacked data as an array.
        The expanded data may be very much larger than the packed data; if it consists
        purely of runs then every 2 bytes of packed data can represent 128 bytes of
        unpacked data.

        Unpacking is a lot simpler than packing.
        ----------------------------------------------------------------------------*/
        public static byte[] Unpack(byte[] src)
        {
            int srcIndex = 0;           // Index of current byte in src array
            List<byte> destList = new();

            while (srcIndex < src.Length)
            {
                // Read header byte
                byte hdr = src[srcIndex++];
                int srcRemaining = src.Length - srcIndex;
                if (IsDiff(hdr))
                {
                    // This is a run of differing bytes
                    int count = DecodeDiff(hdr);
                    // Check for overrun
                    if (count > srcRemaining)
                    {
                        count = srcRemaining;
                    }
                    for (int i = 0; i < count; i++)
                    {
                        destList.Add(src[srcIndex++]);
                    }
                }
                else if (IsRepeat(hdr))
                {
                    // This is a run of repeated bytes
                    int count = DecodeRepeat(hdr);
                    if (srcRemaining != 0)
                    {
                        // Copy the repeated byte run
                        for (int i = 0; i < count; i++)
                        {
                            destList.Add(src[srcIndex]);
                        }
                        ++srcIndex;
                    }
                }
            }
            return destList.ToArray();
        }
    }
}
