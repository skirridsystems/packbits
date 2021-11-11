# packbits
Simple run-length encoding and decoding

## Description 
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

| Header byte | Data following |
|-------------|----------------|
|0 to 127  |(1 + n) literal bytes|
|129 to 255|(257 - n) repeated bytes|
|128       |No operation|

The format is described in [Wikipedia](https://en.wikipedia.org/PackBits)
