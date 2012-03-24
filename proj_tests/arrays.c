#include <stdio.h>


#define	N 50
#define	M 25

typedef int MatrixNM[N][M];
typedef int MatrixMN[M][N];
typedef int MatrixNN[N][N];


void main (void)
{
  int i,j,k;
  MatrixNM A;
  MatrixMN B;
  MatrixNN C;
  FILE *f;

  for (i = 0;i < N;i++)
    for (j = 0;j < M;j++) {
      A[i][j] = i * j;
      if (j != 0)
        B[j][i] = i / j;
      else
        B[j][i] = 0;
    }

  for (i = 0;i < N;i++)
    for (j = 0;j < N;j++)
      for (k = 0;k < M;k++)
        C[i][j] = i * A[i][k] - B[k][j];

  f = fopen ("arrays.out","w");
  for (i = 0;i < N;i++)
    fprintf (f,"C[%d][%d] = %d\n", i, i, C[i][i]);

  fclose(f);
  exit(0);
}
