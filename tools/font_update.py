#!/usr/bin/env python3

import os
from re import search
import freetype

paths = []

for entry in os.scandir('base/sys/fonts/'):
    if entry.path.endswith('.dir'):
        continue
    paths.append(entry.path)

faces = []

def conv(b, prefix):
    if b:
        return prefix
    return ''

for path in paths:
    face = freetype.Face(path)
    faces.append(face)
    filename = path.split('/')[-1]
    family = face.family_name.decode().lower()
    style = face.style_name.decode().lower()

    styles = style.split(' ')

    regular = conv('regular' in styles, 'R')
    light = conv('light' in styles, 'L')
    bold =  conv('bold' in styles, 'B')
    italic = conv('italic' in styles, 'I')
    semibold = conv('semibold' in styles, 'S')
    book = conv('book' in styles, 'b')
    print(f'{filename}:{family}:{regular}{light}{bold}{italic}{semibold}{book}')
