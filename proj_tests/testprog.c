#include <stdio.h>
#include <stdlib.h>

#define iterations 300
#define small_iterations 3
#define const1 3
#define const2 4
#define const3 5

float f1(float b, float c){
   int i;
   float j, k;

   j = c;
   for(i = 0; i < iterations; i++){
      k = b * i;
      j += k;
   }

   return k;
}

int f2(int b){
   int i, j;
   int k;

   k = b;
   for(i = 0; i < iterations; i++){
      for(j = 0; j < small_iterations; j++){
         k += j;
      }
   }

   return k;
}

int f3(int b, int c){
   int i;
   int j, k;

   j = c;
   for(i = 0; i < iterations; i++){
      k = b * i;
      j += k;
   }

   return k;
}

int f4(int b, int c){
   int i;
   int j, k;

   j = 0;
   for(i = 0; i < iterations; i++){
      k = b + c;
      j += k;
   }

   return j;
}

int f5(int b, int c){
   int i, j, k;

   j = const1;
   for(i = 0; i < iterations; i++){
      if(j == 0)
         k = i * b;
      else
         k = i * c;
   }

   return k;
}

int f6(int c){
   int a[iterations];
   int b[iterations];
   int i, k;

   k = 0;

   for(i = 0; i < iterations; i++){
      a[i] = i;
   }
   for(i = 0; i < iterations; i++){
      b[i] = c + a[i];
   }
   for(i = 0; i < iterations; i++){
      k += b[i];
   }

   return k;
}

int f7(int b, int c){
   int i, j;

   j = const1;
   for(i = 0; i < iterations; i++){
      if(i == const1){
         j = b;
      }
      else{
         j = c;
      }
   }

   return j;
}

int main(){
   int t, x;
   FILE* f;

   t = const1;

   x = f1(const2, const3);
   fprintf(stderr, "f1=%d\n", x);
   t += x;
   
   x = f2(const1);
   fprintf(stderr, "f2=%d\n", x);
   t += x;

   x = f3(const2, const3);
   fprintf(stderr, "f3=%d\n", x);
   t += x;

   x = f4(const2, const3);
   fprintf(stderr, "f4=%d\n", x);
   t += x;

   x = f5(const2, const3);
   fprintf(stderr, "f5=%d\n", x);
   t += x;

   x = f6(const1);
   fprintf(stderr, "f6=%d\n", x);
   t += x;

   x = f7(const1, const2);
   fprintf(stderr, "f7=%d\n", x);
   t += x;

   f = fopen("testprog.out", "w");
   fprintf(f, "finished: %d\n", t);
   fclose(f);

   return 0;
}
