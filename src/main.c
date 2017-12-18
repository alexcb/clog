#include "log.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

int append_quoted_string_n( char *buf, int buf_size, const char *s, int n, int *num_written );

void _slog(char *buf, size_t buf_size, const char *fmt, ...);

int escape_binary( char *buf, int buf_size, const char *s, int n, int *num_written, bool escape_doublequote );

void assert_chars(const char *buf, int start, int end, char c)
{
	for( int i = start; i < end; i++ ) {
		assert( buf[i] == c );
	}
}

void test1() {
	char buf[32];
	memset(buf, 'x', 32);
	int n = 0;
	int err = append_quoted_string_n( buf, 31, "abcd", 2, &n );
	assert( err == LOG_OK );
	assert( n == 2 );
	assert_chars( buf, 2, 32, 'x' );
	buf[n] = '\0';
	assert( strlen(buf) == 2 );
	assert( strcmp(buf, "ab") == 0 );
}

void test2() {
	char buf[32];
	memset(buf, 'x', 32);
	int n = 0;
	int err = append_quoted_string_n( buf, 4, "abcdefg", 4, &n );
	assert( err == LOG_OK );
	assert( n == 4 );
	assert_chars( buf, 5, 32, 'x' );
	buf[n] = '\0';
	assert( strlen(buf) == 4 );
	assert( strcmp(buf, "abcd") == 0 );
}

void test3() {
	char buf[32];
	memset(buf, 'x', 32);
	int n = 0;
	int err = append_quoted_string_n( buf, 0, "abcdefg", 4, &n );
	assert( err == LOG_OK );
	assert( n == 0 );
	assert_chars( buf, 0, 32, 'x' );
}

void test4() {
	char buf[32];
	memset(buf, 'x', 32);
	int n = 0;
	int err = append_quoted_string_n( buf, 32, "a cdefg", 4, &n );
	assert( err == LOG_OK );
	assert( n == 6 );
	assert_chars( buf, 6, 32, 'x' );
	buf[n] = '\0';
	assert( strcmp(buf, "\"a cd\"") == 0 );
}

void test5() {
	char buf[32];
	memset(buf, 'x', 32);
	int n = 0;
	int err = append_quoted_string_n( buf, 3, "a cdefg", 4, &n );
	assert( err == LOG_OK );
	assert( n == 3 );
	assert_chars( buf, 3, 32, 'x' );
	buf[n] = '\0';
	assert( strcmp(buf, "\"a ") == 0 );
}

void test6() {
	char buf[32];
	memset(buf, 'x', 32);
	_slog(buf, 32, "key=s hello world", "some_value");
	assert( strlen(buf) == 31);
	assert( strcmp(buf, "key=some_value msg=\"hello worl\n") == 0 );
}

void test_utf8_non_quoted() {
	char buf[32];
	memset(buf, 'x', 32);
	_slog(buf, 32, "key=s foo", "\xE2\x88\x83y");
	printf("got %s\n", buf);
	assert( strlen(buf) == 26);
	assert( strcmp(buf, "key=\\xE2\\x88\\x83y msg=foo\n") == 0 );
}

void test_utf8_quoted() {
	char buf[32];
	memset(buf, 'x', 32);
	_slog(buf, 32, "key=s foo", "\xE2\x88\x83 y");
	printf("got %s\n", buf);
	assert( strlen(buf) == 29);
	assert( strcmp(buf, "key=\"\\xE2\\x88\\x83 y\" msg=foo\n") == 0 );
}

void test_escape_binary()
{
	char buf[32];
	memset(buf, 'x', 32);
	int n;
	int err = escape_binary(buf, 31, "\xE2\x88\x83y", 4, &n, false);
	assert( err == LOG_OK );
	assert( n == 13 );
	buf[n] = '\0';
	printf("got \"%s\"\n", buf);
	assert( strcmp(buf, "\\xE2\\x88\\x83y") == 0 );
}

void test_escape_binary_overflow()
{
	char buf[8];
	memset(buf, 'x', 8);
	int n;
	int err = escape_binary(buf, 7, "a\xFF\xFFy", 3, &n, false);
	assert( err == LOG_ERROR_OUT_OF_SPACE );
	printf("got \"%d\"\n", n);
	assert( n == 5 );
	buf[n] = '\0';
	printf("got \"%s\"\n", buf);
}

int main() {
	test1();
	//test2();
	//test3();
	//test4();
	//test5();
	//test6();
	//test_utf8_non_quoted();
	//test_utf8_quoted();
	//test_escape_binary();
	//test_escape_binary_overflow();

	char longstr[2000];
	for( int i = 0; i < 10; i++ ) {
		longstr[i] = 'a' + i;
	}
	for( int i = 10; i < 2000/3; i += 3 ) {
		longstr[i] = '\xE2';
		longstr[i+1] = '\x88';
		longstr[i+2] = '\x83';
	}
	longstr[0] = ' ';
	longstr[1999] = '\0';

	const char *utf8data = "\xE2\x88\x83y \xE2\x88\x80x \xC2\xAC(x \xE2\x89\xBA y)";
	const char *color = "green";
	LOG_INFO("color=s data=s long=s hello", color, utf8data, longstr);
	return 0;
}
