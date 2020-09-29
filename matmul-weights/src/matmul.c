#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

const int numthreads = 256;
const int numteams = 56;
#ifndef NUMTHREADS
#define NUMTHREADS numthreads
#endif
#ifndef NUMTEAMS
#define NUMTEAMS numteams
#endif

#define N (1024*2)
float matA[N][N];
float matB[N][N];
float matC[N][N];
float matE[N][N];

float alpha = 2.0;
float beta = 3.0;
int main(int argc, char **argv) {
// initialize on host
#pragma omp parallel for
  for (int i=0; i < N; i++) {
    for (int j=0; j < N; j++) {
      matA[i][j] = i+j;
      matB[i][j] = i+j;
      matE[i][j] = 0;
    }
  }
// generate expected result on host
#pragma omp parallel for
  for (int i=0; i < N; i++) {
    for (int j=0; j < N; j++) {
      float sum = 0.0;
      for (int k = 0; k < N; k++) {
	sum += matA[i][k] * matB[k][j];
      }
      matE[i][j] = beta *  matE[i][j] + alpha *sum;
    }
  }

  struct timespec t0,t1,t2;
  fprintf(stderr, "Starting matmul\n");
  // time the data copy separate from gflops
  clock_gettime(CLOCK_REALTIME, &t0);
  #pragma omp target data map(to: matA, matB) map(from: matC)
  {
    clock_gettime(CLOCK_REALTIME, &t1);
    // leaving num_threads and teams off, to allow easier env-var control
    #pragma omp target teams num_teams(NUMTEAMS) thread_limit(NUMTHREADS)
    #pragma omp distribute parallel for collapse(2)
    for (int i = 0; i < N; i++) {
      for (int j = 0; j < N; j++) {
        float sum = 0.0;
        for (int k = 0; k < N; k++) {
	  sum += matA[i][k] * matB[k][j];
        }
        matC[i][j] = beta *  matC[i][j] + alpha *sum;
      }
    }
    clock_gettime(CLOCK_REALTIME, &t2);
  }
  float ops = (float)N *N *N *4.0;
  double m = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec)/1e9;
  fprintf(stderr, "Time %g GBytes/sec %g for copy\n", m, N*N*3.0*4/m/1e9);
  double t = (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec)/1e9;
  fprintf(stderr, "Time %g GFlops/sec %g for compute\n", t, ops/t/1e9);
  double c = (t2.tv_sec - t0.tv_sec) + (t2.tv_nsec - t0.tv_nsec)/1e9;
  fprintf(stderr, "Time %g GFlops/sec %g for copy + compute\n", c, ops/c/1e9);

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      if (matE[i][j] != matC[i][j]) {
        fprintf(stderr, "Failed %d %d\n",i,j);
        return 1;
      }
    }
  }
  fprintf(stderr, "Passed\n");
  return 0;
}
