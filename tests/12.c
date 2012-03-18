int main() {
     int i = 0;
     for(; i < 10; ++i) {
     label_into_block:
          ++i;
     }
     return 0;
     goto label_into_block;
}
