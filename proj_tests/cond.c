#include "stdio.h"

#define N 2000

void main (void)
{
  int i,c1,c2;
  int x[N];
  FILE *f;

  for (i = 0;i < N;i++) {
    c1 = 15;
    if (c1 < 17)
      x[i] = c1;
      c2 = c1 + 5;
    if (c2 > c1) {
      x[i] = c2;
      c2 = 10;
      c1 = 5;
      c2 = c1 * 5 - c1 / 5;
      c2 + 1;
    }
  }

  f = fopen ("cond.out","w");

  for(i = 0; i < N/100;i++){
    fprintf(f,"%d\n", x[i]);
  }

  fclose(f);
  exit(0);
}
