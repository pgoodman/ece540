#include <stdio.h>


#define	N 100

typedef int Matrix[N][N];
void matrix_multiply (Matrix C, Matrix A, Matrix B, int n);


void main (void)
{
  int i,j;
  Matrix A;
  Matrix B;
  Matrix C;
  FILE *f;

  for (i = 0;i < N;i++)
    for (j = 0;j < N;j++) {
      A[i][j] = i * j;
      if (j != 0)
        B[i][j] = i / j;
      else
        B[i][j] = 0;
    }

  matrix_multiply (C, A, B, N);

  f = fopen ("mmult.out","w");
  for (i = 0;i < N;i++)
    fprintf (f, "C[%d][%d] = %d\n", i, i, C[i][i]);

  fclose(f);
  exit(0);
}

void matrix_multiply (Matrix C, Matrix A, Matrix B, int n)
{
  int i,j,k;

  for (i=0;i < n;i++) {
    for (j=0;j < n;j++) {
      C[i][j] = 0;
      for (k=0;k < n;k++)
        C[i][j] += A[i][k] * B[k][j];
    }
  }
}
