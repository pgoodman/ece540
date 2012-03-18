int main() {
	int x;
	if(x > 0) {
		loop_head:
			x = 1;
			x = 2;
			x = 3;
			if(x > 3) {
				goto loop_tail; // break
			} else {
				goto loop_head; // continue
			}
		loop_tail:
		x = 9;
	}

	x = x + 1;
	if(x > 10) {
		goto loop_head; // restart loop
	}
}
