#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 80
#define HEIGHT 40

int main() {
  float A = 0, B = 0;
  float i, j;
  int k;
  float z[WIDTH * HEIGHT];
  char b[WIDTH * HEIGHT];
  printf("\x1b[2J");

	int size = WIDTH / 2;
	float sz_x = size;
	float sz_y = size / ((float)WIDTH / (float)HEIGHT); 
  while (true) {
    memset(b, ' ', WIDTH * HEIGHT);
    memset(z, 0, 4 * WIDTH * HEIGHT);
		float e = sin(A);
		float g = cos(A);
		float m = cos(B);
		float n = sin(B);
    for (j = 0; j < 6.28; j += 0.07) {
			float d = cos(j);
      float h = d + 2;
      float f = sin(j);
      for (i = 0; i < 6.28; i += 0.02) {
				float c = sin(i);
        float D = 1 / (c * h * e + f * g + 5);
        float l = cos(i);
        float t = c * h * g - f * e;
        int x = (WIDTH / 2.0) + sz_x * D * (l * h * m - t * n);
        int y = (HEIGHT / 2.0) + sz_y * D * (l * h * n + t * m);
        int o = x + WIDTH * y;
        int N = 8 * ((f * e - c * d * g) * m - c * d * e - f * g - l * d * n);
        if (HEIGHT > y && y > 0 && x > 0 && WIDTH > x && D > z[o]) {
          z[o] = D;
          b[o] = ".,-~:;=!*#$@"[N > 0 ? N : 0];
        }
      }
    }
    printf("\x1b[H");
		unsigned last = 0;
		char buf[1024];
		char *cur = buf;
    for (k = 0; k < WIDTH * HEIGHT; k += WIDTH) {
			write(1, b + k, WIDTH);
			puts("");
      A += 0.00004 * WIDTH;
      B += 0.00002 * WIDTH;
    }
  	usleep(1000 * 30);
  }
  return 0;
}
