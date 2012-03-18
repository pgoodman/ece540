void weird_loop (int a, int b)
{
  int i;

  i = 0;
Ltop:
  switch (a) {
    case 0 :
      a = 16 * b;
      i++;
      goto Ltop;
    case 1 :
      b = 16 * a;
      i--;
      goto Ltop;
    default :
      break;
  }
  printf ("wow, got here\n");
}
