#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 1500

static void serialgauss(float **A, float *B, float *X) {
  /* Solve for x in Ax = B */
  int norm, row, col;
  float multiplier;


  /* Gaussian elimination */
  for (norm = 0; norm < N - 1; norm++) {
    for (row = norm + 1; row < N; row++) {
      multiplier = A[row][norm] / A[norm][norm];
      for (col = norm; col < N; col++) A[row][col] -= A[norm][col] * multiplier;
      B[row] -= B[norm] * multiplier;
    }
  }
  /* (Diagonal elements are not normalized to 1.  This is treated in back
   * substitution.)
   */
  /* Back substitution */
  for (row = N - 1; row >= 0; row--) {
    X[row] = B[row];
    for (col = N - 1; col > row; col--) {
      X[row] -= A[row][col] * X[col];
    }
    X[row] /= A[row][row];
  }
}


int main(int argc, char **argv) {
  for (int i = 0; i < 30; i++) {
    float **A = malloc(N * sizeof(float *));
    float *B = malloc(N * sizeof(float));
    for (int i = 0; i < N; i++) {
      A[i] = malloc(N * sizeof(float));
      for (int j = 0; j < N; j++) {
        A[i][j] = rand() / 32768.0;
      }
      B[i] = rand() / 32768.0;
    }

    float *x1 = malloc(N * sizeof(float *));


    long long start = clock();
    serialgauss(A, B, x1);
    long long end = clock();

    printf("%3d: %lldms\n", i, end - start);


    free(x1);
    for (int i = 0; i < N; i++) {
      free(A[i]);
    }
    free(A);
    free(B);
  }

  return 0;
}
