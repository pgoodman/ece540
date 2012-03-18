#include <stdlib.h>
#include <stdio.h>

int mbr_test (int a)
{

if (a < 10) {

switch(a)
{
case 1: printf("*\n");
case 2: printf("**\n");
case 3: printf("***\n");
case 4: printf("****\n");
default: printf("*****\n");
}
}
else {

switch(a)
{
case 11: printf("*\n"); break;
case 12: printf("**\n"); break;
case 13: printf("***\n"); break; 
case 14: printf("****\n"); break; 
default: printf("*****\n"); break; } 
} 

return 0; 
}
