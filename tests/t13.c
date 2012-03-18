#include <stdlib.h>
#include <stdio.h>

int loop_label_first_instruction (int a)
{
L1:
printf("*");

if (a > 10) {
a--;
}
else if (a < 5) {
a++;
}
else {
a = 0;
}

goto L1;

// return a;
}
