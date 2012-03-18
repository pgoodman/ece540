#include <stdlib.h>
#include <stdio.h>

int loop_label_first_instruction_2nd (int a)
{
L1:
printf("*");
if (a < 6)
goto Lend1;
else if (a > 11)
goto Lend2;
else
goto L1;

Lend1:
return a;

Lend2:
a = a+5;
return a;
}
