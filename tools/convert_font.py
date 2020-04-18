#!/usr/bin/env python3


from bdflib import reader
from sys import argv
import struct


if len(argv) != 3:
    print("usage: convert_font.py <input> <output>")
    exit(-1)

with open(argv[1], "rb") as handle:
    font = reader.read_bdf(handle)



# the chariot font "format" is just the specific 

expected_w = -1
expected_h = -1


# go through and verify all the glyphs
for i in range(0, 255):
    if i in font:
        gl = font[i];
        if expected_w == -1:
            expected_w = gl.bbW
            expected_h = gl.bbH
        if expected_w != gl.bbW or expected_h != gl.bbH:
            print("Font must be the same dimentions for each char")
            exit(-1)

        if gl.bbW > 8:
            print("Font must be 8 bits wide")
            exit(-1)



# starts with two shorts, width and height
f = struct.pack("BB", expected_w, expected_h);

print(expected_w, expected_h)

# followed by a glyph for every char (0-255 extended ascii)
for i in range(0, 255):
    if i in font:
        gl = font[i];
        for l in reversed(gl.data):
            # each line is a short
            f += struct.pack("H", l)
    else:
        for l in range(0, expected_h):
            f += struct.pack("H", l)



with open(argv[2], "wb") as handle:
    handle.write(f)
