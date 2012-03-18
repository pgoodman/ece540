int fibonaci(int n)
{
  int ret;

  if (n < 2)
    ret = n;
  else
    ret = (fibonaci (n-1) + fibonaci(n-2));
  return ret;
}

void loop_with_cflow()
{
  int a,b,c,d;

  c = 15;
  do {
    b = 3;
    if (b == 3) {
      a = 3;
      b = 4;
    }
    else {
      a = 4;
      b = 3;
    }
  } while (b == 3);
  d = a + b;
}
