#!/usr/bin/env python3


import sys


def format_parts(va, parts, pbits):
    p = []
    mask = (1 << pbits) - 1

    for off in range(parts - 1, -1, -1):
        t = ((va >> (pbits * off + 12)) & mask)
        p.append(f'0x{t:03x}')

    return ' '.join(p)


for arg in sys.argv[1:]:
    va = int(arg, 16)

    print(arg, 'SV-39: ', format_parts(va, 3, 9))
    print(arg, 'SV-48: ', format_parts(va, 4, 9))
    print()

