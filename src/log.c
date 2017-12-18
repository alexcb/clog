#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>


#define LOG_BUF_SIZE 1024

bool needs_quotes( const char *s, int n )
{
	for( int i = 0; i < n; i++ ) {
		if( s[i] == '\0' ) {
			return false;
		}
		if( s[i] == ' ' || s[i] == '"' ) {
			return true;
		}
	}
	return false;
}

int escape_binary( char *buf, int buf_size, const char *s, int n, int *num_written, bool escape_doublequote )
{
	char *start = buf;
	for( int i = 0; i < n; i++ ) {
		char c = s[i];
		if( c == '\\' ) {
			if( buf_size <= 1 ) {
				*num_written = buf - start;
				return LOG_ERROR_OUT_OF_SPACE;
			}
			buf[0] = '\\';
			buf[1] = '\\';
		} else if( c == '\t' ) {
			if( buf_size <= 1 ) {
				*num_written = buf - start;
				return LOG_ERROR_OUT_OF_SPACE;
			}
			buf[0] = '\\';
			buf[1] = 't';
		} else if( c == '\n' ) {
			if( buf_size <= 1 ) {
				*num_written = buf - start;
				return LOG_ERROR_OUT_OF_SPACE;
			}
			buf[0] = '\\';
			buf[1] = 'n';
		} else if( c == '"' && escape_doublequote ) {
			if( buf_size <= 1 ) {
				*num_written = buf - start;
				return LOG_ERROR_OUT_OF_SPACE;
			}
			buf[0] = '\\';
			buf[1] = 'n';
		} else if( 32 <= c && c <= 126 ) {
			buf[0] = c;
			buf++;
			buf_size--;
			if( buf_size == 0 ) {
				*num_written = buf - start;
				return LOG_ERROR_OUT_OF_SPACE;
			}
		} else {
			if( buf_size <= 4 ) {
				*num_written = buf - start;
				return LOG_ERROR_OUT_OF_SPACE;
			}
			buf[0] = '\\';
			buf[1] = 'x';
			sprintf(&buf[2], "%02hhX", c);
			buf += 4;
			buf_size -= 4;
		}
	}
	*num_written = buf - start;
	return LOG_OK;
}

int tokenize_key(const char *s, const char **tok, int *n)
{
	const char *start = s;
	while( *start && (*start == ' ' || *start == '\t') ) {
		start++;
	}
	const char *p = start;
	while( *p ) {
		if( *p == ' ' ) {
			return LOG_ERROR_UNEXPECTED_CHAR;
		}
		if( *p == '=' ) {
			if( start == p ) {
				return LOG_ERROR_UNEXPECTED_CHAR;
			}
			*tok = start;
			*n = p - start;
			return LOG_OK;
		}
		p++;
	}
	return LOG_ERROR_UNEXPECTED_CHAR;
}

int tokenize_value_fmt(const char *s, const char **tok, int *n)
{
	//if( s != '%' ) {
	//	return ERROR_UNEXPECTED_CHAR;
	//}
	//s++;

	const char *p = s;
	while( *p != ' ' && *p ) {
		p++;
	}
	if( s == p ) {
		return LOG_ERROR_UNEXPECTED_CHAR;
	}
	*tok = s;
	*n = p - s;
	return LOG_OK;
}

int append_quoted_string_n( char *buf, int buf_size, const char *s, int n, int *num_written )
{
	int m;
	int err;
	if( buf_size <= 0 ) {
		return 0;
	}
	if( needs_quotes( s, n ) == false ) {
		if( n > buf_size ) {
			n = buf_size;
		}

		return escape_binary( buf, buf_size, s, n, num_written, false);
	}

	char *start = buf;
	buf[0] = '"';
	buf++;
	buf_size--;
	m = 0;
	err = escape_binary( buf, buf_size, s, n, &m, true);
	assert( buf_size >= m );
	buf += m;
	buf_size -= m;
	if( buf_size == 0 || err == LOG_ERROR_OUT_OF_SPACE ) {
		*num_written = buf - start;
		return LOG_ERROR_OUT_OF_SPACE;
	}
	buf[0] = '"';
	buf++;
	buf_size--;
	*num_written = buf - start;
	return 0;
}

