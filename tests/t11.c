#include <stdlib.h>
#include <stdio.h>

int hw_1_test_11 (int a)
{
  int y = 0;
  int x = a;

L1:
  if (x)
  {
    x--;
    y += 2;
    goto L2;
  }
  goto Lend;

L2:
  if (x)
  {
    x--;
    y -= 1;
    goto L1;
  }
  goto Lend;

Lend:
  return y;
}

void subfunction (void)
{
	printf("*");
	return;
}

