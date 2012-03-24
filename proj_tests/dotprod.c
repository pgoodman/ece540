#include <stdio.h>


#define	N 1000

typedef int Vector[N];
void vector_dotprod (Vector C, Vector A, Vector B, int n);


void main (void)
{
  int i,j;
  Vector A;
  Vector B;
  Vector C;
  FILE *f;

  for (i = 0;i < N;i++) {
    A[i] = i + i;
    B[i] = i;
  }

  vector_dotprod (C, A, B, N);

  f = fopen ("dotprod.out","w");
  for (i = 0;i < N;i+=20)
    fprintf (f, "C[%d] = %d\n", i, C[i]);

  fclose(f);
  exit(0);
}

void vector_dotprod (Vector C, Vector A, Vector B, int n)
{
  int i,j;
  int t1, t2, t3;

  for (i=0;i < n;i++) {
    for (j=0;j < 5;j++) {
      t1 = A[i];
      t2 = B[i];
      t3 = A[i] * B[i];
      C[i] = t3;
    }
  }
}