int append_quoted_string( char *buf, int buf_size, const char *s, int *num_written )
{
	return append_quoted_string_n( buf, buf_size, s, strlen(s), num_written );
}

int _slog_args(char *buf, size_t buf_size, const char *fmt, va_list arguments)
{
	int res;
	int key_n, val_fmt_n;
	const char *key, *val_fmt;

	int buf_i = 0;
	int m;
	int err;

	if( buf_size <= 2 ) {
		return 1;
	}
	// reserve spaces for "\n\0" at the end
	buf_size -= 2;

	const char *p = fmt;
	while( 1 ) {
		res = tokenize_key( p, &key, &key_n );
		if( res != LOG_OK ) {
			break;
		}
		p = key + key_n;

		assert( *p == '=' );
		p++;

		res = tokenize_value_fmt( p, &val_fmt, &val_fmt_n );
		assert( res == LOG_OK );
		p = val_fmt + val_fmt_n;

		if( buf_size - buf_i > key_n + 1 ) {
			strncpy( buf+buf_i, key, key_n );
			buf_i += key_n;
			buf[buf_i] = '=';
			buf_i++;
		}

		if( val_fmt_n == 1 && val_fmt[0] == 's' ) {
			const char *val = va_arg( arguments, const char* );
			err = append_quoted_string( buf+buf_i, buf_size - buf_i, val, &m );
			if( err == LOG_ERROR_OUT_OF_SPACE ) {
				buf_i += m;
				err = 1;
				goto error;
			} else if( err != LOG_OK ) {
				return 1;
			}
			buf_i += m;
		} else if( val_fmt_n == 1 && val_fmt[0] == 'd' ) {
			int val = va_arg( arguments, int );
			snprintf( buf+buf_i, buf_size - buf_i, "%d", val );
			buf_i += strlen(buf+buf_i);
		} else if( val_fmt_n == 1 && val_fmt[0] == 'p' ) {
			void* val = va_arg( arguments, void* );
			snprintf( buf+buf_i, buf_size - buf_i, "%p", val );
			buf_i += strlen(buf+buf_i);
		} else if( val_fmt_n == 2 && val_fmt[0] == '*' && val_fmt[1] == 's' ) {
			int n = va_arg( arguments, int );
			const char *val = va_arg( arguments, const char* );
			m = 0;
			err = append_quoted_string_n( buf+buf_i, buf_size - buf_i, val, n, &m );
			if( err == LOG_ERROR_OUT_OF_SPACE ) {
				buf_i += m;
				err = 1;
				goto error;
			} else if( err != LOG_OK ) {
				return 1;
			}
			buf_i += m;
		} else {
			printf("Unhandled format: %.*s=%.*s\n", key_n, key, val_fmt_n, val_fmt);
			assert( 0 );
		}

		if( (buf_size - buf_i) > 1 ) {
			buf[buf_i] = ' ';
			buf_i++;
		}
	}
	while( *p && *p == ' ' ) {
		p++;
	}
	if( *p ) {
		err = append_quoted_string( buf+buf_i, buf_size - buf_i, "msg=", &m );
		if( err != LOG_OK && err != LOG_ERROR_OUT_OF_SPACE ) {
			return 1;
		}
		buf_i += m;

		err = append_quoted_string( buf+buf_i, buf_size - buf_i, p, &m );
		if( err != LOG_OK && err != LOG_ERROR_OUT_OF_SPACE ) {
			return 1;
		}
		buf_i += m;
	}

	err = 0;
error:
	// two bytes were initially reserved at the top of this function
	assert( (buf_i+1) < (buf_size+2) );
	buf[buf_i] = '\n';
	buf[buf_i+1] = '\0';

	return err;
}

void _slog(char *buf, size_t buf_size, const char *fmt, ...)
{
	va_list arguments;
	va_start( arguments, fmt );
	_slog_args(buf, buf_size, fmt, arguments);
	va_end( arguments );
}

void _log(const char *fmt, ...)
{
	char buf[LOG_BUF_SIZE];

	va_list arguments;
	va_start( arguments, fmt );
	_slog_args(buf, LOG_BUF_SIZE, fmt, arguments);
	va_end( arguments );

	fputs( buf, stderr );
	fflush( stderr );
}
