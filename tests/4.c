int main(void) {
foo: goto bar;
bar: goto foo;
   return 0;
}

