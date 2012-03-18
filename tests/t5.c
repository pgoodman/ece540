void main ()
{
  int a,b,c,d,i,j;

  if (i != 0)
    j = 1;

  if (a && b)
    i = 1;

  if (a || b)
    i = 1;

  if ((a||c) && (b||d))
    i = 1;

  if ((a&&c) || (b&&d))
    i = 1;

  exit(i);
}
