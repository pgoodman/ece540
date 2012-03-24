#include <stdio.h>

#define N 10000

double x[N];

main()
{
  int i, a, b, c, d, e, f, g;
  double ctr = 0.0;
  FILE *file;

  for (i = 0; i < N; i++) {
    ctr = ctr + (double)(i);
    if (ctr > 200.0){
      ctr = ctr - 300.0;
    }
    x[i] = ctr;
  }

  for (i = 1; i < N-1; i++) {
    a = x[i + 1];
    b = x[i - 1];
    c = a + b;
    d = a + b + c;
    e = a + b + c + d;
    f = a + b + c + d + e;
    g = (a + b + c + d + e + f) % 200;
    x[i] = (double) g;
  }

  file = fopen ("cse.out","w");

  for (i = 10; i < 20; i++) {
    fprintf(file,"x[%d] = %.3f\n",i,x[i]);
  }

  fclose(file);
  exit(0);
}
