int main(void) {
   int x = 0;
top:
   if(x) {
      x = 0;
   } else {
      x = x + 1;
      goto top;
   }
   return x;
}


