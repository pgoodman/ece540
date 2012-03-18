int main(void) {
   int i, j;
   for(i = 0; i < 10; ++i) {
      for(j = 10; j > 0; --j) {
         if(i * j == 5) {
            break;
         } else if(i - j == 4) {
            goto done;
         }
      }
   }
done:
   return 0;
}

