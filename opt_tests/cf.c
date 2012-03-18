int main(void) {

	int a = 1;
	int b = 2;
	int c = a + b + 2 + 3U;
	unsigned k = 1 + 2U + 3U;
	
	int sadd = 1 + 2;
	int ssub = 1 - 2;
	int smul = 2 * 3;
	int sdiv = 3 / 2;
	int smod = 3 % 2;
	int smod_or_rem = -3 % 2;
	int asr = 20 >> 2;
	int lsl = 13 << 2;
	int sand = 5 & 9;
	int sior = 5 | 9;
	int sxor = 5 ^ 9;
	int seq = 5 == 9;
	int sneq = 5 != 6;
	int slt = 5 < 9;
	int sleq = 5 <= 9;
	int snot = ~5;
	int sneg = !5;
	
        unsigned uadd = 1U + 2U;
        unsigned usub = 1U - 2U;
        unsigned umul = 2U * 3U;
        unsigned udiv = 3U / 2U;
        unsigned umod = 3U % 2U;
        unsigned umod_or_rem = -3U % 2U;
        unsigned lsr = 20U >> 2U;
        unsigned ulsl = 13U << 2U;
        unsigned uand = 5U & 9U;
        unsigned uior = 5U | 9U;
        unsigned uxor = 5U ^ 9U;
        unsigned ueq = 5U == 9U;
        unsigned uneq = 5U != 6U;
        unsigned ult = 5U < 9U;
        unsigned uleq = 5U <= 9U;
        unsigned unot = ~5U;
        unsigned uneg = !5U;

	return 1;
}



