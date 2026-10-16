/* Host smoke test for float_strtod.c (musl's strtof/strtod/strtold, ported
 * for the PS5). Also compiles into an on-target check with the SDK's
 * prospero-clang, since the whole point is that the console's own strtof is
 * broken and this one must be the symbol the linker keeps.
 *
 * Build and run, exit code 0 = pass:
 *   clang test_strtof.c float_strtod.c -o test && ./test
 *
 * The expected bit patterns are the correctly-rounded IEEE754 results,
 * computed for this file with Python's struct, not guessed:
 *   python -c "import struct
 *     db = lambda x: '0x%016X' % struct.unpack('<Q', struct.pack('<d', x))[0]
 *     fl = lambda x: '0x%08X' % struct.unpack('<I', struct.pack('<f', x))[0]"
 *
 * Not linked into the applications: CI and console builds only.
 */

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char buf[64];
static int fails;

/* Through a global buffer so the compiler cannot constant-fold the call
 * around the function under test. */
static double parse_d(const char *s, char **end)
{
	strcpy(buf, s);
	errno = 0;
	return strtod(buf, end);
}

static float parse_f(const char *s, char **end)
{
	strcpy(buf, s);
	errno = 0;
	return strtof(buf, end);
}

static void check_d(const char *s, uint64_t bits, long long end_off, int erange)
{
	char *end;
	double v = parse_d(s, &end);
	uint64_t got;
	memcpy(&got, &v, sizeof got);
	if (got != bits) {
		printf("FAIL strtod(\"%s\"): bits 0x%016llX, want 0x%016llX\n",
		       s, (unsigned long long)got, (unsigned long long)bits);
		fails++;
	}
	if (end - buf != end_off) {
		printf("FAIL strtod(\"%s\"): endptr at %lld, want %lld\n",
		       s, (long long)(end - buf), end_off);
		fails++;
	}
	if ((errno == ERANGE) != erange) {
		printf("FAIL strtod(\"%s\"): errno ERANGE=%d, want %d\n",
		       s, errno == ERANGE, erange);
		fails++;
	}
}

static void check_f(const char *s, uint32_t bits, long long end_off, int erange)
{
	char *end;
	float v = parse_f(s, &end);
	uint32_t got;
	memcpy(&got, &v, sizeof got);
	if (got != bits) {
		printf("FAIL strtof(\"%s\"): bits 0x%08X, want 0x%08X\n",
		       s, got, bits);
		fails++;
	}
	if (end - buf != end_off) {
		printf("FAIL strtof(\"%s\"): endptr at %lld, want %lld\n",
		       s, (long long)(end - buf), end_off);
		fails++;
	}
	if ((errno == ERANGE) != erange) {
		printf("FAIL strtof(\"%s\"): errno ERANGE=%d, want %d\n",
		       s, errno == ERANGE, erange);
		fails++;
	}
}

int main(void)
{
	/* double: the type nlohmann::json parses into. */
	check_d("0.8", 0x3FE999999999999AULL, 3, 0);
	check_d("-1.5e3", 0xC097700000000000ULL, 6, 0);
	check_d("2.", 0x4000000000000000ULL, 2, 0);
	check_d(".5", 0x3FE0000000000000ULL, 2, 0);
	check_d("1e10", 0x4202A05F20000000ULL, 4, 0);
	check_d("0.0", 0x0000000000000000ULL, 3, 0);
	check_d("-0.0", 0x8000000000000000ULL, 4, 0);
	check_d("0.8abc", 0x3FE999999999999AULL, 3, 0);
	check_d("", 0x0000000000000000ULL, 0, 0);
	/* overflow: HUGE_VAL, errno ERANGE */
	check_d("1e310", 0x7FF0000000000000ULL, 5, 1);
	/* 17 significant digits: rounding must be exact to the bit */
	check_d("0.12345678901234567", 0x3FBF9ADD3746F65EULL, 19, 0);

	/* float: the other overload json.hpp's lexer can hit. */
	check_f("0.8", 0x3F4CCCCDu, 3, 0);
	check_f("-1.5e3", 0xC4BB8000u, 6, 0);
	check_f("2.", 0x40000000u, 2, 0);
	check_f(".5", 0x3F000000u, 2, 0);
	check_f("1e10", 0x501502F9u, 4, 0);
	check_f("0.0", 0x00000000u, 3, 0);
	check_f("-0.0", 0x80000000u, 4, 0);
	check_f("0.8abc", 0x3F4CCCCDu, 3, 0);
	check_f("", 0x00000000u, 0, 0);
	/* overflow: HUGE_VALF, errno ERANGE */
	check_f("1e310", 0x7F800000u, 5, 1);
	check_f("0.12345678901234567", 0x3DFCD6EAu, 19, 0);
	check_f("0.123456789", 0x3DFCD6EAu, 11, 0);

	if (fails) {
		printf("%d failure(s)\n", fails);
		return 1;
	}
	printf("all %d vectors pass\n", 11 + 12);
	return 0;
}
