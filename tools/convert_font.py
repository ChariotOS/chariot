#!/usr/bin/env python3


from bdflib import reader
from sys import argv
import struct


if len(argv) != 3:
    print("usage: convert_font.py <input> <output>")
    exit(-1)

with open(argv[1], "rb") as handle:
    font = reader.read_bdf(handle)


# this part sucks. bdflib doesn't provide this information
height = 0
width = 0
with open(argv[1], "rb") as handle:
    for line in handle:
        if line.startswith(b"FONTBOUNDINGBOX"):
            parts = line.split(b" ")
            height = int(parts[2])
            width = int(parts[2])


MAGIC = 0x464f4e54


cmap = struct.pack("");
dmap = struct.pack("");

count = len(font.codepoints())
i = 0
for cp in font.codepoints():
    gl = font[cp]
    print(i / count)
    i += 1
    if gl.bbW > 32:
        print("Glyphs cannot be wider than 32pixels")
        exit()

    c = struct.pack("")
    # the codepoint
    c += struct.pack("I", cp)

    # data offset
    c += struct.pack("I", len(dmap))
    print(gl)

    # bounding box
    c += struct.pack("b", gl.bbX)
    c += struct.pack("b", gl.bbY)
    c += struct.pack("b", gl.bbW)
    c += struct.pack("b", gl.bbH)
    c += struct.pack("b", gl.advance)


    for line in gl.data:
        dmap += struct.pack("I", line)

    cmap += c


with open(argv[2], "wb") as handle:
    handle.write(struct.pack("I", MAGIC))
    handle.write(struct.pack("I", height))
    handle.write(struct.pack("I", width))
    handle.write(struct.pack("I", len(cmap)))
    handle.write(struct.pack("I", len(dmap)))
    handle.write(cmap)
    handle.write(dmap)
