void infinity()
{
  int a,b,c,d,e;

  printf ("please wait for a long time...");
  e=1;
  while (e) {
    b = 5;
    a = b * 3;
    c = a;
    d = a + c - b / 2;
  }
  printf ("The world has ended.\n");
}

void never()
{
  int a,b,c,d,i;

  c = -3;
  for (i=0;i<c;i+=3) {
    a = 6;
    b = a / 3;
    c = c + b;
    d = 17;
  }
  printf ("come back real soon now\n");
}
