void weird_loop (int a, int b)
{
  int i;

  i = 0;
Ltop1:
    if (a < b) goto Ltop2;
    b = 6 * a;
    a = 8 * b;
    i = a - b * a;
    goto Ltop3;

Ltop2:
    if (a == b) goto Lend;
    a = 6 * b;
    b = 8 * a;
    i = b - a * b;
    goto Ltop1;

Ltop3:
    if (a < b) goto Ltop1;
    a = b - 1;
    goto Ltop3;

Lend:
  printf ("wow, got here\n");
}
