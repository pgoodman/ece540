#include <stdlib.h>
#include <stdio.h>

int loop_label_at_bottom (int a)
{
  int y = 0;
  int x = a;

  goto L1;

L2:
  if (x == 0)
  {
    goto Lend;
  }

  x--;
  y += 2;

  if (((x & 1) == 1) && ((a & 1) == 1))
  {
    y -= 1;
  }

L1:
  goto L2;

Lend:
  return y;
}

