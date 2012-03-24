#include <stdio.h>

#define N 300
#define TIMESTEPS 5

double x[N][N];

main()
{
  int i,j,k,t;
  double ctr = 0.0;
  FILE *f;

  for (i = 0; i < N; i++) {
    for (j = 0; j < N; j++) {
      ctr = ctr + (double)(i | j);
      if (ctr > 200.0)
	ctr = ctr - 300.0;
      x[i][j] = ctr;
    }
  }

  for (t = 0; t < TIMESTEPS; t++) {
    for (i = 1; i < N-1; i++) {
      for (j = 1; j < N-1; j++) {
	for (k = 1; k < 5; k++) {
	  x[i][j] =  (x[i][j] + x[i-1][j] + x[i+1][j] + 
		      x[i][j-1] + x[i][j+1])/5 + (double)k;
	}
      }
    }
  }

  f = fopen ("sor.out","w");
  for (t = 0; t < TIMESTEPS; t++) {
    for (i = 1; i < N-1; i++) {
      for (j = 1; j < N-1; j++) {
	for (k = 1; k < 5; k++) {
	  x[i][j] =  (x[i][j] + x[i-1][j] + x[i+1][j] + 
		      x[i][j-1] + x[i][j+1])/5 + (double)k;
	}
      }
    }
    fprintf(f,"x[20][5] = %.3f, x[149][100] = %.3f\n",x[20][5],x[149][100]);
  }

  fclose(f);
  exit(0);
}
