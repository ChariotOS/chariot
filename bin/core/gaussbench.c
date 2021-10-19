#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/sysbind.h>
#include <pthread.h>

#define N 1500

static void serialgauss(float **A, float *B, float *X) {
  /* Solve for x in Ax = B */
  int norm, row, col;
  float multiplier;


  /* Gaussian elimination */
  for (norm = 0; norm < N - 1; norm++) {
    for (row = norm + 1; row < N; row++) {
      multiplier = A[row][norm] / A[norm][norm];
      for (col = norm; col < N; col++)
        A[row][col] -= A[norm][col] * multiplier;
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


void *do_gauss(void *arg) {
  // return NULL;
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

  serialgauss(A, B, x1);

  free(x1);
  for (int i = 0; i < N; i++) {
    free(A[i]);
  }
  free(A);
  free(B);
  return NULL;
}

struct thread_info {
  pthread_t thread;
  unsigned long start;
};

int main(int argc, char **argv) {
  int nproc = sysbind_get_nproc();
  printf("nproc=%d\n", nproc);

  struct thread_info *threads = malloc(nproc * sizeof(*threads));

  for (int i = 0; 1; i++) {
    for (int p = 0; p < nproc; p++) {
      threads[p].start = sysbind_gettime_microsecond();
      pthread_create(&threads[p].thread, NULL, do_gauss, NULL);
    }



    printf("%3d:", i);
    for (int p = 0; p < nproc; p++) {
      pthread_join(threads[p].thread, NULL);
      printf(" %8.2f", (sysbind_gettime_microsecond() - threads[p].start) / 1000.0);
    }
    printf("\n");

    // long long end = sysbind_gettime_microseconds();
    // printf("%3d: %lldms\n", i, end - start);
  }


  free(threads);
  return 0;
}
