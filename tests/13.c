
static int foo(int a, int b) {
	return a + b;
}

int main(void) {
	int x;
	int y;
	int *z;

	if(x) {
		x = y ^ y;
		y = x;
	}

	if(y) {
		y = x ^ x;
		x = y;
	}

	x = foo(x, y);
	z = &y;
	*z = foo(y, y);

	return y;
}