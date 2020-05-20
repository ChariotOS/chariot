
// Author: Christian Vallentin <mail@vallentinsource.com>
// Website: http://vallentinsource.com
// Repository: https://github.com/MrVallentin/LoadBMP
//
// Date Created: January 03, 2014
// Last Modified: August 13, 2016
//
// Version: 1.1.0

// Copyright (c) 2014-2016 Christian Vallentin <mail@vallentinsource.com>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.

// Include loadbmp.h as following
// to create the implementation file.
//
// #define LOADBMP_IMPLEMENTATION
// #include "loadbmp.h"

#ifndef LOADBMP_H
#define LOADBMP_H

// Errors
#define LOADBMP_NO_ERROR 0

#define LOADBMP_OUT_OF_MEMORY 1

#define LOADBMP_FILE_NOT_FOUND 2
#define LOADBMP_FILE_OPERATION 3

#define LOADBMP_INVALID_FILE_FORMAT 4

#define LOADBMP_INVALID_SIGNATURE 5
#define LOADBMP_INVALID_BITS_PER_PIXEL 6

// Components
#define LOADBMP_RGB 3
#define LOADBMP_RGBA 4

#include <mem.h>
#include <util.h>

// LoadBMP uses raw buffers and support RGB and RGBA formats.
// The order is RGBRGBRGB... or RGBARGBARGBA..., from top left
// to bottom right, without any padding.

unsigned int load_bmp(const char *filename, int *&imageData,
		      int &width, int &height,
		      unsigned int components);

struct bitmap {

  static inline bitmap load(const char *path) {

		int w = 0; int h = 0;
		int *data = NULL;


		load_bmp(path, data, w, h, LOADBMP_RGBA);
		hexdump(data, w * h * sizeof(int), true);
		return bitmap(data, w, h);
	}

  inline int width(void) { return m_width; }
  inline int height(void) { return m_height; }


	// I trust the caller to be good.
  inline int pix(int x, int y) {
		return data[x + y * m_width];
	}
  inline ~bitmap(void) { kfree(data); }

 private:
  inline bitmap(int *data, int w, int h)
      : data(data), m_width(w), m_height(h) {}

  int *data;
  int m_width, m_height;
};
#endif
