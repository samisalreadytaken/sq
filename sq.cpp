//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sq
//-----------------------------------------------------------------------
//

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <float.h>
#include <new>

#ifdef _WIN32
	#ifdef _DEBUG
		#include <crtdbg.h>
	#endif
	#include <io.h>
	#include <fcntl.h>
	#define WIN32_LEAN_AND_MEAN
	#ifdef __MINGW32__
		#include <windows.h>
	#else
		#include <Windows.h>
	#endif
	#ifdef SQDBG_NATIVE_STACKTRACE
		#ifdef __MINGW32__
			#include <dbghelp.h>
			#include <psapi.h>
		#else
			#include <DbgHelp.h>
			#include <Psapi.h>
		#endif
		#ifdef _MSC_VER
			#pragma comment(lib, "dbghelp.lib")
			#pragma comment(lib, "psapi.lib")
		#endif
	#endif
	#undef Yield
#else
	#include <time.h>
	#include <errno.h>
	#include <signal.h>
	#include <termios.h>
	#include <unistd.h>
	#include <poll.h>
	#include <sys/fcntl.h>
	#include <setjmp.h>
	#include <wchar.h>

	#define _fileno fileno
	#define _setmode setmode
#endif

#ifdef SQFFI
	#include <time.h>
	#include <malloc.h>
	#ifndef _WIN32
		#include <dlfcn.h>
	#endif
	#include <ffi.h>
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define ___CAT(a, b) a##b
#define __CAT(a, b) ___CAT(a,b)

#if 1
#include <../sqdbg/debug.h>
#ifdef _DEBUG
	#ifdef _WIN32
		#include <debugapi.h>

		inline const char *GetModuleBaseName()
		{
			static char module[MAX_PATH];
			int len = GetModuleFileNameA( NULL, module, sizeof(module) );

			if ( len != 0 )
			{
				for ( char *pBase = module + len; pBase-- > module; )
				{
					if ( *pBase == '\\' )
						return pBase + 1;
				}

				return module;
			}

			return "";
		}
	#endif
	#define assert(...) Assert( __VA_ARGS__ )
#else
	#define assert(...) (void)0
#endif
#else
#ifdef _DEBUG
	#include <assert.h>
	#define Assert( x ) assert(x)
#else
	#define assert( x ) (void)0
	#define Assert( x ) (void)0
#endif

#define STATIC_ASSERT( x ) static_assert( x, #x )
#ifdef _MSC_VER
	#define UNREACHABLE() do { Assert(!"UNREACHABLE"); __assume(0); } while(0)
#else
	#define UNREACHABLE() do { Assert(!"UNREACHABLE"); __builtin_unreachable(); } while(0)
#endif
#endif

#if !defined(SQDBG_DLL) || !defined(_WIN32)
#define HAS_SQCLOSURE_TYPE
#endif

#include <squirrel.h>
#include <sqdbg.h>

#include <sqobject.h>
#include <sqstate.h>
#include <sqvm.h>
#include <sqstring.h>
#include <sqarray.h>
#include <sqtable.h>
#include <sqclass.h>
#include <squserdata.h>
#ifdef HAS_SQCLOSURE_TYPE
#include <sqfuncproto.h>
#include <sqclosure.h>
#endif
#ifndef NOSQSTDLIB
#include <sqstdblob.h>
#include <sqstdsystem.h>
#include <sqstdio.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdaux.h>
#endif

#undef _SC
#ifdef SQUNICODE
	#define _SC(s) __CAT( L, s )
#else
	#define _SC(s) s
#endif

#ifdef _WIN32
void _sleep( int ms )
{
	::Sleep( (DWORD)ms );
}
#else
void _sleep( int ms )
{
	timespec t;
	t.tv_nsec = ms * 1000000;
	t.tv_sec = 0;
	nanosleep( &t, NULL );
}
#endif


////////////////////////////////////////////////
// Common utility from sqdbg

#define ALIGN(v, a) (((v) + ((a)-1)) & ~((a)-1))

#ifdef _DEBUG
class CStackCheck
{
private:
	HSQUIRRELVM vm;
	int top;

public:
	CStackCheck( HSQUIRRELVM v )
	{
		vm = v;
		top = vm->_top;
	}

	~CStackCheck()
	{
		Assert( vm->_top == top );
	}
};
#define STACKCHECK( vm ) CStackCheck stackcheck( vm )
#else
#define STACKCHECK( vm ) (void)0
#endif

#ifndef scstricmp
#ifdef SQUNICODE
	#ifdef _WIN32
		#define scstricmp _wcsicmp
	#else
		#define scstricmp sq_wcsicmp
		int sq_wcsicmp( const SQChar *s1, const SQChar *s2 )
		{
			for (;;)
			{
				SQChar c1 = *s1++;
				SQChar c2 = *s2++;

				if ( !c1 || !c2 )
					return c1 - c2;

				if ( c1 == c2 )
					continue;

				if ( c1 >= 'A' && c1 <= 'Z' )
					c1 |= 0x20;

				if ( c2 >= 'A' && c2 <= 'Z' )
					c2 |= 0x20;

				if ( c1 == c2 )
					continue;

				return c1 - c2;
			}
		}
	#endif
#else
	#ifdef _WIN32
		#define scstricmp _stricmp
	#else
		#define scstricmp strcasecmp
	#endif
#endif
#endif

#undef scsprintf
#ifdef SQUNICODE
	#define scsprintf swprintf
#else
	#define scsprintf snprintf
#endif

#ifdef _SQ64
	#if defined(_WIN32) || SQUIRREL_VERSION_NUMBER >= 300
		#define FMT_INT "%lld"
	#else
		#define FMT_INT "%ld"
	#endif
	#if defined(_WIN32)
		#define FMT_PTR "%016llX"
	#else
		#define FMT_PTR "%016lX"
	#endif
#else
	#define FMT_INT "%d"
	#define FMT_PTR "%08X"
#endif

#ifdef SQUNICODE
	#define FMT_STR "%ls"
	#define FMT_VSTR "%.*ls"
	#define FMT_STR50 "%.50ls"
	#define FMT_CSTR "%hs"
	#define FMT_CSTR_S "hs"
	#define FMT_VCSTR "%.*hs"
#else
	#define FMT_STR "%s"
	#define FMT_VSTR "%.*s"
	#define FMT_STR50 "%.50s"
	#define FMT_CSTR "%s"
	#define FMT_CSTR_S "s"
	#define FMT_VCSTR "%.*s"
#endif

#ifdef SQUSEDOUBLE
	#define FMT_FLT_DIG_STR "17"
#else
	#define FMT_FLT_DIG_STR "9"
#endif

#ifdef _WIN32
	typedef uint16_t uwchar_t;
#else
	typedef unsigned int uwchar_t;
#endif

STATIC_ASSERT( sizeof(wchar_t) == sizeof(uwchar_t) );
#ifdef SQUNICODE
STATIC_ASSERT( sizeof(wchar_t) == sizeof(SQChar) );
#endif

#if SQUIRREL_VERSION_NUMBER >= 300
	#define _fp(func) (func)
	#define CLOSURE_ENV_ISVALID(env) (env)
#else
	#define _fp(func) _funcproto(func)
	#define CLOSURE_ENV_ISVALID(env) (sq_type(env) == OT_WEAKREF && _weakref(env))
#endif

#define IN_RANGE(c, min, max) \
	((uint32_t)((uint32_t)(c) - (uint32_t)(min)) <= (uint32_t)((max)-(min)))
#define IN_RANGE_CHAR(c, min, max) \
	((unsigned char)((unsigned char)(c) - (unsigned char)(min)) <= (unsigned char)((max)-(min)))
#define UTF8_TRAIL(c) ( ( (c) & 0xC0 ) == 0x80 )
#define UTF_SURROGATE(cp) ( ( (cp) & 0xFFFFF800 ) == 0x0000D800 )
#define UTF_SURROGATE_LEAD(cp) ( ( (cp) & 0xFFFFFC00 ) == 0x0000D800 )
#define UTF_SURROGATE_TRAIL(cp) ( ( (cp) & 0xFFFFFC00 ) == 0x0000DC00 )
#define UTF8_2_LEAD(c) ( ( (c) & 0xE0 ) == 0xC0 )
#define UTF8_3_LEAD(c) ( ( (c) & 0xF0 ) == 0xE0 )
#define UTF8_4_LEAD(c) ( ( (c) & 0xF8 ) == 0xF0 )
#define UTF8_2(len, c0, src) \
	( (len) > 1 && UTF8_TRAIL((src)[1]) )
#define UTF8_3(len, c0, src) \
	( (len) > 2 && UTF8_3_ISVALID(c0, (src)[1]) && UTF8_TRAIL((src)[2]) )
#define UTF8_4(len, c0, src) \
	( (len) > 3 && UTF8_4_ISVALID(c0, (src)[1]) && UTF8_TRAIL((src)[2]) && UTF8_TRAIL((src)[3]) )
#define UTF8_3_ISVALID(c0, c1) \
	( ( c0 == 0xE0 && IN_RANGE(c1, 0xA0, 0xBF) ) || \
	  ( c0 == 0xED && IN_RANGE(c1, 0x80, 0x9F) ) || \
	  ( UTF8_TRAIL(c1) && (IN_RANGE(c0, 0xE1, 0xEC) || IN_RANGE(c0, 0xEE, 0xEF)) ) )
#define UTF8_4_ISVALID(c0, c1) \
	( ( c0 == 0xF0 && IN_RANGE(c1, 0x90, 0xBF) ) || \
	  ( c0 == 0xF4 && IN_RANGE(c1, 0x80, 0x8F) ) || \
	  ( UTF8_TRAIL(c1) && IN_RANGE(c0, 0xF1, 0xF3) ) )
#define UTF32_FROM_UTF8_2(c0, c1) \
	( ( ( (c0) & 0x1F ) << 6 ) | \
	    ( (c1) & 0x3F ) )
#define UTF32_FROM_UTF8_3(c0, c1, c2) \
	( ( ( (c0) & 0x0F ) << 12 ) | \
	  ( ( (c1) & 0x3F ) << 6 ) | \
	    ( (c2) & 0x3F ) )
#define UTF32_FROM_UTF8_4(c0, c1, c2, c3) \
	( ( ( (c0) & 0x07 ) << 18 ) | \
	  ( ( (c1) & 0x3F ) << 12 ) | \
	  ( ( (c2) & 0x3F ) << 6 ) | \
	    ( (c3) & 0x3F ) )
#define UTF32_FROM_UTF16_SURROGATE(lead, trail) \
	( ( ( ( (lead) & 0x3FF ) << 10 ) | ( (trail) & 0x3FF ) ) + 0x10000 )
#define UTF16_SURROGATE_FROM_UTF32(dst, cp) \
do { \
	(dst)[0] = 0xD800 | ( (cp - 0x10000) >> 10 ); \
	(dst)[1] = 0xDC00 | ( (cp - 0x10000) & 0x3FF ); \
} while (0)
#define UTF8_2_FROM_UTF32(mbc, cp) \
do { \
	(mbc)[0] = 0xC0 | ( (cp) >> 6 ); \
	(mbc)[1] = 0x80 | ( (cp) & 0x3F ); \
} while (0)
#define UTF8_3_FROM_UTF32(mbc, cp) \
do { \
	(mbc)[0] = 0xE0 | ( (cp) >> 12 ); \
	(mbc)[1] = 0x80 | ( ( (cp) >> 6 ) & 0x3F ); \
	(mbc)[2] = 0x80 | ( (cp) & 0x3F ); \
} while (0)
#define UTF8_4_FROM_UTF32(mbc, cp) \
do { \
	(mbc)[0] = 0xF0 | ( (cp) >> 18 ); \
	(mbc)[1] = 0x80 | ( ( (cp) >> 12 ) & 0x3F ); \
	(mbc)[2] = 0x80 | ( ( (cp) >> 6 ) & 0x3F ); \
	(mbc)[3] = 0x80 | ( (cp) & 0x3F ); \
} while (0)

static int IsValidUTF8( const char *src, int srclen );
#ifdef SQUNICODE
static int IsValidUnicode( const wchar_t *src, unsigned int srclen );
#endif
static char *UTF8Start( char *ptr, const char *mem );
static bool IsDoubleWidth( const char *src );
static unsigned int WCharToUTF8( char *dst, unsigned int destSize, const wchar_t *src, unsigned int srclen );
#if defined(SQUNICODE) || defined(_WIN32)
static unsigned int UTF8ToWChar( wchar_t *dst, unsigned int destSize, const char *src, unsigned int srclen );
#endif
#ifdef SQUNICODE
static void PrintWCharToUTF8( FILE *fd, const wchar_t *src, unsigned int srclen, char escape = 0 );
#endif
#ifndef SQUNICODE
static void PrintEscaped( FILE *fd, const char *src, unsigned int srclen, bool json = false );
#endif
template < typename C >
static int atoi( const C *ptr, int len );


////////////////////////////////////////////////

int IsValidUTF8( const char *src, int srclen )
{
	unsigned char cp = ((unsigned char*)src)[0];

	if ( cp <= 0x7E )
	{
		if ( cp >= 0x20 )
			return 1;

		return 0;
	}
	else if ( IN_RANGE_CHAR( cp, 0xC2, 0xF4 ) )
	{
		if ( UTF8_2_LEAD(cp) )
		{
			if ( UTF8_2( srclen, cp, (unsigned char*)src ) )
			{
				return 2;
			}
		}
		else if ( UTF8_3_LEAD(cp) )
		{
			if ( UTF8_3( srclen, cp, (unsigned char*)src ) )
			{
				return 3;
			}
		}
		else if ( UTF8_4_LEAD(cp) )
		{
			if ( UTF8_4( srclen, cp, (unsigned char*)src ) )
			{
				return 4;
			}
		}
	}

	return 0;
}

#ifdef SQUNICODE
int IsValidUnicode( const wchar_t *src, unsigned int srclen )
{
	uint32_t cp = (uint32_t)((uwchar_t*)src)[0];

	if ( cp <= 0x7E )
	{
		if ( cp >= 0x20 )
			return 1;

		return 0;
	}
	else if ( cp < 0xA0 )
	{
		return 0;
	}

	if ( cp <= 0xFFFF )
	{
		if ( UTF_SURROGATE(cp) )
		{
			if ( srclen > 1 && UTF_SURROGATE_LEAD(cp) && UTF_SURROGATE_TRAIL(src[1]) )
			{
				return 2;
			}

			return -1;
		}

		return 1;
	}
	else if ( cp <= 0x10FFFF )
	{
		return 2;
	}
	else
	{
		return -1;
	}
}
#endif

char *UTF8Start( char *ptr, const char *mem )
{
	Assert( ptr >= mem );

	if ( !UTF8_TRAIL(*ptr) )
		return ptr;

	int len = 1;
	const char *lim = ptr - 4;

	if ( lim < mem )
		lim = mem;

	for (;;)
	{
		ptr--;
		len++;

		if ( ptr < lim )
			return NULL;

		Assert( ptr >= mem );

		if ( UTF8_TRAIL(*ptr) )
			continue;

		if ( IsValidUTF8( ptr, len ) )
			return ptr;

		return NULL;
	}
}

bool IsDoubleWidth( const char *src )
{
	uint32_t cp = (uint32_t)((unsigned char*)src)[0];

	if ( cp < 0xC2 )
	{
		return false;
	}
	else if ( UTF8_3_LEAD(cp) && UTF8_3( 3, cp, (unsigned char*)src ) )
	{
		cp = UTF32_FROM_UTF8_3( cp, src[1], src[2] );
	}
	else if ( UTF8_4_LEAD(cp) && UTF8_4( 4, cp, (unsigned char*)src ) )
	{
		cp = UTF32_FROM_UTF8_4( cp, src[1], src[2], src[3] );
	}
	else
	{
		return false;
	}

	// http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
	return (cp >= 0x1100 &&
			(cp <= 0x115F || cp == 0x2329 || cp == 0x232A ||
			(cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) ||
			(cp >= 0xAC00 && cp <= 0xD7A3) ||
			(cp >= 0xF900 && cp <= 0xFAFF) ||
			(cp >= 0xFE10 && cp <= 0xFE19) ||
			(cp >= 0xFE30 && cp <= 0xFE6F) ||
			(cp >= 0xFF00 && cp <= 0xFF60) ||
			(cp >= 0xFFE0 && cp <= 0xFFE6) ||
			(cp >= 0x10000 && cp <= 0x1FFFF) || // SMP
			(cp >= 0x20000 && cp <= 0x2FFFD) ||
			(cp >= 0x30000 && cp <= 0x3FFFD)));
}

unsigned int WCharToUTF8( char *dst, unsigned int destSize, const wchar_t *src, unsigned int srclen )
{
	uint32_t cp;
	const wchar_t *end = src + srclen;
	unsigned char mbc[4];
	unsigned int count = 0;
	unsigned int bytes;

	for ( ; src < end; src++ )
	{
		cp = (uint32_t)((uwchar_t*)src)[0];

		if ( cp <= 0x7F )
		{
			mbc[0] = (unsigned char)cp;
			bytes = 1;
		}
		else if ( cp <= 0x7FF )
		{
			UTF8_2_FROM_UTF32( mbc, cp );
			bytes = 2;
		}
		else if ( cp <= 0xFFFF )
		{
			if ( UTF_SURROGATE(cp) )
			{
				if ( src + 1 < end && UTF_SURROGATE_LEAD(cp) && UTF_SURROGATE_TRAIL(src[1]) )
				{
					cp = UTF32_FROM_UTF16_SURROGATE( cp, (uint32_t)((uwchar_t*)src)[1] );
					src++;
					goto supplementary;
				}
			}

			UTF8_3_FROM_UTF32( mbc, cp );
			bytes = 3;
		}
		else
		{
supplementary:
			UTF8_4_FROM_UTF32( mbc, cp );
			bytes = 4;
		}

		if ( dst )
		{
			if ( bytes <= destSize )
			{
				memcpy( dst, mbc, bytes );
				dst += bytes;
				destSize -= bytes;
				count += bytes;
			}
			else
			{
				break;
			}
		}
		else
		{
			count += bytes;
		}
	}

	return count;
}

#if defined(SQUNICODE) || defined(_WIN32)
unsigned int UTF8ToWChar( wchar_t *dst, unsigned int destSize, const char *src, unsigned int srclen )
{
	uint32_t cp;
	const char *end = src + srclen;
	unsigned int count = 0;

	for ( ; src < end; src++ )
	{
		cp = (uint32_t)((unsigned char*)src)[0];

		if ( cp <= 0x7E )
		{
			goto single;
		}
		else if ( IN_RANGE( cp, 0xC2, 0xF4 ) )
		{
			if ( UTF8_2_LEAD(cp) )
			{
				if ( UTF8_2( end - src, cp, (unsigned char*)src ) )
				{
					cp = UTF32_FROM_UTF8_2( cp, src[1] );
					src += 1;
					goto single;
				}
			}
			else if ( UTF8_3_LEAD(cp) )
			{
				if ( UTF8_3( end - src, cp, (unsigned char*)src ) )
				{
					cp = UTF32_FROM_UTF8_3( cp, src[1], src[2] );
					src += 2;
					goto single;
				}
			}
			else if ( UTF8_4_LEAD(cp) )
			{
				if ( UTF8_4( end - src, cp, (unsigned char*)src ) )
				{
					cp = UTF32_FROM_UTF8_4( cp, src[1], src[2], src[3] );
					src += 3;
					if ( sizeof(wchar_t) == 2 )
					{
						goto supplementary;
					}
					else if ( sizeof(wchar_t) == 4 )
					{
						goto single;
					}
					else UNREACHABLE();
				}
			}

			goto single;
		}
		else
		{
			goto single;
		}

single:
		if ( dst )
		{
			if ( sizeof(wchar_t) <= destSize )
			{
				*dst++ = (wchar_t)cp;
				destSize -= sizeof(wchar_t);
				count += 1;
			}
			else
			{
				break;
			}
		}
		else
		{
			count += 1;
		}

		continue;

supplementary:
		if ( sizeof(wchar_t) == 2 )
		{
			if ( dst )
			{
				if ( sizeof(wchar_t) * 2 <= destSize )
				{
					UTF16_SURROGATE_FROM_UTF32( dst, cp );
					dst += 2;
					destSize -= sizeof(wchar_t) * 2;
					count += 2;
				}
				else
				{
					cp = 0xFFFD;
					goto single;
				}
			}
			else
			{
				count += 2;
			}

			continue;
		}
	}

	return count;
}
#endif

#ifdef SQUNICODE
void PrintWCharToUTF8( FILE *fd, const wchar_t *src, unsigned int srclen, char escape )
{
	uint32_t cp;
	const wchar_t *end = src + srclen;
	const wchar_t *ptr = src;
	unsigned char mbc[4];

	if ( escape )
		putc( '\"', fd );

	for ( ; src < end; src++ )
	{
		cp = (uint32_t)((uwchar_t*)src)[0];

		if ( escape )
		{
			if ( *ptr == '\\' || *ptr == '\"' ||
				*ptr == '\a' || *ptr == '\b' || *ptr == '\f' ||
				*ptr == '\n' || *ptr == '\r' || *ptr == '\t' || *ptr == '\v' )
			{
				putc( '\\', fd );

				if ( escape == 1 )
				{
					switch ( *ptr )
					{
						case '\\':
						case '\"': putc( *ptr, fd ); break;
						case '\a': putc( 'a', fd ); break;
						case '\b': putc( 'b', fd ); break;
						case '\f': putc( 'f', fd ); break;
						case '\n': putc( 'n', fd ); break;
						case '\r': putc( 'r', fd ); break;
						case '\t': putc( 't', fd ); break;
						case '\v': putc( 'v', fd ); break;
						default: UNREACHABLE();
					}
				}
				else
				{
					fprintf( fd, "\\u%04X", *(unsigned char*)ptr );
				}

				continue;
			}
			else if ( !IsValidUnicode( ptr, srclen - (unsigned int)( ptr - src ) ) )
			{
				if ( cp < 0x10FFFF )
				{
					fprintf( fd, "\\u%04X", *(uwchar_t*)ptr );
				}
				else
				{
					uint16_t s[2];
					UTF16_SURROGATE_FROM_UTF32( s, cp );
					fprintf( fd, "\\u%04X\\u%04X", s[0], s[1] );
				}

				continue;
			}
		}

		if ( cp <= 0x7F )
		{
			mbc[0] = (unsigned char)cp;
			putc( mbc[0], fd );
		}
		else if ( cp <= 0x7FF )
		{
			UTF8_2_FROM_UTF32( mbc, cp );
			fwrite( mbc, 1, 2, fd );
		}
		else if ( cp <= 0xFFFF )
		{
			if ( UTF_SURROGATE(cp) )
			{
				if ( src + 1 < end && UTF_SURROGATE_LEAD(cp) && UTF_SURROGATE_TRAIL(src[1]) )
				{
					cp = UTF32_FROM_UTF16_SURROGATE( cp, (uint32_t)((uwchar_t*)src)[1] );
					src++;
					goto supplementary;
				}
			}

			UTF8_3_FROM_UTF32( mbc, cp );
			fwrite( mbc, 1, 3, fd );
		}
		else
		{
supplementary:
			UTF8_4_FROM_UTF32( mbc, cp );
			fwrite( mbc, 1, 4, fd );
		}
	}

	if ( escape )
		putc( '\"', fd );
}
#endif

#ifndef SQUNICODE
void PrintEscaped( FILE *fd, const char *src, unsigned int srclen, bool json )
{
	const char *end = src + srclen;
	const char *ptr = src;
	const char *last = src;
	int bytes;

	putc( '\"', fd );

	for ( ; ptr < end; ptr += bytes )
	{
		bytes = IsValidUTF8( ptr, srclen - (unsigned int)( ptr - src ) );

		if ( !bytes ||
				*ptr == '\\' || *ptr == '\"' ||
				*ptr == '\a' || *ptr == '\b' || *ptr == '\f' ||
				*ptr == '\n' || *ptr == '\r' || *ptr == '\t' || *ptr == '\v' )
		{
			if ( ptr != last )
				fwrite( last, 1, (int)( ptr - last ), fd );

			putc( '\\', fd );

			switch ( *ptr )
			{
				case '\\':
				case '\"': putc( *ptr, fd ); break;
				case '\a': putc( 'a', fd ); break;
				case '\b': putc( 'b', fd ); break;
				case '\f': putc( 'f', fd ); break;
				case '\n': putc( 'n', fd ); break;
				case '\r': putc( 'r', fd ); break;
				case '\t': putc( 't', fd ); break;
				case '\v': putc( 'v', fd ); break;
				default: fprintf( fd, !json ? "x%02X" : "u%04X", *(unsigned char*)ptr );
			}

			bytes = 1;
			last = ptr + 1;
		}
	}

	if ( last < end )
		fwrite( last, 1, (int)( end - last ), fd );

	putc( '\"', fd );
}
#endif

template < typename C >
int atoi( const C *ptr, int len )
{
	Assert( ptr && len > 0 );

	int val = 0;
	bool neg = ( *ptr == '-' );

	if ( neg )
	{
		ptr++;
		len--;
	}

	for ( ; len--; ptr++ )
	{
		C ch = *ptr;

		if ( IN_RANGE_CHAR( ch, '0', '9' ) )
		{
			val = val * 10 + ch - '0';
		}
		else
		{
			return 0;
		}
	}

	return !neg ? val : -val;
}


////////////////////////////////////////////////

#if SQUIRREL_VERSION_NUMBER <= 225
#define setprintfunc( vm, pfn, efn ) sq_setprintfunc( vm, pfn )
#else
#define setprintfunc( vm, pfn, efn ) sq_setprintfunc( vm, pfn, efn )
#endif

#if SQUIRREL_VERSION_NUMBER <= 310
#define sq_getstringandsize( vm, idx, str, len ) \
	do { \
		sq_getstring( (vm), (idx), (str) ); \
		*(len) = scstrlen( *(str) ); \
	} while (0)
#endif

#undef ARRAYSIZE
#define ARRAYSIZE(p) (int)(sizeof(p)/sizeof(*(p)))
#define STRLEN(s) (ARRAYSIZE(s)-1)

#define _isdigit( c ) \
	IN_RANGE_CHAR( c, '0', '9' )

#define _isalpha( c ) \
	( IN_RANGE_CHAR( c, 'A', 'Z' ) || IN_RANGE_CHAR( c, 'a', 'z' ) )

#define _isalnum( c ) \
	( _isalpha(c) || _isdigit(c) )

#define _isprint( ch ) ( (ch) >= 0x20 && (ch) < 0x7F )
#define CTRLCH( ch ) ( ( (ch) & ~0x20 ) - '@' )
#define ASCIICH( ctrl ) ( (ctrl) + '@' )
#define CHARCMP( str, ch ) ( (str)[0] == (ch) && (str)[1] == 0 )

#define TERM_CLR_BRIGHT_FG "\033[1m"
#define TERM_CLR_BRIGHT_RESTORE "\033[22m"
#define TERM_CLR_UNDERLINE "\033[4m"
#define TERM_CLR_UNDERLINE_RESTORE "\033[24m"
#define TERM_CLR_FG_RED "\033[31m"
#define TERM_CLR_FG_CYAN "\033[36m"
#define TERM_CLR_FG_RESTORE "\033[39m"
#define TERM_CLR_NEGATIVE "\033[7m"
#define TERM_CLR_NEGATIVE_RESTORE "\033[27m"

#define TERM_INSERT_CHAR_N "\033[%d@"
#define TERM_INSERT_CHAR "\033[1@"

#define TERM_DELETE_CHAR_N "\033[%dP"
#define TERM_DELETE_CHAR "\033[1P"

#define TERM_ERASE_CHAR_N "\033[%dX"
#define TERM_ERASE_CHAR "\033[1X"

#define TERM_INSERT_LINES_N "\033[%dL"
#define TERM_INSERT_LINE "\033[1L"

#define TERM_DELETE_LINES_N "\033[%dM"
#define TERM_DELETE_LINE "\033[1M"

#define TERM_ERASE_LINE_TO_END "\033[0K"
#define TERM_ERASE_LINE_FROM_START "\033[1K"
#define TERM_ERASE_LINE "\033[2K"
#define TERM_ERASE_DISPLAY "\033[2J"

#define TERM_CURSOR_SAVE "\0337"
#define TERM_CURSOR_RESTORE "\0338"

#define TERM_CURSOR_UP_N "\033[%dA"
#define TERM_CURSOR_UP "\033M"

#define TERM_CURSOR_DN_N "\033[%dB"
// Also may do CR
#define TERM_CURSOR_DN "\n"

#define TERM_CURSOR_RT_N "\033[%dC"
#define TERM_CURSOR_RT "\033[1C"

#define TERM_CURSOR_LF_N "\033[%dD"
#define TERM_CURSOR_LF "\033[1D"

#define TERM_CURSOR_LINE_UP_N "\033[%dF"
#define TERM_CURSOR_LINE_DN_N "\033[%dE"

#define TERM_CURSOR_POS_11 "\033[1;1H"

#define TERM_CURSOR_HORZ_N "\033[%dG"
//#define TERM_CURSOR_HORZ_1 "\r"

#define TERM_CURSOR_VERT_N "\033[%dd"
#define TERM_CURSOR_VERT_1 "\033[1d"

#define TERM_SEQ_MAX_LEN 6

#define SEQ_UPARROW "[A"
#define SEQ_DOWNARROW "[B"
#define SEQ_RIGHTARROW "[C"
#define SEQ_LEFTARROW "[D"

#define SEQ_CRIGHT "[1;5C"
#define SEQ_CLEFT "[1;5D"

#define SEQ_S_INS "[2;2~"
#define SEQ_C_INS "[2;5~"

#define SEQ_DEL "[3~"
#define SEQ_C_DEL "[3;5~"
#define SEQ_PGUP "[5~"
#define SEQ_PGDN "[6~"
#define SEQ_HOME "[H"
#define SEQ_END "[F"
#define SEQ_F1 "OP"

template < unsigned int SEQSIZE >
static inline bool IsSeq( const char input[TERM_SEQ_MAX_LEN], const char (&cmp)[SEQSIZE] )
{
	for ( unsigned int i = 0; i < SEQSIZE-1; i++ )
	{
		if ( input[i] != cmp[i] )
			return false;
	}

	return true;
}

#ifdef _WIN32
	DWORD g_dwInMode;
	DWORD g_dwOutMode;
	#ifdef SQFFI
		const char *GetExceptionCodeStr( DWORD dwCode );
		LONG WINAPI ExceptionHandler( _EXCEPTION_POINTERS *ExceptionInfo );
	#endif
	void PrintWin32Error( const char *fmt, DWORD dwErr );
	BOOL WINAPI SigHandler( DWORD dwCtrlType );
#else
	#ifdef SQFFI
		jmp_buf g_jmp;
		int g_try;
		int g_sig;
		int GetExceptionCode();
		const char *GetExceptionCodeStr( int signum );
	#endif
	termios g_tios_def;
	void SigHandler( int signum );
#endif

bool InitTerm();
void RestoreTerm();
void ClipboardPaste();
void ClipboardCopy();

void PushHistory( char *str, int len );
void GetHistory( int amt );
void InvalidateHistoryIndex();

int CountTabsFront( const char *ptr );
bool IsTabFront( const char *ptr );
bool IsTabBack( const char *ptr );

int LineCountBetween( const char *a, const char *b );
int LineCountAhead( const char *ptr );
int LineCountUpTo( const char *ptr );

char *LineStart( char *ptr );
char *PrevLineStart( char *pStart );
char *LineEnd( char *ptr );
char *LineEnd( char *pStart, int *pnColumn );
int LineColumnEnd( char *pStart );
char *ByteAtColumn( char *pStart, int nColumn );
int ColumnAtByte( char *pStart, char *ptr );
void UpdateNavColumn();

int CharGroup( char c );
char *WordBoundNext( char *ptr );
char *WordBoundPrev( char *ptr, int end = 0 );

void PutPrompt();
void ClearPrompt();
void EnsureBufSpace( int len );
void Put( char ch );
void PutsNoPrint( const char *str, int len );
void Puts( const wchar_t *str, int wlen );
void Puts( const char *str, int len );
template < int SIZE > inline void Puts( const char (&str)[SIZE] ) { Puts( str, SIZE - 1 ); }
template < int SIZE > inline void PutsNoPrint( const char (&str)[SIZE] ) { PutsNoPrint( str, SIZE - 1 ); }
void Erase( int count, bool isByte = false );
void EraseFront( int count, bool isByte = false );

const char *GetType( HSQOBJECT obj );
void PrintArray( HSQUIRRELVM vm, HSQOBJECT array );
void PrintTable( HSQUIRRELVM vm, HSQOBJECT obj, bool isClass = false );
void PrintObj( HSQUIRRELVM vm, HSQOBJECT obj, bool escape = false );
void DoExecute( HSQUIRRELVM vm, const SQChar *str, int len, char **argv );
void Execute( HSQUIRRELVM vm, const char *str, int len, char **argv = NULL );
void ReadStdin( HSQUIRRELVM vm, char **argv );

void ExecutePrompt( HSQUIRRELVM vm );
void Input( HSQUIRRELVM vm, HSQDEBUGSERVER dbg );
#ifdef _WIN32
int DoInput( HSQUIRRELVM vm, wchar_t ch );
#else
int DoInput( HSQUIRRELVM vm, char ch );
#endif

struct vm_init_options;
void InitVM( HSQUIRRELVM *vm, vm_init_options opt );
void PrintVersion();
void PrintHelp( const char *cmd, bool ex );

struct vm_init_options
{
	int stacksize;
	bool nostdlib;
#if defined(SQFFI) && defined(_WIN32)
	bool windef;
#endif
};

#define push_func( vm, name, func, paramcount, paramcheck ) \
	sq_pushstring( vm, _SC(name), STRLEN(name) ); \
	sq_newclosure( vm, &func, 0 ); \
	sq_setparamscheck( vm, paramcount, _SC(paramcheck) ); \
	sq_newslot( vm, -3, SQFalse ); \
	(void)0

#define push_int( vm, name, val ) \
	sq_pushstring( vm, _SC(name), STRLEN(name) ); \
	sq_pushinteger( vm, val ); \
	sq_newslot( vm, -3, SQFalse ); \
	(void)0

#ifdef SQUNICODE
	#define scvprintf vfwprintf
#else
	#define scvprintf vfprintf
#endif

#undef scvsprintf
#ifdef SQUNICODE
	#define scvsprintf vswprintf
#else
	#define scvsprintf vsnprintf
#endif

void dummyprintfunc( HSQUIRRELVM, const SQChar *, ... );
SQRESULT throwerrorf( HSQUIRRELVM vm, const SQChar *fmt, ... );
void printfunc( HSQUIRRELVM, const SQChar *fmt, ... );
void errorfunc( HSQUIRRELVM, const SQChar *fmt, ... );
#ifdef _DEBUG
SQInteger sq_getregistrytable( HSQUIRRELVM vm );
#endif
SQInteger sq_printl( HSQUIRRELVM vm );
SQInteger sq_sleep( HSQUIRRELVM vm );
#ifdef NOSQSTDLIB
void compilererrorfunc( HSQUIRRELVM vm,
		const SQChar *err,
		const SQChar *source,
		SQInteger line,
		SQInteger column );
#endif
int PushArgv( HSQUIRRELVM vm, char **argv );
void DoExecuteFile( HSQUIRRELVM vm, const char *filename, const SQChar *sourcename, char **argv );

#define TAB_LEN 4
#define TAB_STR "    "
#define STR_PROMPT TERM_CLR_FG_CYAN "->" TERM_CLR_FG_RESTORE " "
#define STR_PROMPT_PLAIN "-> "
#define STR_PROMPT_LEN 3

#define STR_EXEC_BUF_SIZE 8192
#define STDIN_READ_BUF_SIZE 8192
#define CMD_BUF_INIT_SIZE 512
#define HISTORY_COUNT 256

char *g_pCmdBuf = NULL;
char *g_pCmdPtr = NULL;
char *g_pCmdEnd = NULL;
int g_nCmdCap = 0;

char *g_pCmdCopy = NULL;
char *g_pHistory = NULL;
int g_nHistoryLength = 0;
int g_nHistoryCapacity = 0;

int *g_pHistoryMap = NULL;
int g_nHistoryMapIndex = 0;
int g_nHistoryMapLength = 0;

int g_nNavColumn = 0;

bool g_bVT = false;
bool g_bErrorHighlight = false;
bool g_bTableIndent = true;
bool g_bTableJSONOutput = false;
int g_TablePrintIndent = 0;
void *g_TablePrintStack[32];

FILE *g_StreamOut = NULL;
FILE *g_StreamIn = NULL;

HSQUIRRELVM g_vm = NULL;

#define Print( psz ) fputs( (psz), g_StreamOut )
#define Printc( ch ) putc( (ch), g_StreamOut )
#define Printf( ... ) fprintf( g_StreamOut, __VA_ARGS__ )
#define Printw( _ptr, _len ) fwrite( (_ptr), 1, (_len), g_StreamOut )

#if 0
#define Error( psz ) fputs( (psz), stderr )
#define Errorc( ch ) putc( (ch), stderr )
#define Errorf( ... ) fprintf( stderr, __VA_ARGS__ )
#else
void Error( const char *psz )
{
	if ( g_bVT )
		fputs( TERM_CLR_FG_RED, stderr );

	fputs( psz, stderr );

	if ( g_bVT )
		fputs( TERM_CLR_FG_RESTORE, stderr );
}

void Errorc( char ch )
{
	char psz[2] = { ch, 0 };
	Error( psz );
}

void Errorf( const char *fmt, const char *p1 )
{
	if ( g_bVT )
		fputs( TERM_CLR_FG_RED, stderr );

	fprintf( stderr, fmt, p1 );

	if ( g_bVT )
		fputs( TERM_CLR_FG_RESTORE, stderr );
}

#ifdef SQUNICODE
void Errorf( const char *fmt, const SQChar *p1 )
{
	if ( g_bVT )
		fputs( TERM_CLR_FG_RED, stderr );

	fprintf( stderr, fmt, p1 );

	if ( g_bVT )
		fputs( TERM_CLR_FG_RESTORE, stderr );
}
#endif
#endif

#ifdef SQDBG_NATIVE_STACKTRACE
extern "C" {
SQDBG_API void sqdbg_dump_stack( HSQUIRRELVM vm,
		void (*callback)(
			void *ud,
			const char *nameptr, int namelen,
			const char *typeptr, int typelen,
			const char *valueptr, int valuelen ),
		void *ud );

SQDBG_API void sqdbg_stacktrace( HSQUIRRELVM vm, int startFrame, int levels,
		bool parameters, bool parameterTypes, bool parameterNames, bool parameterValues,
		void (*callback)(
			void *ud,
			const char *nameptr, int namelen,
			const char *srcnameptr, int srcnamelen,
			const char *srcpathptr, int srcpathlen,
			int line, void *ip, int offset ),
		void *ud );

SQDBG_API void sqdbg_set_process( HSQUIRRELVM vm, HANDLE hProcess );
} // extern "C"

struct printdata_t
{
	HSQUIRRELVM vm;
	SQPRINTFUNCTION fn;
};

HANDLE g_hProcess = 0;

bool LoadModuleSymbols( HANDLE hProcess, const char *pModuleName )
{
	HMODULE hModule = 0;

	// if pModuleName is 1, get the module where this function resides in
	if ( !( pModuleName == (char*)1 &&
				GetModuleHandleExA(
					GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
					(LPCSTR)&LoadModuleSymbols,
					&hModule ) ) &&
			!GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, pModuleName, &hModule ) )
	{
		PrintWin32Error( "GetModuleHandleExA failed: %s", GetLastError() );
		return false;
	}

	MODULEINFO info;
	if ( !GetModuleInformation( hProcess, hModule, &info, sizeof(MODULEINFO) ) )
	{
		PrintWin32Error( "GetModuleInformation failed: %s", GetLastError() );
		return false;
	}

	char name[256];
	if ( !GetModuleFileNameExA( hProcess, hModule, name, sizeof(name) ) )
	{
		PrintWin32Error( "GetModuleFileNameA failed: %s", GetLastError() );
		return false;
	}

	if ( !SymLoadModule( hProcess, NULL, name, NULL, (uintptr_t)info.lpBaseOfDll, info.SizeOfImage ) &&
			GetLastError() != ERROR_SUCCESS )
	{
		PrintWin32Error( "SymLoadModule failed: %s", GetLastError() );
		return false;
	}

	return true;
}

bool InitSymbolHandler( HANDLE *pProcess, HSQUIRRELVM vm )
{
	if ( !*pProcess )
	{
		HANDLE hCurrentProcess = GetCurrentProcess();

		if ( !DuplicateHandle(
					hCurrentProcess,
					hCurrentProcess,
					hCurrentProcess,
					pProcess,
					0,
					TRUE,
					DUPLICATE_SAME_ACCESS ) )
		{
			PrintWin32Error( "DuplicateHandle failed: %s", GetLastError() );
			return false;
		}

		SymSetOptions(
				SYMOPT_DEFERRED_LOADS |
				SYMOPT_LOAD_LINES |
				SYMOPT_NO_UNQUALIFIED_LOADS |
				SYMOPT_AUTO_PUBLICS |
				SYMOPT_DISABLE_SYMSRV_AUTODETECT );

		if ( !SymInitialize( *pProcess, NULL, FALSE ) )
		{
			PrintWin32Error( "SymInitialize failed: %s", GetLastError() );
			goto fail;
		}

#ifdef SYM_LOAD_ALL
		if ( !SymRefreshModuleList( *pProcess ) )
		{
			PrintWin32Error( "SymRefreshModuleList failed: %s", GetLastError() );
			goto fail;
		}
#else
		LoadModuleSymbols( *pProcess, (char*)1 );
#ifdef SQDBG_DLL
		LoadModuleSymbols( *pProcess, "sqdbg.dll" );
		LoadModuleSymbols( *pProcess, "squirrel.dll" );
#ifndef NOSQSTDLIB
		LoadModuleSymbols( *pProcess, "sqstdlib.dll" );
#endif
#endif
#ifdef SQFFI
		LoadModuleSymbols( *pProcess, "libffi-8.dll" );
#endif
#endif

		sqdbg_set_process( vm, *pProcess );
	}

	return true;

fail:
	CloseHandle( *pProcess );
	*pProcess = 0;
	return false;
}

void TerminateSymbolHandler( HANDLE *pProcess )
{
	if ( !*pProcess )
		return;

	if ( !SymCleanup( *pProcess ) )
		PrintWin32Error( "SymCleanup failed: %s", GetLastError() );

	CloseHandle( *pProcess );
	*pProcess = 0;
}

static void stackdumpcallback(
		void *ud,
		const char *nameptr, int,
		const char *typeptr, int typelen,
		const char *valueptr, int )
{
	printdata_t *data = (printdata_t*)ud;
	int pad = 16 - typelen;

	data->fn( data->vm,
			_SC("%-9" FMT_CSTR_S " : <%" FMT_CSTR_S ">%*" FMT_CSTR_S ": %.100" FMT_CSTR_S "\n"),
			nameptr, typeptr, max( pad, 0 ), "", valueptr );
}

static void stacktracecallback(
		void *ud,
		const char *nameptr, int,
		const char *srcnameptr, int srcnamelen,
		const char *, int,
		int line, void *, int offset )
{
	// ignore label
	if ( *nameptr == '-' )
		return;

	printdata_t *data = (printdata_t*)ud;
	const int kSourcePadLen = 30;

	int digits = 0, line2 = line;
	do { line2 /= 10; digits++; } while ( line2 );
	int pad = kSourcePadLen - ( ( srcnamelen ? srcnamelen : STRLEN("??") ) + digits + 1 );

	data->fn( data->vm,
			_SC("[%04X] %" FMT_CSTR_S ":%d %*" FMT_CSTR_S " %" FMT_CSTR_S "\n"),
			(unsigned int)offset, srcnamelen ? srcnameptr : "??", line, max( pad, 0 ), "", nameptr );
}

// stacktrace( error, flags )
SQInteger StackTrace( HSQUIRRELVM vm )
{
	if ( !g_hProcess )
		return 0;

	int top = sq_gettop( vm );
	int start = 0;
	bool bNoDump = 0;
	bool bNoTrace = 0;
	printdata_t data;
	data.vm = vm;
	data.fn = sq_getprintfunc( vm );

	if ( top > 1 )
	{
		HSQOBJECT o;

		if ( top > 2 )
		{
			if ( top > 3 )
				return sq_throwerror( vm, _SC("wrong number of parameters") );

			sq_getstackobj( vm, 3, &o );
			Assert( sq_type(o) == OT_INTEGER );

			bNoDump = ( _integer(o) & 0x1 ) != 0;
			bNoTrace = ( _integer(o) & 0x2 ) != 0;
		}

		sq_getstackobj( vm, 2, &o );

		if ( sq_type(o) != OT_NULL )
		{
			start = 1;
#if SQUIRREL_VERSION_NUMBER > 225
			data.fn = sq_geterrorfunc( vm );
#endif
#if SQUIRREL_VERSION_NUMBER >= 300
			if ( SQ_SUCCEEDED( sq_tostring( vm, 2 ) ) )
#else
			sq_tostring( vm, 2 );
#endif
			{
				sq_getstackobj( vm, -1, &o );
				Assert( sq_type(o) == OT_STRING );
				data.fn( vm, _SC("ERROR: " FMT_STR "\n"), _string(o)->_val );
				data.fn( vm, _SC("-------------------------------\n") );
				sq_pop( vm, 1 );
			}
		}
	}

	if ( !bNoTrace )
	{
		data.fn( vm, _SC("STACKTRACE:\n") );
		sqdbg_stacktrace( vm, start, 0, true, false, false, true, stacktracecallback, &data );
		data.fn( vm, _SC("-------------------------------\n") );
	}

	if ( !bNoDump )
	{
		data.fn( vm, _SC("STACKDUMP:\n") );
		sqdbg_dump_stack( vm, stackdumpcallback, &data );
		data.fn( vm, _SC("-------------------------------\n") );
	}

	return 0;
}
#endif


////////////////////////////////////////////////

#ifdef _WIN32
#ifdef SQFFI
const char *GetExceptionCodeStr( DWORD dwCode )
{
	switch ( dwCode )
	{
		case STATUS_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
		case STATUS_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
		case STATUS_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
		case STATUS_FLOAT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND";
		case STATUS_FLOAT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
		case STATUS_FLOAT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT";
		case STATUS_FLOAT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
		case STATUS_FLOAT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
		case STATUS_FLOAT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK";
		case STATUS_FLOAT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW";
		case STATUS_GUARD_PAGE_VIOLATION: return "EXCEPTION_GUARD_PAGE";
		case STATUS_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
		case STATUS_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
		case STATUS_INTEGER_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
		case STATUS_INTEGER_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
		case STATUS_INVALID_HANDLE: return "EXCEPTION_INVALID_HANDLE";
		case STATUS_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
		case STATUS_PRIVILEGED_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
		case STATUS_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
		default:
		{
			static char buf[9];
			int i = 7;
			do
			{
				buf[i--] = ( dwCode & 0xF ) + ( ( ( dwCode & 0xF ) < 0xA ) ? '0' : ( 'A' - 0xA ) );
				dwCode >>= 4;
			}
			while ( dwCode );
			while ( i >= 0 )
				buf[i--] = '0';
			return buf;
		}
	}
}

EXCEPTION_RECORD g_ExceptionRecord;

LONG WINAPI ExceptionHandler( _EXCEPTION_POINTERS *ExceptionInfo )
{
	g_ExceptionRecord = *ExceptionInfo->ExceptionRecord;
	return EXCEPTION_EXECUTE_HANDLER;
}

#define SQ_EXCEPTION_HANDLER ExceptionHandler( GetExceptionInformation() )

int ThrowSQException( HSQUIRRELVM vm )
{
	if ( g_ExceptionRecord.ExceptionCode == STATUS_ACCESS_VIOLATION )
	{
		throwerrorf( vm,
				_SC("Access violation " FMT_CSTR " location 0x" FMT_PTR ""),
				( g_ExceptionRecord.ExceptionInformation[0] == 0 ) ? "reading" : "writing",
				g_ExceptionRecord.ExceptionInformation[1] );
	}
	else
	{
		throwerrorf( vm, _SC(FMT_CSTR), GetExceptionCodeStr( g_ExceptionRecord.ExceptionCode ) );
	}

	return SQ_ERROR;
}
#endif

void PrintWin32Error( const char *fmt, DWORD dwErr )
{
	LPVOID lpErr = 0;
	FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dwErr,
			0,
			(LPSTR)&lpErr,
			0, NULL);
	Errorf( fmt, (char*)lpErr );
	LocalFree( lpErr );
}

BOOL WINAPI SigHandler( DWORD dwCtrlType )
{
	if ( dwCtrlType == CTRL_C_EVENT )
		RestoreTerm();

	return FALSE;
}
#else
#ifdef SQFFI
int GetExceptionCode()
{
	return g_sig;
}

const char *GetExceptionCodeStr( int signum )
{
	switch ( signum )
	{
		case SIGILL: return "SIGILL";
		case SIGFPE: return "SIGFPE";
		case SIGBUS: return "SIGBUS";
		case SIGSEGV: return "SIGSEGV";
		default:
		{
			static char buf[10];
			int c = signum;
			int i = 0;
			do
			{
				c /= 10;
				i++;
			}
			while ( c );
			i--;
			do
			{
				buf[i--] = ( signum % 10 ) + '0';
				signum /= 10;
			}
			while ( signum );
			return buf;
		}
	}
}

#define ThrowSQException( vm ) throwerrorf( vm, _SC(FMT_CSTR), GetExceptionCodeStr( GetExceptionCode() ) )
#endif

void SigHandler( int signum )
{
	if ( signum == SIGINT )
	{
		RestoreTerm();
		Print("^C");
		exit(1);
	}

#ifdef SQFFI
	if ( g_try )
	{
		g_try = 0;
		siglongjmp( g_jmp, signum );
	}
#endif
}
#endif

#ifdef SQFFI
#ifdef _WIN32
	#ifndef _MSC_VER
		#undef __try
		#undef __except
		#define __try
		#define __except(a) if (0)
	#endif
	#define __try_end() (void)0
#else
	#undef __try
	#undef __except
	#define __try if ( ( g_try = 1, ( g_sig = sigsetjmp( g_jmp, 1 ) ) == 0 ) )
	#define __except(a) else
	#define __try_end() g_try = 0
#endif
#endif

bool InitTerm()
{
#ifdef _WIN32
	HANDLE hStdin = GetStdHandle( STD_INPUT_HANDLE );
	HANDLE hStdOut = GetStdHandle( STD_OUTPUT_HANDLE );

	if ( hStdin != INVALID_HANDLE_VALUE && GetConsoleMode( hStdin, &g_dwInMode ) )
	{
		if ( !SetConsoleMode( hStdin,
					( g_dwInMode & ~ENABLE_ECHO_INPUT ) |
					ENABLE_VIRTUAL_TERMINAL_INPUT |
					DISABLE_NEWLINE_AUTO_RETURN ) )
		{
			PrintWin32Error( "could not set stdin console mode: %s\n", GetLastError() );
		}
		else
		{
			SetConsoleCtrlHandler( SigHandler, TRUE );
		}
	}
	else
	{
		return false;
	}

	if ( hStdOut != INVALID_HANDLE_VALUE && GetConsoleMode( hStdOut, &g_dwOutMode ) )
	{
		if ( !SetConsoleMode( hStdOut,
					g_dwOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING ) )
			PrintWin32Error( "could not set stdout console mode: %s\n", GetLastError() );
	}

	SetConsoleCP( CP_UTF8 );
	SetConsoleOutputCP( CP_UTF8 );
#else
	int f = fcntl( _fileno( g_StreamIn ), F_GETFL );

	if ( f == -1 || fcntl( _fileno( g_StreamIn ), F_SETFL, f | O_NONBLOCK ) == -1 )
	{
		Error( "failed to set nonblock\n" );
		return false;
	}

	if ( !isatty( _fileno( g_StreamIn ) ) )
	{
		Error( "not tty\n" );
		return false;
	}

	termios t;

	if ( tcgetattr( _fileno( g_StreamIn ), &t ) != 0 )
	{
		Errorf( "tcgetattr error: %s\n", strerror( errno ) );
		return false;
	}

	g_tios_def = t;
	t.c_iflag = ( t.c_iflag & ~( INLCR | ICRNL | IGNCR ) ) | IUTF8;
	t.c_lflag = ( t.c_lflag & ~( ECHO | ECHONL | ICANON | IEXTEN ) ) | VEOF;

	if ( tcsetattr( _fileno( g_StreamIn ), TCSANOW, &t ) != 0 )
	{
		Errorf( "tcsetattr error: %s\n", strerror( errno ) );
		return false;
	}

	struct sigaction sa;
	sigemptyset( &sa.sa_mask );
	sa.sa_flags = SA_NODEFER;
	sa.sa_handler = SigHandler;

	if ( sigaction( SIGINT, &sa, NULL ) != 0 ||
			sigaction( SIGSEGV, &sa, NULL ) != 0 ||
			sigaction( SIGBUS, &sa, NULL ) != 0 ||
			sigaction( SIGILL, &sa, NULL ) != 0 ||
			sigaction( SIGFPE, &sa, NULL ) != 0 )
	{
		Errorf( "sigaction error: %s\n", strerror( errno ) );
		return false;
	}
#endif

	g_bVT = true;
	return true;
}

void RestoreTerm()
{
#ifdef _WIN32
	HANDLE hStdin = GetStdHandle( STD_INPUT_HANDLE );
	HANDLE hStdOut = GetStdHandle( STD_OUTPUT_HANDLE );

	if ( hStdin != INVALID_HANDLE_VALUE )
		SetConsoleMode( hStdin, g_dwInMode );

	if ( hStdOut != INVALID_HANDLE_VALUE )
		SetConsoleMode( hStdOut, g_dwOutMode );
#else
	tcsetattr( _fileno( g_StreamIn ), TCSANOW, &g_tios_def );
#endif

	g_bVT = false;
}

void ClipboardPaste()
{
#ifdef _WIN32
	HANDLE hData;
	wchar_t *pszText;

	if ( OpenClipboard( NULL ) )
	{
		if ( ( hData = GetClipboardData( CF_UNICODETEXT ) ) != NULL &&
				( pszText = (wchar_t*)GlobalLock( hData ) ) != NULL )
		{
			int nTextLen = wcslen( pszText );
			if ( nTextLen > 16 * 1024 )
				nTextLen = 16 * 1024;

			Puts( pszText, nTextLen );
			GlobalUnlock( hData );
		}

		CloseClipboard();
	}
#endif
}

void ClipboardCopy()
{
#ifdef _WIN32
	HANDLE hData;
	wchar_t *pszText;

	if ( g_pCmdEnd == g_pCmdBuf && g_nHistoryMapIndex == -1 )
		return;

	*g_pCmdEnd = 0;

	if ( !OpenClipboard( NULL ) )
		return;

	int wlen = UTF8ToWChar( NULL, 0, g_pCmdBuf, g_pCmdEnd - g_pCmdBuf + 1 );

	if ( ( hData = GlobalAlloc( GMEM_MOVEABLE, wlen * sizeof(wchar_t) ) ) != NULL &&
			( pszText = (wchar_t*)GlobalLock( hData ) ) != NULL )
	{
		// Copy history if browsing
		if ( g_nHistoryMapIndex == -1 )
		{
			UTF8ToWChar( pszText, wlen * sizeof(wchar_t), g_pCmdBuf, g_pCmdEnd - g_pCmdBuf + 1 );
		}
		else
		{
			char *pPrev = g_pHistory + g_pHistoryMap[ g_nHistoryMapIndex ];
			UTF8ToWChar( pszText, wlen * sizeof(wchar_t), pPrev, strlen( pPrev ) );
		}

		GlobalUnlock( hData );

		if ( SetClipboardData( CF_UNICODETEXT, hData ) )
		{
			// Visual confirmation, highlight line
			const char alert[] = "  *** Copied to clipboard ***  ";
			const char alert_blank[] = "                               ";
			Printf( TERM_CURSOR_SAVE TERM_CLR_NEGATIVE "\r%s", alert );
			_sleep( 150 );
			const char *pStart = LineStart( g_pCmdPtr );
			const char *prompt = ( pStart == g_pCmdBuf ) ? STR_PROMPT : "";
			int nPrintLen = (int)( g_pCmdPtr - pStart );

			if ( nPrintLen < STRLEN(alert) )
			{
				nPrintLen = STRLEN(alert);

				if ( nPrintLen > (int)( g_pCmdEnd - pStart ) )
					nPrintLen = (int)( g_pCmdEnd - pStart );
			}

			Printf( TERM_CLR_NEGATIVE_RESTORE "\r%s\r%s%.*s" TERM_CURSOR_RESTORE,
					alert_blank, prompt, nPrintLen, pStart );
		}
		else
		{
			PrintWin32Error( "clipboard failure: %s\n", GetLastError() );
		}
	}

	CloseClipboard();
#endif
}

void PushHistory( char *str, int len )
{
	Assert( len > 0 );

	int size = g_nHistoryCapacity;

	if ( len + 1 > size - g_nHistoryLength )
	{
		do
		{
			size += size / 2;
		}
		while ( len + 1 > size - g_nHistoryLength );

		g_pHistory = (char*)realloc( g_pHistory, size );
		g_nHistoryCapacity = size;
	}

	g_nHistoryMapIndex = -1;

	// Is it identical to previous one?
	if ( g_nHistoryMapLength )
	{
		char *pPrev = g_pHistory + g_pHistoryMap[ g_nHistoryMapLength - 1 ];

		if ( strcmp( pPrev, str ) == 0 )
			return;
	}

	// History limit, shift all down by 1
	if ( g_nHistoryMapLength >= HISTORY_COUNT )
	{
		memmove( g_pHistoryMap, g_pHistoryMap + 1, ( HISTORY_COUNT - 1 ) * sizeof(*g_pHistoryMap) );
		g_nHistoryMapLength--;
	}

	g_pHistoryMap[ g_nHistoryMapLength ] = g_nHistoryLength;
	g_nHistoryMapLength++;

	char *pTarget = g_pHistory + g_nHistoryLength;
	memcpy( pTarget, str, len );
	pTarget[len] = 0;

	g_nHistoryLength += len + 1;
}

void GetHistory( int amt )
{
	Assert( amt == 1 || amt == -1 );

	// No history
	if ( !g_nHistoryMapLength )
		return;

	if ( g_nHistoryMapIndex == -1 )
	{
		// Next at end, do nothing
		if ( amt == 1 )
			return;

		// Cache current buf
		if ( g_pCmdCopy )
		{
			free( g_pCmdCopy );
			g_pCmdCopy = NULL;
		}

		int len = g_pCmdEnd - g_pCmdBuf;
		if ( len )
		{
			g_pCmdCopy = (char*)malloc( len + 1 );
			memcpy( g_pCmdCopy, g_pCmdBuf, len );
			g_pCmdCopy[len] = 0;
		}

		g_nHistoryMapIndex = g_nHistoryMapLength;
	}
	else if ( g_nHistoryMapIndex == 0 )
	{
		// Next at end, do nothing
		if ( amt == -1 )
			return;
	}

	ClearPrompt();

	g_nHistoryMapIndex += amt;
	Assert( g_nHistoryMapIndex >= 0 && g_nHistoryMapIndex <= g_nHistoryMapLength );

	if ( g_nHistoryMapIndex == g_nHistoryMapLength )
	{
		Assert( amt == 1 );

		// Next at end, restore cached prompt
		g_nHistoryMapIndex = -1;

		if ( g_pCmdCopy )
			Puts( g_pCmdCopy, strlen( g_pCmdCopy ) );

		return;
	}

	// Load up
	char *pPrev = g_pHistory + g_pHistoryMap[ g_nHistoryMapIndex ];
	Puts( pPrev, strlen( pPrev ) );

	// Moving down, put cursor at the very start
	if ( amt == 1 )
	{
		DoInput( NULL, CTRLCH('A') );
		DoInput( NULL, CTRLCH('A') );
	}
	else
	{
		UpdateNavColumn();
	}
}

void InvalidateHistoryIndex()
{
	if ( g_pCmdCopy )
	{
		free( g_pCmdCopy );
		g_pCmdCopy = NULL;
	}

	g_nHistoryMapIndex = -1;
}

int CountTabsFront( const char *ptr )
{
	int count = 0;

	for ( ; ptr + TAB_LEN <= g_pCmdEnd; ptr += TAB_LEN )
	{
		for ( int i = 0; i < TAB_LEN; i++ )
			if ( ptr[i] != ' ' )
				goto exit;

		count++;
	}

exit:
	return count;
}

bool IsTabFront( const char *ptr )
{
	if ( ptr + TAB_LEN > g_pCmdEnd )
		return false;

	for ( int i = 0; i < TAB_LEN; i++ )
		if ( ptr[i] != ' ' )
			return false;

	return true;
}

bool IsTabBack( const char *ptr )
{
	if ( ptr - TAB_LEN < g_pCmdBuf )
		return false;

	for ( int i = -1; i > -TAB_LEN - 1; i-- )
		if ( ptr[i] != ' ' )
			return false;

	return true;
}

int LineCountBetween( const char *a, const char *b )
{
	Assert( a <= b );

	int count = 1;

	for ( ; a < b; a++ )
	{
		if ( *a == '\n' )
			count++;
	}

	return count;
}

int LineCountAhead( const char *ptr )
{
	return LineCountBetween( ptr, g_pCmdEnd );
}

int LineCountUpTo( const char *ptr )
{
	return LineCountBetween( g_pCmdBuf, ptr );
}

char *LineStart( char *ptr )
{
	Assert( ptr >= g_pCmdBuf && ptr <= g_pCmdEnd );

	// if not at the end, start from next char
	if ( !( *ptr == '\n' || *ptr == 0 ) )
		ptr++;

	for (;;)
	{
		ptr--;

		if ( ptr < g_pCmdBuf )
			return g_pCmdBuf;

		if ( *ptr == '\n' )
			return ptr + 1;
	}
}

char *PrevLineStart( char *pStart )
{
	return LineStart( pStart - (int)( pStart != g_pCmdBuf ) );
}

char *LineEnd( char *ptr )
{
	Assert( ptr >= g_pCmdBuf && ptr <= g_pCmdEnd );

	// already at the end
	if ( *ptr == '\n' || *ptr == 0 )
		return ptr;

	for (;;)
	{
		ptr++;

		if ( ptr >= g_pCmdEnd )
			return g_pCmdEnd;

		if ( *ptr == '\n' )
			return ptr;
	}
}

char *LineEnd( char *pStart, int *pnColumn )
{
	Assert( pStart >= g_pCmdBuf && pStart <= g_pCmdEnd );

	// Columns are 1 based
	int nColumn = 1;

	while ( pStart < g_pCmdEnd && *pStart != '\n' )
	{
		int bytes = IsValidUTF8( pStart, g_pCmdEnd - pStart );

		if ( bytes == 0 )
		{
			bytes = 1;
		}
		else if ( bytes > 2 && IsDoubleWidth( pStart ) )
		{
			nColumn++;
		}

		pStart += bytes;
		nColumn++;
	}

	*pnColumn = nColumn;
	return pStart;
}

int LineColumnEnd( char *pStart )
{
	int col;
	LineEnd( pStart, &col );
	return col;
}

char *ByteAtColumn( char *pStart, int nColumn )
{
	Assert( pStart >= g_pCmdBuf && pStart <= g_pCmdEnd );

	nColumn--;

	while ( nColumn > 0 )
	{
		int bytes = IsValidUTF8( pStart, g_pCmdEnd - pStart );

		if ( bytes == 0 )
		{
			bytes = 1;
		}
		else if ( bytes > 2 && IsDoubleWidth( pStart ) )
		{
			nColumn--;
		}

		pStart += bytes;
		nColumn--;
	}

	return pStart;
}

int ColumnAtByte( char *pStart, char *ptr )
{
	Assert( pStart >= g_pCmdBuf && pStart <= g_pCmdEnd );
	Assert( pStart <= ptr );

	int nColumn = 1;

	while ( pStart < ptr && *pStart != '\n' )
	{
		int bytes = IsValidUTF8( pStart, g_pCmdEnd - pStart );

		if ( bytes == 0 )
		{
			bytes = 1;
		}
		else if ( bytes > 2 && IsDoubleWidth( pStart ) )
		{
			nColumn++;
		}

		pStart += bytes;
		nColumn++;
	}

	return nColumn;
}

void UpdateNavColumn()
{
	char *c = LineStart( g_pCmdPtr );
	int nColumn = 1;

	while ( c < g_pCmdPtr )
	{
		int bytes = IsValidUTF8( c, g_pCmdEnd - c );

		if ( bytes == 0 )
		{
			bytes = 1;
		}
		else if ( bytes > 2 && IsDoubleWidth( c ) )
		{
			nColumn++;
		}

		c += bytes;
		nColumn++;
	}

	g_nNavColumn = nColumn;
}

int CharGroup( char c )
{
	if ( c == ' ' || c == '\n' || c == '\r' )
		return 0;

	// Lazily put all unicode bytes in the same group
	if ( _isalnum(c) || IN_RANGE_CHAR( c, 0x80, 0xF3 ) )
		return 1;

	if ( c == 0 )
		return -1;

	return 2;
}

char *WordBoundNext( char *ptr )
{
	Assert( ptr >= g_pCmdBuf && ptr <= g_pCmdEnd );

	int type = CharGroup( *ptr );

	for ( char *c = ptr; c < g_pCmdEnd; c++ )
	{
		if ( type != CharGroup( *c ) )
		{
			while ( c < g_pCmdEnd && ( *c == ' ' || *c == '\n' ) )
				c++;

			return c;
		}
	}

	return g_pCmdEnd;
}

char *WordBoundPrev( char *ptr, int end )
{
	Assert( ptr >= g_pCmdBuf && ptr <= g_pCmdEnd );

	int type = CharGroup( *ptr );

	for ( char *c = ptr - 1; c >= g_pCmdBuf; c-- )
	{
		if ( type != CharGroup( *c ) )
		{
			while ( c >= g_pCmdBuf && ( *c == ' ' || *c == '\n' ) )
				c--;

			if ( !end )
			{
				type = CharGroup( *c );

				// move to start
				while ( c >= g_pCmdBuf && type == CharGroup( *c ) )
					c--;
			}

			return c + 1;
		}
	}

	return g_pCmdBuf;
}

void PutPrompt()
{
	Print( "\r\n" STR_PROMPT );
}

void ClearPrompt()
{
	int lines = LineCountAhead( g_pCmdBuf );
	int curline = LineCountUpTo( g_pCmdPtr );

	if ( curline > 1 )
		Printf( TERM_CURSOR_LINE_UP_N, curline - 1 );

	if ( lines > 1 )
	{
		Printf( "\r" TERM_DELETE_LINES_N STR_PROMPT, lines );
	}
	else
	{
		Print( "\r" TERM_ERASE_LINE STR_PROMPT );
	}

	g_pCmdPtr = g_pCmdBuf;
	g_pCmdEnd = g_pCmdBuf;
}

void EnsureBufSpace( int len )
{
	int size = g_nCmdCap;
	int cmdlen = g_pCmdEnd - g_pCmdBuf;

	if ( cmdlen + len >= size )
	{
		int cmdidx = g_pCmdPtr - g_pCmdBuf;

		do
		{
			size += size / 2;
		}
		while ( cmdlen + len >= size );

		g_pCmdBuf = (char*)realloc( g_pCmdBuf, size );
		g_pCmdPtr = g_pCmdBuf + cmdidx;
		g_pCmdEnd = g_pCmdBuf + cmdlen;
		g_nCmdCap = size;
	}
}

void Put( char ch )
{
	Assert( _isprint( ch ) || ch == '\n' );

	EnsureBufSpace( 1 );

	if ( g_pCmdPtr != g_pCmdEnd )
	{
		memmove( g_pCmdPtr + 1, g_pCmdPtr, g_pCmdEnd - g_pCmdPtr );

		if ( ch != '\n' )
			Print( TERM_INSERT_CHAR );
	}

	if ( ch == '\n' )
		Printc( '\r' );

	Printc( ch );
	*g_pCmdPtr++ = ch;
	g_pCmdEnd++;
}

void PutsNoPrint( const char *str, int len )
{
	EnsureBufSpace( len );

	if ( g_pCmdPtr != g_pCmdEnd )
		memmove( g_pCmdPtr + len, g_pCmdPtr, g_pCmdEnd - g_pCmdPtr );

	memcpy( g_pCmdPtr, str, len );

	g_pCmdPtr += len;
	g_pCmdEnd += len;
}

void Puts( const wchar_t *str, int wlen )
{
	int len = WCharToUTF8( NULL, 0, str, wlen );
	EnsureBufSpace( len );

	if ( g_pCmdPtr != g_pCmdEnd )
		memmove( g_pCmdPtr + len, g_pCmdPtr, g_pCmdEnd - g_pCmdPtr );

	WCharToUTF8( g_pCmdPtr, len, str, wlen );

	g_pCmdPtr += len;
	g_pCmdEnd += len;

	if ( g_pCmdPtr != g_pCmdEnd )
	{
		int count = 0;

		for ( int i = 0; i < wlen; )
		{
			if ( UTF_SURROGATE( str[i] ) )
			{
				count++;
				i++;
			}

			count++;
			i++;
		}

		Printf( TERM_INSERT_CHAR_N, count );
	}

	Printw( g_pCmdPtr - len, len );
}

void Puts( const char *str, int len )
{
	PutsNoPrint( str, len );

	if ( g_pCmdPtr != g_pCmdEnd )
	{
		int count = 0;
		int bytes;

		for ( int i = 0; i < len; i += bytes )
		{
			bytes = IsValidUTF8( str + i, len - i );

			if ( bytes == 0 )
			{
				bytes = 1;
			}
			else if ( bytes > 2 && IsDoubleWidth( str + i ) )
			{
				count++;
			}

			count++;
		}

		Printf( TERM_INSERT_CHAR_N, count );
	}

	Printw( str, len );
}

void Erase( int count, bool isByte )
{
	Assert( count > 0 );

	int byteCount = 0;

	if ( count == 1 && IsTabBack( g_pCmdPtr ) )
	{
		count = TAB_LEN;
		byteCount = TAB_LEN;
	}
	else
	{
		int maxCount = g_pCmdPtr - g_pCmdBuf;
		int cpCount = min( count, maxCount );
		count = 0;

		for ( char *c = g_pCmdPtr - 1; c >= g_pCmdPtr - cpCount; c-- )
		{
			char *ptr = UTF8Start( c, g_pCmdBuf );

			if ( ptr )
			{
				int bytes = IsValidUTF8( ptr, c - ptr + 1 );

				if ( bytes == 0 )
				{
					bytes = 1;
				}
				else if ( bytes > 2 && IsDoubleWidth( ptr ) )
				{
					count++;
				}

				byteCount += bytes;
				count++;
				c = ptr;
			}
			else
			{
				byteCount++;
			}
		}

		if ( isByte )
			byteCount = cpCount;
	}

	Assert( g_pCmdPtr - byteCount >= g_pCmdBuf );

	int lines = LineCountBetween( g_pCmdPtr - byteCount, g_pCmdPtr ) - 1;

	if ( lines == 0 )
	{
		if ( count == 1 )
		{
			Print( "\b" TERM_DELETE_CHAR );
		}
		else
		{
			Printf( TERM_CURSOR_LF_N TERM_DELETE_CHAR_N, count, count );
		}
	}
	else
	{
		// Deleted lines, move lines up, put cursor up and at the end
		char *pNextStart = LineStart( g_pCmdPtr - byteCount );
		int col = LineColumnEnd( pNextStart );

		if ( pNextStart == g_pCmdBuf )
			col += STR_PROMPT_LEN;

		if ( lines != 1 )
			Printf( TERM_CURSOR_UP_N, lines - 1 );

		Printf( TERM_DELETE_LINES_N TERM_CURSOR_UP TERM_CURSOR_HORZ_N, lines, col );

		// Moved text from next line into current line, append
		if ( !( g_pCmdPtr[0] == '\n' || g_pCmdPtr[0] == 0 ) )
		{
			char *pStart = LineStart( g_pCmdPtr );
			int curcol = LineColumnEnd( pStart );
			Printw( pStart, curcol - 1 );
			Printf( TERM_CURSOR_HORZ_N, col );
		}
		// else Deleted empty line
	}

	if ( g_pCmdPtr != g_pCmdEnd )
		memmove( g_pCmdPtr - byteCount, g_pCmdPtr, g_pCmdEnd - g_pCmdPtr );

	g_pCmdPtr -= byteCount;
	g_pCmdEnd -= byteCount;

	if ( g_pCmdPtr == g_pCmdEnd )
		*g_pCmdEnd = 0;
}

void EraseFront( int count, bool isByte )
{
	Assert( count > 0 );
	Assert( g_pCmdPtr < g_pCmdEnd );

	int byteCount = 0;

	if ( count == 1 && IsTabFront( g_pCmdPtr ) )
	{
		count = TAB_LEN;
		byteCount = TAB_LEN;
	}
	else
	{
		int maxCount = g_pCmdEnd - g_pCmdPtr;
		int cpCount = min( count, maxCount );
		count = 0;

		for ( char *c = g_pCmdPtr; c < g_pCmdPtr + cpCount; )
		{
			int bytes = IsValidUTF8( c, g_pCmdEnd - c );

			if ( bytes )
			{
				count++;

				if ( bytes > 2 && IsDoubleWidth( c ) )
					count++;
			}
			else
			{
				bytes = 1;
			}

			byteCount += bytes;
			c += bytes;
		}

		if ( isByte )
			byteCount = cpCount;
	}

	Assert( g_pCmdPtr + byteCount <= g_pCmdEnd );

	int lines = LineCountBetween( g_pCmdPtr, g_pCmdPtr + byteCount ) - 1;

	if ( lines == 0 )
	{
		Printf( TERM_DELETE_CHAR_N, count );
	}
	else
	{
		char *pStart = LineStart( g_pCmdPtr );
		int col = ColumnAtByte( pStart, g_pCmdPtr );

		if ( pStart == g_pCmdBuf )
			col += STR_PROMPT_LEN;

		Printf( TERM_ERASE_LINE_TO_END
				TERM_CURSOR_DN
				TERM_DELETE_LINES_N
				TERM_CURSOR_UP
				TERM_CURSOR_HORZ_N,
				lines,
				col );

		char *pEnd = LineEnd( g_pCmdPtr + byteCount );
		int len = pEnd - ( g_pCmdPtr + byteCount );

		if ( len )
		{
			Printw( g_pCmdPtr + byteCount, len );
			Printf( TERM_CURSOR_HORZ_N, col );
		}
	}

	memmove( g_pCmdPtr, g_pCmdPtr + byteCount, g_pCmdEnd - ( g_pCmdPtr + byteCount ) );
	g_pCmdEnd -= byteCount;
	*g_pCmdEnd = 0;
}

void ExecutePrompt( HSQUIRRELVM vm )
{
	Assert( g_pCmdEnd - g_pCmdBuf > 0 );
	*g_pCmdEnd = 0;

	// Move cursor to end
	if ( g_pCmdPtr != g_pCmdEnd )
	{
		int lines = LineCountAhead( g_pCmdPtr );
		char *pLastLineStart = LineStart( g_pCmdEnd );
		int col = LineColumnEnd( pLastLineStart );
		Printf( TERM_CURSOR_DN_N TERM_CURSOR_HORZ_N, lines, col );
	}

	Print( "\r\n" TERM_CLR_BRIGHT_FG );

	g_bErrorHighlight = true;
	Execute( vm, g_pCmdBuf, g_pCmdEnd - g_pCmdBuf );
	g_bErrorHighlight = false;
	PushHistory( g_pCmdBuf, g_pCmdEnd - g_pCmdBuf );

	Print( TERM_CLR_BRIGHT_RESTORE "\r\n" );
	PutPrompt();

	g_pCmdPtr = g_pCmdBuf;
	g_pCmdEnd = g_pCmdBuf;
}

void Input( HSQUIRRELVM vm, HSQDEBUGSERVER dbg )
{
	PutPrompt();
	fflush( g_StreamOut );

	for (;;)
	{
		int ret;
#ifdef _WIN32
		WCHAR ch;
		HANDLE hStdin = GetStdHandle( STD_INPUT_HANDLE );
		DWORD wait = WaitForSingleObject( hStdin, 10 );
		DWORD nRead;
		INPUT_RECORD recs[1];

		if ( wait != WAIT_OBJECT_0 )
			goto CONTINUE;

		if ( !ReadConsoleInputW( hStdin, recs, 1, &nRead ) )
		{
			PrintWin32Error( "stdin read failure: %s\n", GetLastError() );
			return;
		}

		if ( !nRead )
			goto CONTINUE;

		if ( recs[0].EventType != KEY_EVENT ||
				!recs[0].Event.KeyEvent.bKeyDown ||
				!recs[0].Event.KeyEvent.uChar.UnicodeChar )
			goto CONTINUE;

		// NOTE: Windows console sends unicode multibyte sequences asynchronously,
		// read wchar instead of byte
		ch = recs[0].Event.KeyEvent.uChar.UnicodeChar;
#else
		char ch;
		int nRead;
		struct pollfd pfd;
		pfd.fd = _fileno( g_StreamIn );
		pfd.events = POLLIN;

		if ( poll( &pfd, 1, -1 ) == -1 )
		{
			Errorf( "stdin read failure: %s\n", strerror( errno ) );
			return;
		}

		if ( !( pfd.revents & POLLIN ) )
			goto CONTINUE;

		nRead = fread( &ch, 1, sizeof(ch), g_StreamIn );

		if ( nRead <= 0 )
			goto CONTINUE;
#endif
		ret = DoInput( vm, ch );

		if ( ret >= 1 )
		{
			if ( ret == 1 )
				ret = 0;

			if ( !dbg )
				dbg = sqdbg_attach_debugger( vm );

			if ( sqdbg_listen_socket( dbg, ret ) != 0 )
				dbg = NULL;
		}
		else if ( ret == -1 )
		{
			return;
		}

CONTINUE:
		fflush( g_StreamOut );

		if ( dbg )
			sqdbg_frame( dbg );
	}
}

#ifdef _WIN32
int DoInput( HSQUIRRELVM vm, wchar_t ch )
#else
int DoInput( HSQUIRRELVM vm, char ch )
#endif
{
	Assert( g_pCmdPtr >= g_pCmdBuf && g_pCmdPtr <= g_pCmdEnd );

	if ( _isprint( ch ) )
	{
		// dedent
		if ( ch == '}' && IsTabBack( g_pCmdPtr ) )
			Erase( 1 );

		Put( ch );
		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
	else if ( ch == '\t' )
	{
		Puts( TAB_STR );
		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
	// New line or execute
	else if ( ch == CTRLCH('M') ) // \r
	{
		if ( g_pCmdEnd == g_pCmdBuf )
			return 0;

		char *pStart = LineStart( g_pCmdPtr );
		bool bBracketOpen = ( g_pCmdPtr != g_pCmdBuf && g_pCmdPtr[-1] == '{' );
		bool bIndent = ( pStart != g_pCmdBuf || bBracketOpen );

		// Already indented, no bracket on cursor, or closer at start
		if ( !bIndent || *pStart == '}' )
		{
			ExecutePrompt( vm );
		}
		// New line; increment or maintain indentation
		else
		{
			int tabs = CountTabsFront( pStart ) + (int)bBracketOpen;
			int lines = LineCountAhead( g_pCmdPtr ) - 1;

			// Put NL at the end to auto-scroll the terminal output
			if ( *g_pCmdPtr != '\n' && *g_pCmdPtr != 0 )
				Print( TERM_ERASE_LINE_TO_END );

			if ( lines )
				Printf( TERM_CURSOR_DN_N, lines );

			Put( '\n' );

			if ( lines )
				Printf( TERM_CURSOR_UP_N, lines );

			Print( TERM_INSERT_LINE );

			for ( int i = 0; i < tabs; i++ )
				Puts( TAB_STR );

			// Line was broken, reprint
			char *pEnd = LineEnd( g_pCmdPtr );
			int len = pEnd - g_pCmdPtr;

			if ( len )
			{
				Printw( g_pCmdPtr, len );

				int col = tabs * TAB_LEN + 1;
				Printf( TERM_CURSOR_HORZ_N, col );
			}
		}

		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
	// Always execute
	else if ( ch == CTRLCH('J') ) // \n
	{
		if ( g_pCmdEnd == g_pCmdBuf )
			return 0;

		ExecutePrompt( vm );
		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
	else if ( ch == CTRLCH('H') || ch == 0x7F )
	{
		if ( g_pCmdPtr == g_pCmdBuf )
			return 0;

		Erase( 1 );
		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
	else if ( ch == CTRLCH('W') )
	{
		if ( g_pCmdPtr == g_pCmdBuf )
			return 0;

		char *pNext = WordBoundPrev( g_pCmdPtr, 1 );

		if ( pNext == g_pCmdPtr )
			pNext = WordBoundPrev( g_pCmdPtr );

		char *pEnd = LineEnd( pNext );

		// Next is on another line, delete line instead
		if ( g_pCmdPtr > pEnd )
			pNext = pEnd;

		int count = g_pCmdPtr - pNext;
		Erase( count, true );
		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
	else if ( ch == CTRLCH('P') )
	{
CTRL_P:
		char *pStart = LineStart( g_pCmdPtr );
		char *pNextStart = PrevLineStart( pStart );

		// Move between lines
		if ( pStart != pNextStart )
		{
			int curcol = g_nNavColumn;
			int nextcol = LineColumnEnd( pNextStart );

			if ( curcol > nextcol )
				curcol = nextcol;

			g_pCmdPtr = ByteAtColumn( pNextStart, curcol );

			// Don't step into the middle of a tab
			char *p = g_pCmdPtr;
			while ( p != g_pCmdBuf && p[-1] == ' ' )
				p--;
			if ( p != g_pCmdPtr && IsTabFront( p ) )
				g_pCmdPtr = p + TAB_LEN;

			curcol = ColumnAtByte( pNextStart, g_pCmdPtr );

			if ( pNextStart == g_pCmdBuf )
				curcol += STR_PROMPT_LEN;

			Printf( TERM_CURSOR_UP TERM_CURSOR_HORZ_N, curcol );
		}
		// Cycle history
		else
		{
			GetHistory( -1 );
		}
	}
	else if ( ch == CTRLCH('N') )
	{
CTRL_N:
		char *pNextStart = LineEnd( g_pCmdPtr ) + 1;

		// Move between lines
		if ( g_pCmdPtr != g_pCmdEnd && pNextStart <= g_pCmdEnd )
		{
			int curcol = g_nNavColumn;
			int nextcol = LineColumnEnd( pNextStart );

			if ( curcol > nextcol )
				curcol = nextcol;

			g_pCmdPtr = ByteAtColumn( pNextStart, curcol );

			// Don't step into the middle of a tab
			char *p = g_pCmdPtr;
			while ( p != g_pCmdBuf && p[-1] == ' ' )
				p--;
			if ( p != g_pCmdPtr && IsTabFront( p ) )
				g_pCmdPtr = p + TAB_LEN;

			curcol = ColumnAtByte( pNextStart, g_pCmdPtr );

			Printf( TERM_CURSOR_DN TERM_CURSOR_HORZ_N, curcol );
		}
		// Cycle history
		else
		{
			GetHistory( 1 );
		}
	}
	else if ( ch == CTRLCH('E') )
	{
CTRL_E:
		if ( g_pCmdPtr == g_pCmdEnd )
			return 0;

		int col, lines;
		char *pStart = LineStart( g_pCmdPtr );
		char *pEnd = LineEnd( pStart, &col );

		// Move to the very end
		if ( g_pCmdPtr[0] == '\n' && ( lines = LineCountAhead( g_pCmdPtr ) - 1 ) != 0 )
		{
			col = LineColumnEnd( LineStart( g_pCmdEnd ) );
			Printf( TERM_CURSOR_LINE_DN_N TERM_CURSOR_HORZ_N, lines, col );
			pEnd = g_pCmdEnd;
		}
		else
		{
			if ( pStart == g_pCmdBuf )
				col += STR_PROMPT_LEN;

			Printf( TERM_CURSOR_HORZ_N, col );
		}

		g_pCmdPtr = pEnd;
		UpdateNavColumn();
	}
	else if ( ch == CTRLCH('A') )
	{
CTRL_A:
		if ( g_pCmdPtr == g_pCmdBuf )
			return 0;

		char *pStart = LineStart( g_pCmdPtr );

		// Move to the very start
		if ( g_pCmdPtr[-1] == '\n' )
		{
			int lines = LineCountUpTo( pStart ) - 1;
			Printf( TERM_CURSOR_LINE_UP_N TERM_CURSOR_HORZ_N, lines, STR_PROMPT_LEN + 1 );
			pStart = g_pCmdBuf;
		}
		// At the start, reprint prompt instead of moving cursor
		else if ( pStart == g_pCmdBuf )
		{
			Printc( '\r' );
			Print( STR_PROMPT );
		}
		else
		{
			Printc( '\r' );
		}

		g_pCmdPtr = pStart;
		UpdateNavColumn();
	}
	else if ( ch == CTRLCH('F') )
	{
CTRL_F:
		if ( g_pCmdPtr == g_pCmdEnd )
			return 0;

		int bytes;

		if ( g_pCmdPtr[0] == '\n' )
		{
			bytes = 1;
			Print( "\r\n" );
		}
		else if ( IsTabFront( g_pCmdPtr ) )
		{
			bytes = TAB_LEN;
			Printf( TERM_CURSOR_RT_N, bytes );
		}
		else
		{
			int count = 1;
			bytes = IsValidUTF8( g_pCmdPtr, g_pCmdEnd - g_pCmdPtr );

			if ( bytes == 0 )
			{
				bytes = 1;
			}
			else if ( bytes > 2 && IsDoubleWidth( g_pCmdPtr ) )
			{
				count++;
			}

			Printf( TERM_CURSOR_RT_N, count );
		}

		g_pCmdPtr += bytes;
		UpdateNavColumn();
	}
	else if ( ch == CTRLCH('B') )
	{
CTRL_B:
		if ( g_pCmdPtr == g_pCmdBuf )
			return 0;

		int bytes;

		if ( g_pCmdPtr[-1] == '\n' )
		{
			bytes = 1;

			char *pStart = LineStart( g_pCmdPtr - bytes );
			int col = LineColumnEnd( pStart );

			if ( pStart == g_pCmdBuf )
				col += STR_PROMPT_LEN;

			Printf( TERM_CURSOR_UP TERM_CURSOR_HORZ_N, col );
		}
		else if ( IsTabBack( g_pCmdPtr ) )
		{
			bytes = TAB_LEN;
			Printf( TERM_CURSOR_LF_N, bytes );
		}
		else
		{
			int count = 1;
			char *ptr = UTF8Start( g_pCmdPtr - 1, g_pCmdBuf );

			if ( !ptr )
				ptr = g_pCmdPtr - 1;

			bytes = IsValidUTF8( ptr, g_pCmdEnd - ptr );

			if ( bytes == 0 )
			{
				bytes = 1;
			}
			else if ( bytes > 2 && IsDoubleWidth( ptr ) )
			{
				count++;
			}

			Printf( TERM_CURSOR_LF_N, count );
		}

		g_pCmdPtr -= bytes;
		UpdateNavColumn();
	}
	else if ( ch == CTRLCH('D') )
	{
		if ( g_pCmdEnd == g_pCmdBuf )
		{
			Print( "^D" );
#ifdef _DEBUG
			return -1;
#else
			RestoreTerm();
#ifdef _WIN32
			ExitProcess(0);
#else
			exit(0);
#endif
#endif
		}

CTRL_D:
		if ( g_pCmdPtr == g_pCmdEnd )
			return 0;

		EraseFront( 1 );
		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
	else if ( ch == CTRLCH('K') )
	{
		if ( g_pCmdPtr == g_pCmdEnd )
			return 0;

		if ( g_pCmdPtr[0] == '\n' )
			return 0;

		char *pStart = LineStart( g_pCmdPtr );
		char *pEnd = LineEnd( pStart );

		EraseFront( pEnd - g_pCmdPtr, true );
		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
	else if ( ch == CTRLCH('U') )
	{
		if ( g_pCmdPtr == g_pCmdBuf )
			return 0;

		if ( g_pCmdPtr[-1] == '\n' )
			return 0;

		char *pStart = LineStart( g_pCmdPtr );

		Erase( g_pCmdPtr - pStart, true );
		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
	else if ( ch == CTRLCH('V') )
	{
CTRL_V:
		ClipboardPaste();
		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
	else if ( ch == CTRLCH('[') )
	{
		char seq[TERM_SEQ_MAX_LEN];
#ifdef _WIN32
		HANDLE hStdin = GetStdHandle( STD_INPUT_HANDLE );
		DWORD nRead;
		INPUT_RECORD recs[ARRAYSIZE(seq)];

		if ( PeekConsoleInputA( hStdin, recs, ARRAYSIZE(recs), &nRead ) && nRead )
#else
		int nRead = fread( &seq, 1, ARRAYSIZE(seq), g_StreamIn );

		if ( nRead > 0 )
#endif
		{
#ifdef _WIN32
			ReadConsoleInputA( hStdin, recs, nRead, &nRead );

			int c = 0;
			for ( int i = 0; i < (int)nRead; i++ )
			{
				const INPUT_RECORD &rec = recs[i];
				if ( rec.EventType == KEY_EVENT &&
						rec.Event.KeyEvent.bKeyDown &&
						rec.Event.KeyEvent.uChar.AsciiChar )
				{
					seq[c++] = rec.Event.KeyEvent.uChar.AsciiChar;
				}
			}

			nRead = c;
#endif

			if ( nRead > 1 )
			{
				if ( IsSeq( seq, SEQ_RIGHTARROW ) )
				{
					goto CTRL_F;
				}
				else if ( IsSeq( seq, SEQ_LEFTARROW ) )
				{
					goto CTRL_B;
				}
				else if ( IsSeq( seq, SEQ_UPARROW ) )
				{
					goto CTRL_P;
				}
				else if ( IsSeq( seq, SEQ_DOWNARROW ) )
				{
					goto CTRL_N;
				}
				else if ( IsSeq( seq, SEQ_DEL ) )
				{
					goto CTRL_D;
				}
				else if ( IsSeq( seq, SEQ_C_DEL ) )
				{
					goto ALT_D;
				}
				else if ( IsSeq( seq, SEQ_PGUP ) )
				{
					GetHistory( -1 );
				}
				else if ( IsSeq( seq, SEQ_PGDN ) )
				{
					GetHistory( 1 );
				}
				else if ( IsSeq( seq, SEQ_HOME ) )
				{
					goto CTRL_A;
				}
				else if ( IsSeq( seq, SEQ_END ) )
				{
					goto CTRL_E;
				}
				else if ( IsSeq( seq, SEQ_CRIGHT ) )
				{
					goto ALT_F;
				}
				else if ( IsSeq( seq, SEQ_CLEFT ) )
				{
					goto ALT_B;
				}
				else if ( IsSeq( seq, SEQ_S_INS ) )
				{
					goto CTRL_V;
				}
				else if ( IsSeq( seq, SEQ_C_INS ) )
				{
					ClipboardCopy();
				}
				else if ( IsSeq( seq, SEQ_F1 ) )
				{
					// Connect sqdbg server
					int lines = LineCountAhead( g_pCmdPtr ) - 1;

					if ( lines == 0 )
					{
						Print( "\r\n" );
					}
					else
					{
						Printf( TERM_CURSOR_LINE_DN_N "\r\n", lines );
					}

					int val = atoi( g_pCmdBuf, g_pCmdEnd - g_pCmdBuf );

					g_pCmdPtr = g_pCmdBuf;
					g_pCmdEnd = g_pCmdBuf;

					if ( val > 0 )
						return val;

					return 1;
				}
#if 0
				else
				{
					Puts( seq, nRead );
					UpdateNavColumn();
					InvalidateHistoryIndex();
				}
#endif
			}
			// ALT+KEY
			else // nRead == 1
			{
				if ( seq[0] == 'f' )
				{
ALT_F:
					if ( g_pCmdPtr == g_pCmdEnd )
						return 0;

					char *pNext = WordBoundNext( g_pCmdPtr );
					int lines = LineCountBetween( g_pCmdPtr, pNext ) - 1;

					if ( lines )
						Printf( TERM_CURSOR_DN_N, lines );

					char *pStart = LineStart( pNext );
					int col = ColumnAtByte( pStart, pNext );

					if ( pStart == g_pCmdBuf )
						col += STR_PROMPT_LEN;

					Printf( TERM_CURSOR_HORZ_N, col );

					g_pCmdPtr = pNext;
					UpdateNavColumn();
				}
				else if ( seq[0] == 'b' )
				{
ALT_B:
					if ( g_pCmdPtr == g_pCmdBuf )
						return 0;

					char *pNext = WordBoundPrev( g_pCmdPtr );
					int lines = LineCountBetween( pNext, g_pCmdPtr ) - 1;

					if ( lines )
						Printf( TERM_CURSOR_UP_N, lines );

					char *pStart = LineStart( pNext );
					int col = ColumnAtByte( pStart, pNext );

					if ( pStart == g_pCmdBuf )
						col += STR_PROMPT_LEN;

					Printf( TERM_CURSOR_HORZ_N, col );

					g_pCmdPtr = pNext;
					UpdateNavColumn();
				}
				else if ( seq[0] == 'd' )
				{
ALT_D:
					if ( g_pCmdPtr == g_pCmdEnd )
						return 0;

					char *pNext = WordBoundNext( g_pCmdPtr );
					int count = pNext - g_pCmdPtr;
					EraseFront( count, true );
					UpdateNavColumn();
					InvalidateHistoryIndex();
				}
			}
		}
		//else // nRead == 0
	}
	else if ( ch == CTRLCH('L') )
	{
		g_pCmdPtr = g_pCmdBuf;
		g_pCmdEnd = g_pCmdBuf;

		Print( "^L" TERM_CURSOR_POS_11 TERM_ERASE_DISPLAY );

		PutPrompt();
		UpdateNavColumn();
		InvalidateHistoryIndex();
	}
#ifdef _WIN32
	else if ( ch > 0x7F )
#else
	else if ( ch & 0x80 )
#endif
	{
		char seq[4];
		int nRead;

#ifdef _WIN32
		nRead = WCharToUTF8( seq, ARRAYSIZE(seq), &ch, 1 );
#else
		seq[0] = ch;
		nRead = fread( seq + 1, 1, ARRAYSIZE(seq) - 1, g_StreamIn );
#endif
		if ( nRead > 0 )
		{
#ifndef _WIN32
			nRead++;
#endif

			if ( IsValidUTF8( seq, nRead ) )
			{
				Puts( seq, nRead );
			}
			else
			{
				for ( int i = 0; i < nRead; i++ )
				{
					char buf[5];
					snprintf( buf, sizeof(buf), "<%02x>", (unsigned char)seq[i] );
					Puts( buf, 4 );
				}
			}

			UpdateNavColumn();
			InvalidateHistoryIndex();
		}
	}
	else
	{
		Assert( ch >= 0 && ch < 0x20 );
		Put( '^' );
		Put( ch + '@' );
	}

	Assert( g_pCmdPtr >= g_pCmdBuf && g_pCmdPtr <= g_pCmdEnd );
	return 0;
}

const char *GetType( HSQOBJECT obj )
{
	switch ( sq_type(obj) )
	{
		case OT_NULL: return "null";
		case OT_INTEGER: return "integer";
		case OT_FLOAT: return "float";
		case OT_BOOL: return "bool";
		case OT_STRING: return "string";
		case OT_TABLE: return "table";
		case OT_ARRAY: return "array";
		case OT_GENERATOR: return "generator";
		case OT_CLOSURE: return "function";
		case OT_NATIVECLOSURE: return "native function";
		case OT_THREAD: return "thread";
		case OT_CLASS: return "class";
		case OT_INSTANCE: return "instance";
		case OT_WEAKREF: return "weakref";
		case OT_USERDATA:
		case OT_USERPOINTER: return "userdata";
		case OT_FUNCPROTO: return "funcproto";
		default: return "unknown";
	}
}

struct HSQOBJECT_KV
{
	HSQOBJECT key;
	HSQOBJECT val;
};

static int _sortkeys( const HSQOBJECT_KV *a, const HSQOBJECT_KV *b )
{
	if ( sq_type(a->key) == OT_STRING )
	{
		if ( sq_type(b->key) == OT_STRING )
		{
			return scstricmp( _string(a->key)->_val, _string(b->key)->_val );
		}
		else
		{
			return 1;
		}
	}
	else
	{
		if ( sq_type(b->key) == OT_STRING )
		{
			return -1;
		}
		else
		{
			return ( _integer(a->key) >= _integer(b->key) );
		}
	}
}

static bool ShouldEscape( const SQChar *ptr, SQInteger len )
{
	Assert( len > 0 );

	if ( _isdigit( *ptr ) )
		return true;

	const SQChar *end = ptr + len;

	do
	{
		if ( !_isalnum( *ptr ) && *ptr != '_' )
			return true;

#ifdef SQUNICODE
		int bytes = IsValidUnicode( ptr, len );
		if ( bytes <= 0 )
			return true;
#else
		int bytes = IsValidUTF8( ptr, len );
		if ( bytes == 0 )
			return true;
#endif
		ptr += bytes;
	}
	while ( ptr < end );

	return false;
}

void PrintArray( HSQUIRRELVM vm, HSQOBJECT array )
{
	bool _g_bTableIndent = g_bTableIndent;

	// Expand for json, otherwise keep it compact
	if ( !g_bTableJSONOutput )
		g_bTableIndent = false;

	if ( g_bTableJSONOutput )
		g_TablePrintIndent++;

	Printc( '[' );

	SQInteger c = _array(array)->Size();

	for ( SQInteger i = 0; i < c; i++ )
	{
		SQObjectPtr val;
		_array(array)->Get( i, val );

		if ( g_bTableJSONOutput && g_bTableIndent )
		{
			Print( "\r\n" );

			for ( int pi = 0; pi < g_TablePrintIndent; pi++ )
				Print( TAB_STR );
		}

		PrintObj( vm, val, true );

		if ( i + 1 != c )
		{
			Printc( ',' );

			if ( !g_bTableJSONOutput || !g_bTableIndent )
				Printc( ' ' );
		}
	}

	if ( g_bTableJSONOutput )
	{
		g_TablePrintIndent--;

		if ( g_bTableIndent )
		{
			Print( "\r\n" );

			for ( int pi = 0; pi < g_TablePrintIndent; pi++ )
				Print( TAB_STR );
		}
	}

	Printc( ']' );
	g_bTableIndent = _g_bTableIndent;
}

void PrintTable( HSQUIRRELVM vm, HSQOBJECT obj, bool isClass )
{
	g_TablePrintIndent++;
	Printc( '{' );

	if ( g_TablePrintIndent <= ARRAYSIZE(g_TablePrintStack) )
	{
		g_TablePrintStack[ g_TablePrintIndent - 1 ] = _refcounted(obj);

		STACKCHECK( vm );

		if ( !isClass )
		{
			sq_pushobject( vm, obj );
		}
		else
		{
			HSQOBJECT table;
			sq_type(table) = OT_TABLE;
			_table(table) = _class(obj)->_members;
			sq_pushobject( vm, table );
		}

		SQInteger count = sq_getsize( vm, -1 );
		HSQOBJECT_KV *sorted;

		if ( count < 1024 )
		{
			sorted = (HSQOBJECT_KV*)alloca( count * sizeof(HSQOBJECT_KV) );
		}
		else
		{
			sorted = (HSQOBJECT_KV*)malloc( count * sizeof(HSQOBJECT_KV) );
		}

		int sorted_count = 0;
		int field_count = 0;

		sq_pushinteger( vm, 0 );

		for ( HSQOBJECT it; SQ_SUCCEEDED( sq_next( vm, -2 ) ); )
		{
			HSQOBJECT_KV kv;
			sq_getstackobj( vm, -3, &it );
			sq_getstackobj( vm, -2, &kv.key );
			sq_getstackobj( vm, -1, &kv.val );

			if ( !isClass )
			{
				sorted[ sorted_count++ ] = kv;
			}
			else
			{
				// field/method separation
				if ( _isfield(kv.val) )
				{
					if ( field_count != sorted_count )
					{
						memmove( sorted + field_count + 1,
								sorted + field_count,
								( sorted_count - field_count ) * sizeof(*sorted) );
					}

					kv.val = _class(obj)->_defaultvalues[ _member_idx(kv.val) ].val;
					sorted[ field_count++ ] = kv;
					sorted_count++;
				}
				else
				{
					kv.val = _class(obj)->_methods[ _member_idx(kv.val) ].val;
					sorted[ sorted_count++ ] = kv;
				}
			}

			sq_pop( vm, 2 );

			if ( _integer(it) == -1 )
				break;
		}

		sq_pop( vm, 2 );

		if ( !isClass )
		{
			qsort( sorted, count,
					sizeof(*sorted), (int (*)(const void *, const void *))_sortkeys );
		}
		else
		{
			if ( field_count )
			{
				qsort( sorted, field_count,
						sizeof(*sorted), (int (*)(const void *, const void *))_sortkeys );
			}

			if ( sorted_count - field_count )
			{
				qsort( sorted + field_count, sorted_count - field_count,
						sizeof(*sorted), (int (*)(const void *, const void *))_sortkeys );
			}
		}

		Assert( sorted_count == count );

		for ( SQInteger k = 0; k < count; k++ )
		{
			const HSQOBJECT_KV &kv = sorted[k];

			if ( g_bTableIndent )
			{
				Print( "\r\n" );

				for ( int pi = 0; pi < g_TablePrintIndent; pi++ )
					Print( TAB_STR );
			}

			if ( !g_bTableJSONOutput )
			{
				// Only escape keys that need it
				if ( !( sq_type(kv.key) != OT_STRING ||
							_string(kv.key)->_len == 0 ||
							ShouldEscape( _string(kv.key)->_val, _string(kv.key)->_len ) ) )
				{
					PrintObj( vm, kv.key );
				}
				else
				{
					Printc( '[' );
					PrintObj( vm, kv.key, true );
					Printc( ']' );
				}
			}
			else
			{
				PrintObj( vm, kv.key, true );
			}

			if ( !g_bTableJSONOutput )
			{
				Print( " = " );
			}
			else
			{
				Print( ": " );
			}

			// self reference?
			if ( ISREFCOUNTED( sq_type(kv.val) ) )
			{
				for ( int pi = 0; pi < g_TablePrintIndent; pi++ )
				{
					if ( _refcounted(kv.val) == g_TablePrintStack[pi] )
					{
						Printf( "@%i", g_TablePrintIndent - pi - 1 );
						goto skip;
					}
				}
			}

			PrintObj( vm, kv.val, true );

skip:
			if ( k + 1 != count )
			{
				Printc( ',' );

				if ( !g_bTableIndent )
					Printc( ' ' );
			}
		}

		if ( g_bTableIndent )
			Print( "\r\n" );

		if ( count >= 1024 )
			free( sorted );
	}
	else if ( g_bTableIndent )
	{
		Print( "\r\n" );

		for ( int pi = 0; pi < g_TablePrintIndent; pi++ )
			Print( TAB_STR );

		Print( "..." );
		Print( "\r\n" );
	}
	else
	{
		Print( "..." );
	}

	g_TablePrintIndent--;

	if ( g_bTableIndent )
	{
		for ( int pi = 0; pi < g_TablePrintIndent; pi++ )
			Print( TAB_STR );
	}

	Printc( '}' );
}

void PrintObj( HSQUIRRELVM vm, HSQOBJECT obj, bool escape )
{
	while ( sq_type(obj) == OT_WEAKREF )
		obj = _weakref(obj)->_obj;

	switch ( sq_type(obj) )
	{
		case OT_BOOL:
			Print( _integer(obj) ? "true" : "false" );
			break;
		case OT_INTEGER:
			Printf( FMT_INT, _integer(obj) );
			break;
		case OT_FLOAT:
			Printf( "%." FMT_FLT_DIG_STR "g", _float(obj) );
			break;
		case OT_STRING:
#ifdef SQUNICODE
			PrintWCharToUTF8( g_StreamOut, _string(obj)->_val, _string(obj)->_len,
					escape + (char)g_bTableJSONOutput );
#else
			if ( escape )
			{
				PrintEscaped( g_StreamOut, _string(obj)->_val, _string(obj)->_len,
						g_bTableJSONOutput );
			}
			else
			{
				Printw( _string(obj)->_val, _string(obj)->_len );
			}
#endif
			break;
		case OT_ARRAY:
			PrintArray( vm, obj );
			break;
		case OT_TABLE:
			PrintTable( vm, obj );
			break;
		case OT_CLASS:
			Print( "class " );
			PrintTable( vm, obj, true );
			break;
		case OT_NULL:
			Print( "null" );
			break;
		case OT_USERDATA:
			if ( _userdata(obj)->_delegate )
			{
				sq_pushobject( vm, obj );

#if SQUIRREL_VERSION_NUMBER >= 300
				if ( SQ_SUCCEEDED( sq_tostring( vm, -1 ) ) )
#else
				sq_tostring( vm, -1 );
#endif
				{
					sq_getstackobj( vm, -1, &obj );
					Assert( sq_type(obj) == OT_STRING );
#ifdef SQUNICODE
					PrintWCharToUTF8( g_StreamOut, _string(obj)->_val, _string(obj)->_len );
#else
					Printw( _string(obj)->_val, _string(obj)->_len );
#endif
					sq_pop( vm, 2 );
					break;
				}

#if SQUIRREL_VERSION_NUMBER >= 300
				sq_pop( vm, 1 );
#endif
			}
		default:
			Printf( "<%s> 0x" FMT_PTR, GetType( obj ), (uintptr_t)_rawval(obj) );
	}
}

void DoExecute( HSQUIRRELVM vm, const SQChar *str, int len, char **argv )
{
	STACKCHECK( vm );
	Assert( str[len] == 0 );

	// If the input ends with ';;', don't print
	bool printout = !( len > 2 && str[len - 1] == ';' && str[len - 2] == ';' );
	if ( !printout )
		len--;

#ifndef SQDBG_DISABLE_COMPILER
	bool sqdbgcompile = !argv;

	for ( int i = 0; i < len; i++ )
	{
		if ( str[i] == '\n' )
		{
			sqdbgcompile = false;
			break;
		}
	}

	if ( sqdbgcompile )
	{
		sq_pushroottable( vm );
		sq_pushstring( vm, _SC("sqdbg_eval"), STRLEN("sqdbg_eval") );

		if ( SQ_SUCCEEDED( sq_get( vm, -2 ) ) )
		{
			sq_pushnull( vm );
			sq_pushstring( vm, str, len );

			if ( SQ_SUCCEEDED( sq_call( vm, 2, (SQBool)printout, SQTrue ) ) )
			{
				if ( printout )
				{
					HSQOBJECT ret;
					sq_getstackobj( vm, -1, &ret );
					if ( sq_type(ret) != OT_NULL )
						PrintObj( vm, ret );
					sq_pop( vm, 1 );
				}
			}
			else
			{
				// Highlight column from sqdbg error message: "... @#"
				if ( g_bErrorHighlight &&
						sq_type(vm->_lasterror) == OT_STRING &&
						_string(vm->_lasterror)->_len > 4 )
				{
					const SQChar *sptr = _string(vm->_lasterror)->_val;
					SQInteger slen = _string(vm->_lasterror)->_len;

					for ( const SQChar *c = max( sptr, sptr + slen - 5 );
							c < sptr + slen;
							c++ )
					{
						if ( *c == '@' )
						{
							int col = atoi( c + 1, sptr + slen - ( c + 1 ) );

							if ( col != 0 )
							{
								col += STR_PROMPT_LEN;
								for ( int i = 1; i < col; i++ )
									Errorc( ' ' );
								Errorc( '^' );
								Errorc( '\n' );
							}

							break;
						}
					}
				}

				Error( "<ERROR:" );
				g_StreamOut = stderr;
				PrintObj( vm, vm->_lasterror );
				g_StreamOut = stdout;
				Errorc( '>' );
			}

			sq_pop( vm, 2 );
			return;
		}
		else
		{
			sq_pop( vm, 1 );
		}
	}
#endif

	if ( SQ_SUCCEEDED( sq_compilebuffer( vm, str, ( len + 1 ) * sizeof(SQChar), _SC("sq"), SQTrue ) ) )
	{
		sq_pushroottable( vm );

		int argc = 1;
		if ( argv )
			argc += PushArgv( vm, argv );

		if ( SQ_SUCCEEDED( sq_call( vm, argc, (SQBool)printout, SQTrue ) ) )
		{
			if ( printout )
			{
				HSQOBJECT ret;
				sq_getstackobj( vm, -1, &ret );
				if ( sq_type(ret) != OT_NULL )
					PrintObj( vm, ret );
				sq_pop( vm, 1 );
			}
		}

		sq_pop( vm, 1 );
	}
	else
	{
#ifdef NOSQSTDLIB
		Error( "<ERROR:" );
		g_StreamOut = stderr;
		PrintObj( vm, vm->_lasterror );
		g_StreamOut = stdout;
		Errorc( '>' );
#endif
	}
}

void Execute( HSQUIRRELVM vm, const char *str, int len, char **argv )
{
	if ( !len )
		return;

#ifdef SQUNICODE
	SQChar *buf;

	if ( len < STR_EXEC_BUF_SIZE )
	{
		buf = (SQChar*)alloca( ( len + 1 ) * sizeof(SQChar) );
	}
	else
	{
		buf = (SQChar*)malloc( ( len + 1 ) * sizeof(SQChar) );
	}

	len = UTF8ToWChar( buf, ( len + 1 ) * sizeof(SQChar), str, len );
	buf[len] = 0;

	DoExecute( vm, buf, len, argv );

	if ( len >= STR_EXEC_BUF_SIZE )
		free( buf );
#else
	DoExecute( vm, str, len, argv );
#endif
}

void ReadStdin( HSQUIRRELVM vm, char **argv )
{
	char sbuf[ STDIN_READ_BUF_SIZE ];
	char *buf = sbuf;
	char *bufp = buf;
	int size = sizeof(sbuf);
	int count = size;
	int len = 0;
	int read;

read:
	read = fread( bufp, 1, count, g_StreamIn );
	bufp += read;
	len += read;

	if ( ferror( g_StreamIn ) )
		goto exit;

	if ( read < 0 )
		goto exit;

	if ( feof( g_StreamIn ) )
		goto exec;

	if ( read == 0 )
		goto exec;

	if ( len == size )
	{
		count = 1;
		size += sizeof(sbuf) / 2;

		if ( buf == sbuf )
		{
			bufp = (char*)malloc( size );
			memcpy( bufp, buf, len );
		}
		else
		{
			bufp = (char*)realloc( buf, size );
		}

		buf = bufp;
		bufp = buf + len;
	}

	goto read;

exec:
	bufp = buf;

	// Strip whitespace
	while ( *bufp == '\n' || *bufp == '\r' || *bufp == ' ' )
	{
		bufp++;
		len--;
	}

	while ( bufp[len-1] == '\n' || bufp[len-1] == '\r' || bufp[len-1] == ' ' )
		len--;

	bufp[len] = 0;

	Execute( vm, bufp, len, argv );

exit:;
#if defined(_WIN32) && defined(_DEBUG)
	if ( buf != sbuf )
		free( buf );
#endif
}


////////////////////////////////////////////////

#ifdef SQFFI
void *TAG_LIBRARY = (void*)"sq ffi lib";
void *TAG_PROC = (void*)"sq ffi proc";
void *TAG_MEMORY = (void*)"sq ffi mem";
void *TAG_STRUCT = (void*)"sq ffi struct";
void *TAG_STRUCT_DEF = (void*)"sq ffi structdef";
void *TAG_ARRAY = (void*)"sq ffi array";
#ifdef FFI_CLOSURES
void *TAG_CLOSURE = (void*)"sq ffi closure";
#endif

enum EFFIType
{
	FFIType_void = 0,
	FFIType_var,

	FFIType_INTEGRAL_BEGIN = 0x10,
	FFIType_i8 = FFIType_INTEGRAL_BEGIN,
	FFIType_u8,
	FFIType_i16,
	FFIType_u16,
	FFIType_i32,
	FFIType_u32,
	FFIType_i64,
	FFIType_u64,
	FFIType_INTEGRAL_END = FFIType_u64,

	FFIType_f32 = 0x20,
	FFIType_f64,

	FFIType_struct = 0x40,
	FFIType_structdef,

	FFIType_fn = 0x80,
	FFIType_fndec,
	FFIType_fndef,

	FFIType_ptr = 0x100,

	FFIType_END = 0x01FF,

	FFIType_str = FFIType_ptr | FFIType_u8,
#ifdef _WIN32
	FFIType_wstr = FFIType_ptr | FFIType_u16,
#else
	FFIType_wstr = FFIType_ptr | FFIType_u32,
#endif

#ifdef _WIN32
#ifdef UNICODE
	FFIType_tstr = FFIType_wstr,
#else
	FFIType_tstr = FFIType_str,
#endif
#endif
};

enum EFFIFlag
{
	FFIFlag_ERRNO = 0x00000001,
	FFIFlag_GETLASTERROR = 0x00000002,
};

bool g_bCreateFunc = true;
int g_ffi_errno = 0;

#ifdef FFI_CLOSURES
struct tramp_data_t;
struct tramp_t;
#endif
struct sqffitype_t;
struct parser_args_t;
struct script_proc_t;
struct script_struct_def_t;
struct script_struct_t;
struct script_array_t;
#ifdef FFI_CLOSURES
struct script_closure_t;
#endif

const char *FFITypeStr( int type );
int ABI_Map( int abi );
int ABI_Get( int abi );
ffi_type *MapFFIType( sqffitype_t &type );
int MapFFIType( ffi_type *type );
template < typename T > inline int CIntToFFI();
int FFITypeByteSize( int type );

void SetDelegateFFIMT( HSQUIRRELVM vm, int idx );
script_array_t *PushArray( HSQUIRRELVM vm, int size );
#ifdef FFI_CLOSURES
script_closure_t *PushClosure( HSQUIRRELVM vm, int size );
script_closure_t *PushClosureGlobal( HSQUIRRELVM vm, int size );
void RemoveClosureGlobal( HSQUIRRELVM vm, script_closure_t *ptr );
#endif
script_struct_t *PushStruct( HSQUIRRELVM vm, int size );
bool FindStructDef( HSQUIRRELVM vm, const SQChar *name, int namelen, SQObjectPtr &out );
void RemoveStructDef( HSQUIRRELVM vm, const HSQOBJECT &obj );
script_struct_def_t *PushStructDef( HSQUIRRELVM vm, int size, SQObjectPtr &out );
script_struct_def_t *PushStructDefGlobal( HSQUIRRELVM vm, int size, SQObjectPtr &out );

SQInteger ffi_ReleaseLibrary( SQUserPointer pModule, SQInteger );
SQInteger ffi_ReleaseProc( SQUserPointer pProc, SQInteger );
SQInteger ffi_ReleaseStruct( SQUserPointer pStruct, SQInteger );
SQInteger ffi_ReleaseStructDef( SQUserPointer pDef, SQInteger );
SQInteger ffi_ReleaseArray( SQUserPointer pArray, SQInteger );
#ifdef FFI_CLOSURES
SQInteger ffi_ReleaseClosure( SQUserPointer pClo, SQInteger );
#endif

SQInteger ffi_typeof( HSQUIRRELVM vm );
SQInteger ffi_sizeof( HSQUIRRELVM vm );
SQInteger ffi_alignof( HSQUIRRELVM vm );
SQInteger ffi_offsetof( HSQUIRRELVM vm );
SQInteger ffi_typedef( HSQUIRRELVM vm );
SQInteger ffi_LoadLibrary( HSQUIRRELVM vm );
SQInteger ffi_malloc( HSQUIRRELVM vm );
SQInteger ffi_MT_ToString( HSQUIRRELVM vm );
SQInteger ffi_MT_Add( HSQUIRRELVM vm );
SQInteger ffi_MT_Sub( HSQUIRRELVM vm );
SQInteger ffi_MT_Get( HSQUIRRELVM vm );
SQInteger ffi_MT_Set( HSQUIRRELVM vm );
SQInteger ffi_MT_Call( HSQUIRRELVM vm );
SQInteger ffi_get( HSQUIRRELVM vm );
SQInteger ffi_set( HSQUIRRELVM vm );

SQInteger ffi_GetFunction( HSQUIRRELVM vm );
SQInteger ffi_MakeFunction( HSQUIRRELVM vm );
SQInteger ffi_GetFunctions( HSQUIRRELVM vm );
SQInteger ffi_GetVirtualClass( HSQUIRRELVM vm );
SQInteger ffi_MakeArray( HSQUIRRELVM vm );
#ifdef FFI_CLOSURES
SQInteger ffi_MakeClosure( HSQUIRRELVM vm );
SQInteger ffi_Clone( HSQUIRRELVM vm );
#endif
SQInteger ffi_GetLastError( HSQUIRRELVM vm );
void RegisterFFI( HSQUIRRELVM vm, vm_init_options opt );

int DoGet( HSQUIRRELVM vm, void *addr, sqffitype_t *type );
int DoSet( HSQUIRRELVM vm, void *addr, sqffitype_t *type, const HSQOBJECT &value );
bool DoMakeFunction( HSQUIRRELVM vm, int stackindex, parser_args_t *args );
void CloneProc( HSQUIRRELVM vm, script_proc_t *src, void *func );
void InitProc( void *proc, void *func, void *thisptr, HSQOBJECT *module );
int DoCall( HSQUIRRELVM vm, int argsBase, int nargs, script_proc_t *proc );

#ifdef FFI_CLOSURES
tramp_data_t *CreateTrampoline( HSQUIRRELVM caller, HSQUIRRELVM vm,
		script_proc_t *proc, HSQOBJECT closure, void *ptdata = NULL );
void FreeTrampoline( tramp_data_t *tdata );
void TrampolineCallback( ffi_cif *cif, void *ret, void **args, void *ud );
#endif

bool DefineStruct( HSQUIRRELVM vm,
		const SQChar **signature,
		const SQChar *name,
		int namelen,
		int alignment,
		int packing,
		bool isUnion,
		SQObjectPtr &out );

#define STRCMP( ptr, len, target ) \
	( len == STRLEN(target) && !memcmp( ptr, target, STRLEN(target) * sizeof(*ptr) ) )

enum EParserToken
{
	Token_NONE = 0,
	Token_const,
	Token_unsigned,
	Token_signed,
	Token_void,
	Token_char,
	Token_short,
	Token_long,
	Token_int,
	Token_float,
	Token_double,
	Token_wchar_t,
	Token_size_t,
	Token_int8_t,
	Token_uint8_t,
	Token_int16_t,
	Token_uint16_t,
	Token_int32_t,
	Token_uint32_t,
	Token_int64_t,
	Token_uint64_t,
	Token_intptr_t,
	Token_uintptr_t,
	Token_struct,
	Token_class,
	Token_virtual,
	Token_public,
	Token_private,
	Token_typedef,
	Token_alignas,
};

struct fileindex_t
{
	int line;
	int col;
};

int g_nParserStackIndex = 0;
const int g_nParserStackCapacity = ( 255 + 1 ) * 2;
sqffitype_t *g_pParserStack = NULL;
const SQChar *g_pParserStart;

struct parsertypedef_t;
#ifndef SQ_DISABLE_TYPE_REDEFINITION
int g_nParserTypedefsUserDefIndex = 0;
#endif
int g_nParserTypedefsIndex = 0;
int g_nParserTypedefsCapacity = 0;
parsertypedef_t *g_pParserTypedefs = NULL;

void Parser_Init( const SQChar *str );
fileindex_t Parser_GetIndex( const SQChar *ptr );
void Parser_SkipWhitespace( const SQChar **str );
const SQChar *Parser_PeekIdentifier( const SQChar *str, int *len );
const SQChar *Parser_ReadIdentifier( const SQChar **str, int *len );
template < int SIZE > int Parser_Skip( const SQChar **str, const SQChar (&target)[SIZE] );
int Parser_Skip( const SQChar **str, SQChar ch );
int Parser_GetKeyword( const SQChar *id, int len );
void Parser_Typedef( HSQUIRRELVM vm, const SQChar *id, int len, const sqffitype_t &type );
bool Parser_Typedef( HSQUIRRELVM vm, const SQChar **str );
int Parser_Typedef2( HSQUIRRELVM vm, const SQChar **str );
void Parser_Typedef_Skip( HSQUIRRELVM vm, const SQChar **str );
void Parser_Typedef_Restore( int cache );
bool IsUserDef( parsertypedef_t *def );
parsertypedef_t *Parser_GetTypedef( const SQChar *id, int len );
int Parser_ReadAttribute( HSQUIRRELVM vm, const SQChar **str );
int Parser_ReadAlignas( HSQUIRRELVM vm, const SQChar **str );
int Parser_ReadStructPack( HSQUIRRELVM vm, const SQChar **str );
void Parser_ReadType( HSQUIRRELVM vm, const SQChar **str, parser_args_t *args );
bool Parser_ReadFuncArgs( HSQUIRRELVM vm, const SQChar **str, parser_args_t *args );
bool InitParserStack( HSQUIRRELVM vm, int nargs, int retBase, int argsBase );


////////////////////////////////////////////////

#ifdef FFI_CLOSURES
struct tramp_data_t
{
	void *code;
	HSQUIRRELVM vm;
	script_proc_t *proc;
	ffi_closure *tramp;
	SQObjectPtr closure;
	unsigned int refs;
	bool external;
};

struct tramp_t
{
	tramp_data_t *ptr;

	~tramp_t()
	{
		if ( ptr )
			FreeTrampoline( ptr );
	}

	tramp_data_t *operator->()
	{
		return ptr;
	}

	operator tramp_data_t*()
	{
		return ptr;
	}

	bool operator!() const
	{
		return !ptr;
	}

	tramp_t &operator=( const tramp_t &src )
	{
		return *operator=( src.ptr );
	}

	tramp_t *operator=( tramp_data_t *src )
	{
		if ( ptr )
			Release();

		if ( src )
		{
			ptr = src;
			ptr->refs++;
		}

		return this;
	}

	void Release()
	{
		Assert( ptr );
		FreeTrampoline( ptr );
		ptr = NULL;
	}
};
#endif

struct sqffitype_t
{
	int id;
	unsigned int isarray : 1;
	int count : 31;
#ifndef SQ_DISABLE_BITFIELDS
	unsigned char bits; // bit field value + shift
	unsigned char shift;
#endif
	SQObjectPtr obj;

	void Release()
	{
		Assert( IsValid() );
		id = 0;
		obj.Null();
	}

#ifndef SQ_DISABLE_BITFIELDS
	template < typename T >
	T Mask()
	{
		return ( bits != ( sizeof(T) == 8 ? 64 : 32 ) ) ?
			( ( ( (T)1 << bits ) - 1 ) & ~( ( (T)1 << shift ) - 1 ) ) :
			( ( sizeof(T) == 8 ? 0xffffffffffffffff : 0xffffffff ) << shift );
	}
#endif

#ifdef _DEBUG
	bool IsValid() const
	{
		if ( IsStruct() || IsFunction() )
			return ( ( id & (0x10|0x20) ) == 0 && sq_type(obj) == OT_USERDATA && _userdata(obj) );

		return ( count > 0 );
	}
#endif

	void Normalise()
	{
		if ( id == FFIType_fndec || id == FFIType_fndef )
		{
			id = FFIType_fn;
		}
		else if ( id == FFIType_structdef )
		{
			id = FFIType_struct;
		}
		else if ( id == ( FFIType_i8 | FFIType_ptr ) )
		{
			id = FFIType_str;
		}
		else if ( ( id & FFIType_ptr ) &&
				id != FFIType_ptr &&
				id != FFIType_str &&
				id != FFIType_wstr )
		{
			Release();
			id = FFIType_ptr;
		}
	}

	void StripPtr()
	{
		if ( ( id & FFIType_ptr ) && id != FFIType_ptr )
			id &= ~FFIType_ptr;

		isarray = 0;
		count = 0;
	}

	bool IsFunction() const
	{
		return ( id & FFIType_fn ) && !( id & FFIType_ptr );
	}

	bool IsStruct() const
	{
		return ( id & FFIType_struct ) && !( id & FFIType_ptr );
	}

	script_struct_def_t *StructDef()
	{
		Assert( IsStruct() );
		Assert( sq_type(obj) == OT_USERDATA && _userdata(obj) );
		return (script_struct_def_t*)_userdataval(obj);
	}

	script_proc_t *Proc()
	{
		Assert( IsFunction() );
		Assert( sq_type(obj) == OT_USERDATA && _userdata(obj) );
		return (script_proc_t*)_userdataval(obj);
	}

	const char *Str()
	{
		return FFITypeStr( id );
	}

	int ElementSize();
	int Alignment();

	int ByteSize()
	{
		Assert( count > 0 );
		return ElementSize() * count;
	}
};

struct parsertypedef_t : sqffitype_t
{
	SQObjectPtr name;

	void Release()
	{
		name.Null();
		sqffitype_t::Release();
	}
};

struct parser_args_t
{
	const SQChar *name;
	int namelen;

	int nargs;
	int abi;
	int flags;

	bool has_thisptr;
	bool has_type;

	parsertypedef_t *tdef;

	sqffitype_t type;
	sqffitype_t rettype;
};

struct script_proc_t
{
	void *func;
	void *thisptr;
	ffi_cif cif;
	int nfixedargs;
	int argssize;
	int abi;
	int flags;
	SQObjectPtr module;

	// size: nfixedargs + 1
	// return type: argtypes[nfixedargs]
	sqffitype_t *argtypes;
	ffi_type **fixed_argtypes;
};

struct script_struct_def_t
{
	struct member_t
	{
		int offset;
		SQObjectPtr key;
		sqffitype_t type;
	};

	SQObjectPtr name;
	int membercount;
	ffi_type ffitype;
	member_t members[1];
};

script_struct_def_t::member_t *FindMember( script_struct_def_t *def, SQString *key );
script_struct_def_t::member_t *FindMember( script_struct_def_t *def,
		const SQChar *keyptr, SQInteger keylen );

int sqffitype_t::ElementSize()
{
	if ( IsStruct() )
	{
		script_struct_def_t *def = StructDef();
		return def->ffitype.size;
	}

	int size = FFITypeByteSize( id );
	Assert( size >= 0 );
	return size;
}

int sqffitype_t::Alignment()
{
	if ( IsStruct() )
	{
		script_struct_def_t *def = StructDef();
		return def->ffitype.alignment;
	}

	int size = FFITypeByteSize( id );
	Assert( size >= 0 );
	return size;
}

struct script_struct_t
{
	void *addr;
	SQObjectPtr def;

	script_struct_def_t *StructDef()
	{
		Assert( sq_type(def) == OT_USERDATA && _userdata(def) );
		return (script_struct_def_t*)_userdataval(def);
	}
};

struct script_array_t
{
	void *addr;
	sqffitype_t type;
};

#ifdef FFI_CLOSURES
struct script_closure_t
{
	tramp_t tramp;
	SQObjectPtr proc;
	SQObjectPtr thread;
};
#endif

const char *FFITypeStr( int type )
{
	Assert( type >= FFIType_void && type < FFIType_END );

	if ( type == FFIType_void )
		return "void";
	if ( type == FFIType_i8 )
		return "i8";
	if ( type == FFIType_u8 )
		return "u8";
	if ( type == FFIType_i16 )
		return "i16";
	if ( type == FFIType_u16 )
		return "u16";
	if ( type == FFIType_i32 )
		return "i32";
	if ( type == FFIType_u32 )
		return "u32";
	if ( type == FFIType_i64 )
		return "i64";
	if ( type == FFIType_u64 )
		return "u64";
	if ( type == FFIType_f32 )
		return "f32";
	if ( type == FFIType_f64 )
		return "f64";
	if ( type == FFIType_str )
		return "str";
	if ( type == FFIType_wstr )
		return "wstr";
	if ( type == FFIType_var )
		return "var";
	if ( type & FFIType_ptr )
		return "ptr";
	if ( type & FFIType_fn )
		return "func";
	if ( type & FFIType_struct )
		return "struct";

	Assert(!"UNREACHABLE");
	return NULL;
}

int ABI_Map( int abi )
{
	if ( abi == FFI_DEFAULT_ABI )
		return 1;
#if defined(_WIN32) && !defined(_WIN64)
	// is default, unreachable, is ok
	if ( abi == FFI_MS_CDECL )
		return 2;
	if ( abi == FFI_STDCALL )
		return 4;
	if ( abi == FFI_FASTCALL )
		return 8;
	if ( abi == FFI_THISCALL )
		return 16;
#endif
	return -1;
}

int ABI_Get( int abi )
{
	switch ( abi )
	{
		case 1: return FFI_DEFAULT_ABI;
#if defined(_WIN32) && !defined(_WIN64)
		case 2: return FFI_MS_CDECL;
		case 4: return FFI_STDCALL;
		case 8: return FFI_FASTCALL;
		case 16: return FFI_THISCALL;
#endif
		default: return -1;
	}
}

ffi_type *MapFFIType( sqffitype_t &type )
{
	Assert( type.id >= FFIType_void && type.id < FFIType_END );

	if ( type.id & ( FFIType_ptr | FFIType_fn ) )
		return &ffi_type_pointer;
	if ( type.id & FFIType_struct )
		return &type.StructDef()->ffitype;
	if ( type.id == FFIType_void )
		return &ffi_type_void;
	if ( type.id == FFIType_i8 )
		return &ffi_type_sint8;
	if ( type.id == FFIType_u8 )
		return &ffi_type_uint8;
	if ( type.id == FFIType_i16 )
		return &ffi_type_sint16;
	if ( type.id == FFIType_u16 )
		return &ffi_type_uint16;
	if ( type.id == FFIType_i32 )
		return &ffi_type_sint32;
	if ( type.id == FFIType_u32 )
		return &ffi_type_uint32;
	if ( type.id == FFIType_i64 )
		return &ffi_type_sint64;
	if ( type.id == FFIType_u64 )
		return &ffi_type_uint64;
	if ( type.id == FFIType_f32 )
		return &ffi_type_float;
	if ( type.id == FFIType_f64 )
		return &ffi_type_double;

	UNREACHABLE();
}

int MapFFIType( ffi_type *type )
{
	if ( type == &ffi_type_pointer )
		return FFIType_ptr;
	if ( type == &ffi_type_sint8 )
		return FFIType_i8;
	if ( type == &ffi_type_uint8 )
		return FFIType_u8;
	if ( type == &ffi_type_sint16 )
		return FFIType_i16;
	if ( type == &ffi_type_uint16 )
		return FFIType_u16;
	if ( type == &ffi_type_sint32 )
		return FFIType_i32;
	if ( type == &ffi_type_uint32 )
		return FFIType_u32;
	if ( type == &ffi_type_sint64 )
		return FFIType_i64;
	if ( type == &ffi_type_uint64 )
		return FFIType_u64;
	if ( type == &ffi_type_float )
		return FFIType_f32;
	if ( type == &ffi_type_double )
		return FFIType_f64;
	if ( type == &ffi_type_void )
		return FFIType_void;

	UNREACHABLE();
}

template < typename T > int CIntToFFI()
{
#define IS_UNSIGNED( I ) ((I)0 < (I)-1)
	if ( sizeof(T) == 1 )
		return IS_UNSIGNED(T) ? FFIType_u8 : FFIType_i8;

	if ( sizeof(T) == 2 )
		return IS_UNSIGNED(T) ? FFIType_u16 : FFIType_i16;

	if ( sizeof(T) == 4 )
		return IS_UNSIGNED(T) ? FFIType_u32 : FFIType_i32;

	if ( sizeof(T) == 8 )
		return IS_UNSIGNED(T) ? FFIType_u64 : FFIType_i64;
#undef IS_UNSIGNED
}

int FFITypeByteSize( int type )
{
	Assert( type >= FFIType_void && type < FFIType_END );

	if ( type & ( FFIType_ptr | FFIType_fn ) )
		return sizeof(void*);

	if ( type == FFIType_void )
		return 0;

	if ( type == FFIType_i8 || type == FFIType_u8 )
		return 1;

	if ( type == FFIType_i16 || type == FFIType_u16 )
		return 2;

	if ( type == FFIType_i32 || type == FFIType_u32 || type == FFIType_f32 )
		return 4;

	if ( type == FFIType_i64 || type == FFIType_u64 || type == FFIType_f64 || type == FFIType_var )
		return 8;

	return -1;
}

void SetDelegateFFIMT( HSQUIRRELVM vm, int idx )
{
	STACKCHECK( vm );
	sq_pushregistrytable( vm );
	sq_pushstring( vm, _SC("ffi"), STRLEN("ffi") );
	sq_get( vm, -2 );
#ifdef _DEBUG
	HSQOBJECT o;
	sq_getstackobj( vm, -1, &o );
	Assert( sq_type(o) == OT_TABLE );
#endif
	sq_setdelegate( vm, idx - 2 );
#ifdef _DEBUG
	HSQOBJECT v;
	sq_getstackobj( vm, idx - 1, &v );
	Assert( sq_type(v) == OT_USERDATA && _userdata(v)->_delegate == _table(o) );
#endif
	sq_pop( vm, 1 );
}

script_array_t *PushArray( HSQUIRRELVM vm, int size )
{
	script_array_t *ptr = (script_array_t*)sq_newuserdata( vm, sizeof(*ptr) + size );
	memset( (void*)ptr, 0, sizeof(*ptr) + size );
	sq_setreleasehook( vm, -1, ffi_ReleaseArray );
	sq_settypetag( vm, -1, TAG_ARRAY );
	SetDelegateFFIMT( vm, -1 );
	return ptr;
}

#ifdef FFI_CLOSURES
script_closure_t *PushClosure( HSQUIRRELVM vm, int size )
{
	script_closure_t *ptr = (script_closure_t*)sq_newuserdata( vm, sizeof(*ptr) + size );
	memset( (void*)ptr, 0, sizeof(*ptr) + size );
	sq_setreleasehook( vm, -1, ffi_ReleaseClosure );
	sq_settypetag( vm, -1, TAG_CLOSURE );
	SetDelegateFFIMT( vm, -1 );
	return ptr;
}

script_closure_t *PushClosureGlobal( HSQUIRRELVM vm, int size )
{
	STACKCHECK( vm );
	HSQOBJECT reg;

	sq_pushregistrytable( vm );
	sq_pushstring( vm, _SC("ffi_closures"), STRLEN("ffi_closures") );
	sq_get( vm, -2 );
	sq_getstackobj( vm, -1, &reg );
	Assert( sq_type(reg) == OT_ARRAY );

	script_closure_t *ptr = PushClosure( vm, size );
	sq_arrayappend( vm, -2 );
	sq_pop( vm, 2 );
	return ptr;
}

void RemoveClosureGlobal( HSQUIRRELVM vm, script_closure_t *ptr )
{
	STACKCHECK( vm );
	HSQOBJECT reg;

	sq_pushregistrytable( vm );
	sq_pushstring( vm, _SC("ffi_closures"), STRLEN("ffi_closures") );
	sq_get( vm, -2 );
	sq_getstackobj( vm, -1, &reg );
	Assert( sq_type(reg) == OT_ARRAY );

	for ( int i = 0; i < _array(reg)->Size(); i++ )
	{
		SQObjectPtr elem;
		_array(reg)->Get( i, elem );

#if SQUIRREL_VERSION_NUMBER >= 222
		Assert( sq_type(elem) == OT_USERDATA );
#else
		if ( sq_type(elem) != OT_USERDATA )
			continue;
#endif

		if ( (void*)_userdataval(elem) == ptr )
		{
#if SQUIRREL_VERSION_NUMBER >= 222
			sq_arrayremove( vm, -1, i );
#else
			_array(reg)->Set( i, SQObjectPtr() );
#endif
			break;
		}
	}

	sq_pop( vm, 2 );
}
#endif

script_struct_t *PushStruct( HSQUIRRELVM vm, int size )
{
	script_struct_t *ptr = (script_struct_t*)sq_newuserdata( vm, sizeof(*ptr) + size );
	memset( (void*)ptr, 0, sizeof(*ptr) + size );
	sq_setreleasehook( vm, -1, ffi_ReleaseStruct );
	sq_settypetag( vm, -1, TAG_STRUCT );
	SetDelegateFFIMT( vm, -1 );
	return ptr;
}

script_struct_def_t::member_t *FindMember( script_struct_def_t *def, SQString *key )
{
	for ( int i = 0; i < def->membercount; i++ )
	{
		script_struct_def_t::member_t *member = &def->members[i];
		if ( key == _string(member->key) )
			return member;
	}

	return NULL;
}

script_struct_def_t::member_t *FindMember( script_struct_def_t *def,
		const SQChar *keyptr, SQInteger keylen )
{
	for ( int i = 0; i < def->membercount; i++ )
	{
		script_struct_def_t::member_t *member = &def->members[i];
		if ( sq_type(member->key) == OT_STRING &&
				_string(member->key)->_len == keylen &&
				!memcmp( _string(member->key)->_val, keyptr, keylen * sizeof(SQChar) ) )
			return member;
	}

	return NULL;
}

bool FindStructDef( HSQUIRRELVM vm, const SQChar *name, int namelen, SQObjectPtr &out )
{
	Assert( namelen > 0 );

	STACKCHECK( vm );
	HSQOBJECT reg;
	bool ret = false;

	sq_pushregistrytable( vm );
	sq_pushstring( vm, _SC("ffi_structs"), STRLEN("ffi_structs") );
	sq_get( vm, -2 );
	sq_getstackobj( vm, -1, &reg );
	Assert( sq_type(reg) == OT_ARRAY );

	for ( int i = 0; i < _array(reg)->Size(); i++ )
	{
		SQObjectPtr elem;
		_array(reg)->Get( i, elem );

#if SQUIRREL_VERSION_NUMBER >= 222
		Assert( sq_type(elem) == OT_USERDATA );
#else
		if ( sq_type(elem) != OT_USERDATA )
			continue;
#endif

		script_struct_def_t *def = (script_struct_def_t*)_userdataval(elem);

		if ( _string(def->name)->_len == namelen &&
				_string(def->name)->_val[0] != '$' &&
				!memcmp( _string(def->name)->_val, name, namelen * sizeof(SQChar) ) )
		{
			out = elem;
			ret = true;
			break;
		}
	}

	sq_pop( vm, 2 );
	return ret;
}

void RemoveStructDef( HSQUIRRELVM vm, const HSQOBJECT &obj )
{
	STACKCHECK( vm );
	HSQOBJECT reg;

	sq_pushregistrytable( vm );
	sq_pushstring( vm, _SC("ffi_structs"), STRLEN("ffi_structs") );
	sq_get( vm, -2 );
	sq_getstackobj( vm, -1, &reg );
	Assert( sq_type(reg) == OT_ARRAY );

	for ( int i = 0; i < _array(reg)->Size(); i++ )
	{
		SQObjectPtr elem;
		_array(reg)->Get( i, elem );

#if SQUIRREL_VERSION_NUMBER >= 222
		Assert( sq_type(elem) == OT_USERDATA );
#else
		if ( sq_type(elem) != OT_USERDATA )
			continue;
#endif

		if ( _userdata(elem) == _userdata(obj) )
		{
#if SQUIRREL_VERSION_NUMBER >= 222
			sq_arrayremove( vm, -1, i );
#else
			_array(reg)->Set( i, SQObjectPtr() );
#endif
			break;
		}
	}

	sq_pop( vm, 2 );
}

script_struct_def_t *PushStructDef( HSQUIRRELVM vm, int size, SQObjectPtr &out )
{
	STACKCHECK( vm );
	script_struct_def_t *def = (script_struct_def_t*)sq_newuserdata( vm, sizeof(*def) + size );
	memset( (void*)def, 0, sizeof(*def) + size );
	sq_setreleasehook( vm, -1, ffi_ReleaseStructDef );
	sq_settypetag( vm, -1, TAG_STRUCT_DEF );
	SetDelegateFFIMT( vm, -1 );

	HSQOBJECT tmp;
	sq_getstackobj( vm, -1, &tmp );
	out = tmp;

	sq_pop( vm, 1 );
	return def;
}

script_struct_def_t *PushStructDefGlobal( HSQUIRRELVM vm, int size, SQObjectPtr &out )
{
	STACKCHECK( vm );
	HSQOBJECT reg;

	if ( g_bCreateFunc )
	{
		sq_pushregistrytable( vm );
		sq_pushstring( vm, _SC("ffi_structs"), STRLEN("ffi_structs") );
		sq_get( vm, -2 );
		sq_getstackobj( vm, -1, &reg );
		Assert( sq_type(reg) == OT_ARRAY );
	}

	script_struct_def_t *def = PushStructDef( vm, sizeof(*def) + size, out );

	if ( g_bCreateFunc )
	{
		sq_pushobject( vm, out );
		sq_arrayappend( vm, -2 );
		sq_pop( vm, 2 );
	}

	return def;
}

SQInteger ffi_ReleaseLibrary( SQUserPointer pModule, SQInteger )
{
	void *module = *(void**)pModule;
#ifdef _WIN32
	FreeLibrary( (HMODULE)module );
#else
	dlclose( module );
#endif
	return 0;
}

SQInteger ffi_ReleaseProc( SQUserPointer pProc, SQInteger )
{
	script_proc_t *proc = (script_proc_t*)pProc;
	proc->module.Null();

	for ( int i = 0; i <= proc->nfixedargs; i++ )
	{
		sqffitype_t &type = proc->argtypes[i];
		type.Release();
	}

	return 0;
}

SQInteger ffi_ReleaseStruct( SQUserPointer pStruct, SQInteger )
{
	script_struct_t *ptr = (script_struct_t*)pStruct;
	Assert( sq_type(ptr->def) == OT_USERDATA );
	ptr->def.Null();
	return 0;
}

SQInteger ffi_ReleaseStructDef( SQUserPointer pDef, SQInteger )
{
	script_struct_def_t *def = (script_struct_def_t*)pDef;

	for ( int i = 0; i < def->membercount; i++ )
	{
		script_struct_def_t::member_t *member = &def->members[i];
		member->key.Null();
		member->type.Release();
	}

	def->name.Null();
	return 0;
}

SQInteger ffi_ReleaseArray( SQUserPointer pArray, SQInteger )
{
	script_array_t *ptr = (script_array_t*)pArray;
	ptr->type.Release();
	return 0;
}

#ifdef FFI_CLOSURES
SQInteger ffi_ReleaseClosure( SQUserPointer pClo, SQInteger )
{
	script_closure_t *ptr = (script_closure_t*)pClo;
	if ( ptr->tramp )
		ptr->tramp.Release();
	ptr->proc.Null();
	ptr->thread.Null();
	return 0;
}
#endif

SQInteger ffi_typeof( HSQUIRRELVM vm )
{
	const SQChar *signature = NULL;
	parser_args_t args = {};
	sq_getstring( vm, -1, &signature );

	Parser_Init( signature );
	g_bCreateFunc = false;
	Parser_ReadType( vm, &signature, &args );
	g_bCreateFunc = true;

	if ( args.type.id == -1 )
		return SQ_ERROR;

	Parser_Skip( &signature, ';' );

	if ( *signature != 0 )
		return throwerrorf( vm, _SC("expected end of string, got '%c'"), *signature );

	args.type.Normalise();
	sq_pushinteger( vm, args.type.id );
	return 1;
}

SQInteger ffi_sizeof( HSQUIRRELVM vm )
{
	HSQOBJECT o;
	sq_getstackobj( vm, -1, &o );
	SQInteger size;

	if ( sq_type(o) == OT_INTEGER )
	{
		size = FFITypeByteSize( _integer(o) );
	}
	else if ( sq_type(o) == OT_USERDATA )
	{
		if ( _userdata(o)->_typetag == TAG_STRUCT )
		{
			script_struct_t *ptr = (script_struct_t*)_userdataval(o);
			size = ptr->StructDef()->ffitype.size;
		}
		else if ( _userdata(o)->_typetag == TAG_ARRAY )
		{
			script_array_t *ptr = (script_array_t*)_userdataval(o);
			size = ptr->type.ByteSize();
		}
		else if ( _userdata(o)->_typetag == TAG_MEMORY )
		{
			size = _userdata(o)->_size;
		}
		else
		{
			return sq_throwerror( vm, _SC("invalid input") );
		}
	}
	else if ( sq_type(o) == OT_STRING )
	{
		const SQChar *signature = _string(o)->_val;
		parser_args_t args = {};

		Parser_Init( signature );
		g_bCreateFunc = false;
		Parser_ReadType( vm, &signature, &args );
		g_bCreateFunc = true;

		if ( args.type.id == -1 )
			return SQ_ERROR;

		Parser_Skip( &signature, ';' );

		if ( *signature != 0 )
			return throwerrorf( vm, _SC("expected end of string, got '%c'"), *signature );

		args.type.Normalise();
		size = args.type.ByteSize();
	}
	else UNREACHABLE();

	sq_pushinteger( vm, size );
	return 1;
}

SQInteger ffi_alignof( HSQUIRRELVM vm )
{
	HSQOBJECT o;
	sq_getstackobj( vm, -1, &o );
	SQInteger align;

	if ( sq_type(o) == OT_INTEGER )
	{
		align = FFITypeByteSize( _integer(o) );
	}
	else if ( sq_type(o) == OT_USERDATA )
	{
		if ( _userdata(o)->_typetag == TAG_STRUCT )
		{
			script_struct_t *ptr = (script_struct_t*)_userdataval(o);
			align = ptr->StructDef()->ffitype.alignment;
		}
		else if ( _userdata(o)->_typetag == TAG_ARRAY )
		{
			script_array_t *ptr = (script_array_t*)_userdataval(o);
			align = ptr->type.ElementSize();
		}
		else
		{
			return sq_throwerror( vm, _SC("invalid input") );
		}
	}
	else if ( sq_type(o) == OT_STRING )
	{
		const SQChar *signature = _string(o)->_val;
		parser_args_t args = {};

		Parser_Init( signature );
		g_bCreateFunc = false;
		Parser_ReadType( vm, &signature, &args );
		g_bCreateFunc = true;

		if ( args.type.id == -1 )
			return SQ_ERROR;

		Parser_Skip( &signature, ';' );

		if ( *signature != 0 )
			return throwerrorf( vm, _SC("expected end of string, got '%c'"), *signature );

		args.type.Normalise();
		align = args.type.Alignment();
	}
	else UNREACHABLE();

	sq_pushinteger( vm, align );
	return 1;
}

SQInteger ffi_offsetof( HSQUIRRELVM vm )
{
	HSQOBJECT o, key;
	SQInteger value;
	sq_getstackobj( vm, 2, &o );
	sq_getstackobj( vm, 3, &key );

	if ( sq_type(o) == OT_USERDATA )
	{
		if ( _userdata(o)->_typetag == TAG_STRUCT )
		{
			script_struct_t *ptr = (script_struct_t*)_userdataval(o);

			if ( sq_gettop( vm ) == 3 )
			{
				if ( sq_type(key) != OT_STRING )
					return sq_throwerror( vm, _SC("invalid struct index") );

				const script_struct_def_t::member_t *member = FindMember( ptr->StructDef(), _string(key) );

				if ( !member )
					return throwerrorf( vm, _SC("index '" FMT_STR50 "' not found"), _string(key)->_val );

				value = member->offset;
			}
			else
			{
				STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );
				value = (SQInteger)ptr->addr;
			}
		}
		else if ( _userdata(o)->_typetag == TAG_ARRAY )
		{
			script_array_t *ptr = (script_array_t*)_userdataval(o);

			if ( sq_gettop( vm ) == 3 )
			{
				if ( sq_type(key) != OT_INTEGER )
					return sq_throwerror( vm, _SC("invalid array index") );

				value = _integer(key) * ptr->type.ElementSize();
			}
			else
			{
				STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );
				value = (SQInteger)ptr->addr;
			}
		}
		else
		{
			return sq_throwerror( vm, _SC("invalid input") );
		}
	}
	else if ( sq_type(o) == OT_STRING )
	{
		if ( sq_gettop( vm ) != 3 )
			return sq_throwerror( vm, _SC("expected struct index") );

		const SQChar *signature = _string(o)->_val;
		parser_args_t args = {};

		Parser_Init( signature );
		g_bCreateFunc = false;
		Parser_ReadType( vm, &signature, &args );
		g_bCreateFunc = true;

		if ( args.type.id == -1 )
			return SQ_ERROR;

		Parser_Skip( &signature, ';' );

		if ( *signature != 0 )
			return throwerrorf( vm, _SC("expected end of string, got '%c'"), *signature );

		if ( !args.type.IsStruct() )
			return sq_throwerror( vm, _SC("expected struct") );

		if ( sq_type(key) != OT_STRING )
			return sq_throwerror( vm, _SC("invalid struct index") );

		const script_struct_def_t::member_t *member = FindMember( args.type.StructDef(), _string(key) );

		if ( !member )
			return throwerrorf( vm, _SC("index '" FMT_STR50 "' not found"), _string(key)->_val );

		value = member->offset;
	}
	else UNREACHABLE();

	sq_pushinteger( vm, value );
	return 1;
}

SQInteger ffi_typedef( HSQUIRRELVM vm )
{
	const SQChar *signature = NULL;
	sq_getstring( vm, 2, &signature );

	Parser_Init( signature );
	Parser_SkipWhitespace( &signature );

	for (;;)
	{
		if ( *signature == 0 )
			break;

		int res = Parser_Typedef2( vm, &signature );

		if ( res == -1 )
			return SQ_ERROR;

		if ( res == 1 )
		{
			fileindex_t pidx = Parser_GetIndex( signature );
			return throwerrorf( vm, _SC("expected typedef @L%d:%d"), pidx.line, pidx.col );
		}
	}

	return 0;
}

SQInteger ffi_LoadLibrary( HSQUIRRELVM vm )
{
	const SQChar *name = NULL;
#if ( !defined(_WIN32) && defined(SQUNICODE) ) || defined(SQDBG_NATIVE_STACKTRACE)
	SQInteger namelen;
	sq_getstringandsize( vm, -1, &name, &namelen );
#else
	sq_getstring( vm, -1, &name );
#endif

#if ( !defined(_WIN32) && defined(SQUNICODE) ) || defined(SQDBG_NATIVE_STACKTRACE)
#ifdef SQUNICODE
	char libname[512];

	if ( namelen > (int)sizeof(libname) - 1 )
		return sq_throwerror( vm, _SC("name is too long") );

	WCharToUTF8( libname, sizeof(libname), name, namelen );
	libname[namelen] = 0;
#else
	const char *libname = name;
#endif
#endif

#ifdef _WIN32
	#ifdef SQUNICODE
		void *module = LoadLibraryW( name );
	#else
		void *module = LoadLibraryA( name );
	#endif
#else
	#ifdef SQUNICODE
		void *module = dlopen( libname, RTLD_NOW );
	#else
		void *module = dlopen( name, RTLD_NOW );
	#endif
#endif

	if ( !module )
	{
#ifdef _WIN32
		Errorf( "could not load module '" FMT_STR "'", name );
		PrintWin32Error( ": %s\n", GetLastError() );
#else
		Errorf( "%s\n", dlerror() );
#endif
		return 0;
	}

#ifdef SQDBG_NATIVE_STACKTRACE
	LoadModuleSymbols( g_hProcess, libname );
#endif

	void **ptr = (void**)sq_newuserdata( vm, sizeof(void*) );
	sq_setreleasehook( vm, -1, ffi_ReleaseLibrary );
	sq_settypetag( vm, -1, TAG_LIBRARY );

	*ptr = module;
	return 1;
}

SQInteger ffi_malloc( HSQUIRRELVM vm )
{
	HSQOBJECT arg;
	sq_getstackobj( vm, -1, &arg );

	if ( sq_type(arg) == OT_INTEGER )
	{
		SQInteger size = _integer(arg);

		if ( size <= 0 || size > INT_MAX )
			return sq_throwerror( vm, _SC("invalid size") );

		void *ptr = sq_newuserdata( vm, size );
		sq_settypetag( vm, -1, TAG_MEMORY );
		SetDelegateFFIMT( vm, -1 );
		memset( ptr, 0, size );
		Assert( ( (uintptr_t)ptr % sizeof(void*) ) == 0 );
		return 1;
	}
	else if ( sq_type(arg) == OT_STRING )
	{
		const SQChar *signature = _string(arg)->_val;
		parser_args_t args = {};

		Parser_Init( signature );
		Parser_SkipWhitespace( &signature );

		int align = 0;
		args.name = Parser_PeekIdentifier( signature, &args.namelen );

		if ( STRCMP( args.name, args.namelen, _SC("alignas") ) )
		{
			signature += STRLEN("alignas");
			align = Parser_ReadAlignas( vm, &signature );

			if ( align == -1 )
				return SQ_ERROR;
		}

		Parser_ReadType( vm, &signature, &args );

		if ( args.type.id == -1 )
			return SQ_ERROR;

		Parser_Skip( &signature, ';' );

		if ( *signature != 0 )
			return throwerrorf( vm, _SC("expected end of string, got '%c'"), *signature );

		args.type.Normalise();
		int size = args.type.ByteSize();

		if ( !align )
			align = args.type.Alignment();

		Assert( ( align & ( align - 1 ) ) == 0 );

		if ( args.type.isarray )
		{
			script_array_t *ptr = PushArray( vm, size + ( align <= (int)sizeof(void*) ? 0 : align - 1 ) );
			ptr->addr = (void*)ALIGN( (uintptr_t)( ptr + 1 ), align );
			ptr->type = args.type;
			Assert( ( (uintptr_t)ptr->addr % align ) == 0 );
			return 1;
		}

		Assert( args.type.count == 1 );

		if ( args.type.IsStruct() )
		{
			script_struct_t *ptr = PushStruct( vm, size + ( align <= (int)sizeof(void*) ? 0 : align - 1 ) );
			ptr->addr = (void*)ALIGN( (uintptr_t)( ptr + 1 ), align );
			ptr->def = args.type.obj;
			Assert( ( (uintptr_t)ptr->addr % align ) == 0 );
			return 1;
		}
		else
		{
			void *ptr = sq_newuserdata( vm, size );
			sq_settypetag( vm, -1, TAG_MEMORY );
			SetDelegateFFIMT( vm, -1 );
			memset( ptr, 0, size );
			Assert( ( (uintptr_t)ptr % sizeof(void*) ) == 0 );
			return 1;
		}
	}
	else UNREACHABLE();
}

SQInteger ffi_MT_ToString( HSQUIRRELVM vm )
{
	HSQOBJECT o;
	sq_getstackobj( vm, -1, &o );
	void *p = _userdataval(o);
	SQUserPointer tag = _userdata(o)->_typetag;

	SQChar buf[256];
	int len;

	if ( tag == TAG_STRUCT )
	{
		script_struct_t *ptr = (script_struct_t*)p;
		len = scsprintf( buf, ARRAYSIZE(buf),
				_SC("(ffi struct " FMT_STR ": 0x" FMT_PTR ")"),
				_string(ptr->StructDef()->name)->_val,
				(uintptr_t)ptr->addr );
	}
	else if ( tag == TAG_ARRAY )
	{
		script_array_t *ptr = (script_array_t*)p;
		len = scsprintf( buf, ARRAYSIZE(buf),
				_SC("(ffi array " FMT_CSTR "[%d]: 0x" FMT_PTR ")"),
				ptr->type.Str(),
				ptr->type.count,
				(uintptr_t)ptr->addr );
	}
	else if ( tag == TAG_MEMORY )
	{
		len = scsprintf( buf, ARRAYSIZE(buf),
				_SC("(ffi memory [%d]: 0x" FMT_PTR ")"),
				(int)_userdata(o)->_size,
				(uintptr_t)p );
	}
	else if ( tag == TAG_PROC )
	{
		script_proc_t *ptr = (script_proc_t*)p;
		len = scsprintf( buf, ARRAYSIZE(buf),
				_SC("(ffi proc: 0x" FMT_PTR ")"),
				(uintptr_t)ptr->func );
	}
#ifdef FFI_CLOSURES
	else if ( tag == TAG_CLOSURE )
	{
		script_closure_t *ptr = (script_closure_t*)p;
		len = scsprintf( buf, ARRAYSIZE(buf),
				_SC("(ffi closure: 0x" FMT_PTR ")"),
				(uintptr_t)_refcounted(ptr->tramp->closure) );
	}
#endif
#ifdef _DEBUG
	else if ( tag == TAG_STRUCT_DEF )
	{
		script_struct_def_t *ptr = (script_struct_def_t*)p;
		len = scsprintf( buf, ARRAYSIZE(buf),
				_SC("(ffi structdef " FMT_STR ")"),
				_string(ptr->name)->_val );
	}
#endif
	else
	{
		len = scsprintf( buf, ARRAYSIZE(buf),
				_SC("(userdata: 0x" FMT_PTR ")"),
				(uintptr_t)p );
	}

	if ( len < 0 || len > ARRAYSIZE(buf) )
		len = ARRAYSIZE(buf);

	sq_pushstring( vm, buf, len );
	return 1;
}

SQInteger ffi_MT_Add( HSQUIRRELVM vm )
{
	void *ptr;
	SQUserPointer tag;
	SQInteger rhs;

	if ( SQ_FAILED( sq_getuserdata( vm, 1, (SQUserPointer*)&ptr, &tag ) ) )
		return sq_throwerror( vm, _SC("invalid input") );

	sq_getinteger( vm, 2, &rhs );

	if ( tag == TAG_MEMORY )
	{
		STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );

		ptr = (char*)ptr + rhs;
		sq_pushinteger( vm, (SQInteger)ptr );
	}
	else if ( tag == TAG_ARRAY )
	{
		script_array_t *arr = (script_array_t*)ptr;

		if ( arr->type.count - rhs < 0 )
			return throwerrorf( vm, _SC("index '" FMT_INT "' is out of bounds"), rhs );

		script_array_t *ret = PushArray( vm, 0 );
		ret->addr = (char*)arr->addr + (intptr_t)rhs * (intptr_t)arr->type.ElementSize();
		ret->type = arr->type;
		ret->type.count -= rhs;
	}
	else
	{
		return sq_throwerror( vm, _SC("invalid input") );
	}

	return 1;
}

SQInteger ffi_MT_Sub( HSQUIRRELVM vm )
{
	void *ptr;
	SQUserPointer tag;
	SQInteger rhs;

	if ( SQ_FAILED( sq_getuserdata( vm, 1, (SQUserPointer*)&ptr, &tag ) ) )
		return sq_throwerror( vm, _SC("invalid input") );

	sq_getinteger( vm, 2, &rhs );

	if ( tag == TAG_MEMORY )
	{
		STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );

		ptr = (char*)ptr - rhs;
		sq_pushinteger( vm, (SQInteger)ptr );
	}
	else if ( tag == TAG_ARRAY )
	{
		script_array_t *arr = (script_array_t*)ptr;

		// No back bound check
		if ( arr->type.count + rhs < 0 )
			return throwerrorf( vm, _SC("index '" FMT_INT "' is out of bounds"), rhs );

		script_array_t *ret = PushArray( vm, 0 );
		ret->addr = (char*)arr->addr - (intptr_t)rhs * (intptr_t)arr->type.ElementSize();
		ret->type = arr->type;
		ret->type.count += rhs;
	}
	else
	{
		return sq_throwerror( vm, _SC("invalid input") );
	}

	return 1;
}

SQInteger ffi_MT_Get( HSQUIRRELVM vm )
{
	void *pptr;
	SQUserPointer tag;
	HSQOBJECT key;
	char *addr = NULL;
	sqffitype_t *type;

	if ( SQ_FAILED( sq_getuserdata( vm, 1, (SQUserPointer*)&pptr, &tag ) ) )
		return sq_throwerror( vm, _SC("invalid input") );

	sq_getstackobj( vm, 2, &key );

	if ( tag == TAG_STRUCT )
	{
		if ( sq_type(key) != OT_STRING )
			return sq_throwerror( vm, _SC("invalid struct index") );

		script_struct_t *ptr = (script_struct_t*)pptr;
		script_struct_def_t::member_t *member = FindMember( ptr->StructDef(), _string(key) );

		if ( !member )
			return throwerrorf( vm, _SC("index '" FMT_STR50 "' not found"), _string(key)->_val );

		addr = (char*)ptr->addr + member->offset;
		type = &member->type;
	}
	else if ( tag == TAG_ARRAY )
	{
		script_array_t *ptr = (script_array_t*)pptr;

		if ( sq_type(key) == OT_STRING )
		{
			if ( STRCMP( _string(key)->_val, _string(key)->_len, _SC("ptr") ) )
			{
				STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );
				sq_pushinteger( vm, (SQInteger)ptr->addr );
				return 1;
			}
			else if ( STRCMP( _string(key)->_val, _string(key)->_len, _SC("count") ) )
			{
				sq_pushinteger( vm, (SQInteger)ptr->type.count );
				return 1;
			}
			else if ( STRCMP( _string(key)->_val, _string(key)->_len, _SC("size") ) )
			{
				sq_pushinteger( vm, (SQInteger)ptr->type.ByteSize() );
				return 1;
			}
			else if ( STRCMP( _string(key)->_val, _string(key)->_len, _SC("align") ) )
			{
				sq_pushinteger( vm, (SQInteger)ptr->type.ElementSize() );
				return 1;
			}
		}

		if ( sq_type(key) != OT_INTEGER )
			return sq_throwerror( vm, _SC("invalid array index") );

		if ( _integer(key) < 0 || (int)_integer(key) >= ptr->type.count )
			return throwerrorf( vm, _SC("index '" FMT_INT "' is out of bounds"), _integer(key) );

		addr = (char*)ptr->addr + (intptr_t)_integer(key) * (intptr_t)ptr->type.ElementSize();
		type = (sqffitype_t*)alloca( sizeof(sqffitype_t) );
		memset( (void*)type, 0, sizeof(sqffitype_t) );
		type->id = ptr->type.id;
		// copy without extra ref, no destructor for 'type'
		sq_type(type->obj) = sq_type(ptr->type.obj);
		_refcounted(type->obj) = _refcounted(ptr->type.obj);
	}
	else
	{
		return sq_throwerror( vm, _SC("invalid input") );
	}

	return DoGet( vm, addr, type );
}

SQInteger ffi_MT_Set( HSQUIRRELVM vm )
{
	void *pptr;
	SQUserPointer tag;
	HSQOBJECT key, value;
	char *addr = NULL;
	sqffitype_t *type;

	if ( SQ_FAILED( sq_getuserdata( vm, 1, (SQUserPointer*)&pptr, &tag ) ) )
		return sq_throwerror( vm, _SC("invalid input") );

	sq_getstackobj( vm, 2, &key );
	sq_getstackobj( vm, 3, &value );

	if ( tag == TAG_STRUCT )
	{
		if ( sq_type(key) != OT_STRING )
			return sq_throwerror( vm, _SC("invalid struct index") );

		script_struct_t *ptr = (script_struct_t*)pptr;
		script_struct_def_t::member_t *member = FindMember( ptr->StructDef(), _string(key) );

		if ( !member )
			return throwerrorf( vm, _SC("index '" FMT_STR50 "' not found"), _string(key)->_val );

		if ( member->type.isarray )
			return sq_throwerror( vm, _SC("cannot set array") );

		addr = (char*)ptr->addr + member->offset;
		type = &member->type;
	}
	else if ( tag == TAG_ARRAY )
	{
		script_array_t *ptr = (script_array_t*)pptr;

		if ( sq_type(key) == OT_STRING )
		{
			if ( STRCMP( _string(key)->_val, _string(key)->_len, _SC("ptr") ) )
			{
				Assert( (intptr_t)_integer(value) - (intptr_t)ptr->addr >= INT_MIN &&
						(intptr_t)_integer(value) - (intptr_t)ptr->addr <= INT_MAX );

				int diff = (int)( (intptr_t)_integer(value) - (intptr_t)ptr->addr );
				int align = ptr->type.ElementSize();

				if ( !( diff % align ) )
				{
					diff /= align;

					if ( diff < ptr->type.count )
					{
						*(char**)&ptr->addr += diff * align;
						ptr->type.count -= diff;
						Assert( ptr->type.count >= 0 );
						return 0;
					}
					else
					{
						return sq_throwerror( vm, _SC("address is out of bounds") );
					}
				}
				else
				{
					return throwerrorf( vm, _SC("address 0x" FMT_PTR " is not aligned to %d"),
							(uintptr_t)_integer(value),
							align );
				}
			}
			else if ( STRCMP( _string(key)->_val, _string(key)->_len, _SC("count") ) )
			{
				if ( sq_type(value) == OT_INTEGER && _integer(value) >= 0 )
				{
					ptr->type.count = (int)_integer(value);
					return 0;
				}
			}
		}

		if ( sq_type(key) != OT_INTEGER )
			return sq_throwerror( vm, _SC("invalid array index") );

		if ( _integer(key) < 0 || (int)_integer(key) >= ptr->type.count )
			return throwerrorf( vm, _SC("index '" FMT_INT "' is out of bounds"), _integer(key) );

		addr = (char*)ptr->addr + (intptr_t)_integer(key) * (intptr_t)ptr->type.ElementSize();
		type = (sqffitype_t*)alloca( sizeof(sqffitype_t) );
		memset( (void*)type, 0, sizeof(sqffitype_t) );
		type->id = ptr->type.id;
		// copy without extra ref, no destructor for 'type'
		sq_type(type->obj) = sq_type(ptr->type.obj);
		_refcounted(type->obj) = _refcounted(ptr->type.obj);
	}
	else
	{
		return sq_throwerror( vm, _SC("invalid input") );
	}

	return DoSet( vm, addr, type, value );
}

SQInteger ffi_MT_Call( HSQUIRRELVM vm )
{
	script_proc_t *proc;
	void *pptr;
	SQUserPointer tag;

	if ( SQ_FAILED( sq_getuserdata( vm, 1, (SQUserPointer*)&pptr, &tag ) ) )
		return sq_throwerror( vm, _SC("invalid input") );

	if ( tag == TAG_PROC )
	{
		script_proc_t *ptr = (script_proc_t*)pptr;
		proc = ptr;
	}
#ifdef FFI_CLOSURES
	else if ( tag == TAG_CLOSURE )
	{
		script_closure_t *ptr = (script_closure_t*)pptr;
		proc = ptr->tramp->proc;
	}
#endif
	else
	{
		return sq_throwerror( vm, _SC("invalid input") );
	}

	// 1:ud, 2:env, 3:args...
	return DoCall( vm, 3, sq_gettop( vm ) - 2, proc );
}

// ffi.get( type, addr )
SQInteger ffi_get( HSQUIRRELVM vm )
{
	void *addr;
	parser_args_t args = {};
	HSQOBJECT t, o;
	sq_getstackobj( vm, 2, &t );
	sq_getstackobj( vm, 3, &o );

	if ( sq_type(o) == OT_INTEGER )
	{
		addr = (void*)_integer(o);
	}
	else if ( sq_type(o) == OT_USERPOINTER )
	{
		addr = (void*)_userpointer(o);
	}
	else if ( sq_type(o) == OT_USERDATA )
	{
		if ( _userdata(o)->_typetag == TAG_STRUCT )
		{
			script_struct_t *ptr = (script_struct_t*)_userdataval(o);
			addr = ptr->addr;
		}
		else if ( _userdata(o)->_typetag == TAG_ARRAY )
		{
			script_array_t *ptr = (script_array_t*)_userdataval(o);
			addr = ptr->addr;
		}
		else
		{
			addr = (void*)_userdataval(o);
		}
	}
	else UNREACHABLE();

	if ( sq_type(t) == OT_INTEGER )
	{
		args.type.id = _integer(t);
	}
	else if ( sq_type(t) == OT_STRING )
	{
		const SQChar *signature = _string(t)->_val;

		Parser_Init( signature );
		Parser_ReadType( vm, &signature, &args );

		if ( args.type.id == -1 )
			return SQ_ERROR;

		Parser_Skip( &signature, ';' );

		if ( *signature != 0 )
			return throwerrorf( vm, _SC("expected end of string, got '%c'"), *signature );

		args.type.Normalise();
	}
	else UNREACHABLE();

	return DoGet( vm, addr, &args.type );
}

// ffi.set( type, addr, value )
SQInteger ffi_set( HSQUIRRELVM vm )
{
	void *addr;
	parser_args_t args = {};
	HSQOBJECT t, o, value;
	sq_getstackobj( vm, 2, &t );
	sq_getstackobj( vm, 3, &o );
	sq_getstackobj( vm, 4, &value );

	if ( sq_type(o) == OT_INTEGER )
	{
		addr = (void*)_integer(o);
	}
	else if ( sq_type(o) == OT_USERPOINTER )
	{
		addr = (void*)_userpointer(o);
	}
	else if ( sq_type(o) == OT_USERDATA )
	{
		if ( _userdata(o)->_typetag == TAG_STRUCT )
		{
			script_struct_t *ptr = (script_struct_t*)_userdataval(o);
			addr = ptr->addr;
		}
		else if ( _userdata(o)->_typetag == TAG_ARRAY )
		{
			script_array_t *ptr = (script_array_t*)_userdataval(o);
			addr = ptr->addr;
		}
		else
		{
			addr = (void*)_userdataval(o);
		}
	}
	else UNREACHABLE();

	if ( sq_type(t) == OT_INTEGER )
	{
		args.type.id = _integer(t);
	}
	else if ( sq_type(t) == OT_STRING )
	{
		const SQChar *signature = _string(t)->_val;

		Parser_Init( signature );
		Parser_ReadType( vm, &signature, &args );

		if ( args.type.id == -1 )
			return SQ_ERROR;

		Parser_Skip( &signature, ';' );

		if ( *signature != 0 )
			return throwerrorf( vm, _SC("expected end of string, got '%c'"), *signature );

		if ( args.type.isarray )
			return sq_throwerror( vm, _SC("cannot set array") );

		args.type.Normalise();
	}
	else UNREACHABLE();

	return DoSet( vm, addr, &args.type, value );
}

#if 0
#define Assert_AddrSize(...)
#else
#define Assert_AddrSize( x ) if (!(x)) Error( "Warning: value does not fit address type\n" )
#endif

int DoGet( HSQUIRRELVM vm, void *addr, sqffitype_t *type )
{
#ifndef SQ_DISABLE_BITFIELDS
#define _get( ctype, addr, ftype ) \
	(ftype->bits ? \
	 ((*(ctype*)addr & ftype->Mask<ctype>()) >> ftype->shift) : \
	 *(ctype*)addr)
#else
#define _get( ctype, addr, ftype ) (*(ctype*)addr)
#endif

	int ret = 1;

	if ( type->isarray )
	{
		script_array_t *ptr = PushArray( vm, 0 );
		ptr->addr = addr;
		ptr->type = *type;
		return ret;
	}

	__try
	{
		switch ( type->id )
		{
			case FFIType_i8:
			{
				SQInteger val;
				val = _get( int8_t, addr, type );
				sq_pushinteger( vm, val );
				break;
			}
			case FFIType_u8:
			{
				SQInteger val;
				val = (SQUnsignedInteger)_get( uint8_t, addr, type );
				sq_pushinteger( vm, val );
				break;
			}
			case FFIType_i16:
			{
				SQInteger val;
				val = _get( int16_t, addr, type );
				sq_pushinteger( vm, val );
				break;
			}
			case FFIType_u16:
			{
				SQInteger val;
				val = (SQUnsignedInteger)_get( uint16_t, addr, type );
				sq_pushinteger( vm, val );
				break;
			}
			case FFIType_i32:
			{
				SQInteger val;
				val = _get( int32_t, addr, type );
				sq_pushinteger( vm, val );
				break;
			}
			case FFIType_u32:
			{
				SQInteger val;
				val = (SQUnsignedInteger)_get( uint32_t, addr, type );
				sq_pushinteger( vm, val );
				break;
			}
			case FFIType_i64:
			{
				Assert_AddrSize( sizeof(SQInteger) == 8 ||
						( *(int64_t*)addr >= INT_MIN && *(int64_t*)addr <= UINT_MAX ) );
				SQInteger val;
				val = *(int64_t*)addr;
				sq_pushinteger( vm, val );
				break;
			}
			case FFIType_u64:
			{
				Assert_AddrSize( sizeof(SQInteger) == 8 ||
						( *(int64_t*)addr >= INT_MIN && *(int64_t*)addr <= UINT_MAX ) );
				SQInteger val;
				val = (SQUnsignedInteger)*(uint64_t*)addr;
				sq_pushinteger( vm, val );
				break;
			}
			case FFIType_f32:
			{
				SQFloat val;
				val = (SQFloat)*(float*)addr;
				sq_pushfloat( vm, val );
				break;
			}
			case FFIType_f64:
			{
				SQFloat val;
				val = (SQFloat)*(double*)addr;
				sq_pushfloat( vm, val );
				break;
			}
			case FFIType_ptr:
			{
				STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );
				SQInteger val;
				val = (SQInteger)*(void**)addr;
				sq_pushinteger( vm, val );
				break;
			}
			case FFIType_str:
			{
				char *val = (char*)addr;
				int srclen = strlen( val );
#ifdef SQUNICODE
				int len = UTF8ToWChar( NULL, 0, val, srclen );
				SQChar *dst = sq_getscratchpad( vm, len * sizeof(SQChar) );
				UTF8ToWChar( dst, len * sizeof(SQChar), val, srclen );
				sq_pushstring( vm, dst, len );
#else
				sq_pushstring( vm, val, srclen );
#endif
				break;
			}
			case FFIType_wstr:
			{
				wchar_t *val = (wchar_t*)addr;
				int srclen = wcslen( val );
#ifdef SQUNICODE
				sq_pushstring( vm, val, srclen );
#else
				int len = WCharToUTF8( NULL, 0, val, srclen );
				SQChar *dst = sq_getscratchpad( vm, len );
				WCharToUTF8( dst, len, val, srclen );
				sq_pushstring( vm, dst, len );
#endif
				break;
			}
			case FFIType_struct:
			{
				script_struct_t *ptr = PushStruct( vm, 0 );
				ptr->addr = addr;
				ptr->def = type->obj;
				break;
			}
			case FFIType_fn:
			{
				script_proc_t *proc = type->Proc();
				CloneProc( vm, proc, *(void**)addr );
				break;
			}
			default:
				ret = sq_throwerror( vm, _SC("invalid address type") );
		}

		__try_end();
	}
	__except( SQ_EXCEPTION_HANDLER )
	{
		ret = ThrowSQException( vm );
	}

	return ret;

#undef _get
}

int DoSet( HSQUIRRELVM vm, void *addr, sqffitype_t *type, const HSQOBJECT &value )
{
#ifndef SQ_DISABLE_BITFIELDS
#define _bitfieldcheck( ctype, v, ftype ) \
	Assert_AddrSize( !ftype->bits || \
			(SQUnsignedInteger)(((ctype)((v) << ftype->shift) & ftype->Mask<ctype>()) >> ftype->shift) == \
			(SQUnsignedInteger)(v) )

#define _set( ctype, addr, v, ftype ) \
	*(ctype*)addr = (ctype)(ftype->bits ? \
		(((ctype)((v) << ftype->shift) & ftype->Mask<ctype>()) | (*(ctype*)addr & ~ftype->Mask<ctype>())) : \
		(v))
#else
#define _bitfieldcheck( ctype, v, ftype ) (void)0
#define _set( ctype, addr, v, ftype ) *(ctype*)addr = (ctype)(v)
#endif

	int ret = 0;

	Assert( !type->isarray );

	__try
	{
		switch ( type->id )
		{
			case FFIType_i8:
			{
				if ( sq_type(value) == OT_INTEGER )
				{
					Assert_AddrSize( _integer(value) >= CHAR_MIN && _integer(value) <= UCHAR_MAX );
					_bitfieldcheck( int8_t, _integer(value), type );
					_set( int8_t, addr, _integer(value), type );
					break;
				}

				goto invalid_value_type;
			}
			case FFIType_u8:
			{
				if ( sq_type(value) == OT_INTEGER )
				{
					Assert_AddrSize( _integer(value) >= CHAR_MIN && _integer(value) <= UCHAR_MAX );
					_bitfieldcheck( uint8_t, _integer(value), type );
					_set( uint8_t, addr, _integer(value), type );
					break;
				}

				goto invalid_value_type;
			}
			case FFIType_i16:
			{
				if ( sq_type(value) == OT_INTEGER )
				{
					Assert_AddrSize( _integer(value) >= SHRT_MIN && _integer(value) <= USHRT_MAX );
					_bitfieldcheck( int16_t, _integer(value), type );
					_set( int16_t, addr, _integer(value), type );
					break;
				}

				goto invalid_value_type;
			}
			case FFIType_u16:
			{
				if ( sq_type(value) == OT_INTEGER )
				{
					Assert_AddrSize( _integer(value) >= SHRT_MIN && _integer(value) <= USHRT_MAX );
					_bitfieldcheck( uint16_t, _integer(value), type );
					_set( uint16_t, addr, _integer(value), type );
					break;
				}

				goto invalid_value_type;
			}
			case FFIType_i32:
			{
				if ( sq_type(value) == OT_INTEGER )
				{
					Assert_AddrSize( sizeof(SQInteger) == 4 ||
							( _integer(value) >= INT_MIN && _integer(value) <= UINT_MAX ) );
					_bitfieldcheck( int32_t, _integer(value), type );
					_set( int32_t, addr, _integer(value), type );
					break;
				}

				goto invalid_value_type;
			}
			case FFIType_u32:
			{
				if ( sq_type(value) == OT_INTEGER )
				{
					Assert_AddrSize( sizeof(SQInteger) == 4 ||
							( _integer(value) >= INT_MIN && _integer(value) <= UINT_MAX ) );
					_bitfieldcheck( uint32_t, _integer(value), type );
					_set( uint32_t, addr, _integer(value), type );
					break;
				}

				goto invalid_value_type;
			}
			case FFIType_i64:
			{
				if ( sq_type(value) == OT_INTEGER )
				{
					_bitfieldcheck( int64_t, _integer(value), type );
					_set( int64_t, addr, _integer(value), type );
					break;
				}

				goto invalid_value_type;
			}
			case FFIType_u64:
			{
				if ( sq_type(value) == OT_INTEGER )
				{
					_bitfieldcheck( uint64_t, _integer(value), type );
					_set( uint64_t, addr, _integer(value), type );
					break;
				}

				goto invalid_value_type;
			}
			case FFIType_f32:
			{
				if ( sq_type(value) == OT_FLOAT )
				{
					*(float*)addr = (float)_float(value);
					break;
				}

				goto invalid_value_type;
			}
			case FFIType_f64:
			{
				if ( sq_type(value) == OT_FLOAT )
				{
					*(double*)addr = (double)_float(value);
					break;
				}

				goto invalid_value_type;
			}
			case FFIType_ptr:
			{
				if ( sq_type(value) == OT_INTEGER )
				{
					*(void**)addr = (void*)_integer(value);
					break;
				}
				else if ( sq_type(value) == OT_USERPOINTER )
				{
					*(void**)addr = (void*)_userpointer(value);
					break;
				}
				else if ( sq_type(value) == OT_USERDATA )
				{
					if ( _userdata(value)->_typetag == TAG_STRUCT )
					{
						script_struct_t *ptr = (script_struct_t*)_userdataval(value);
						*(void**)addr = ptr->addr;
					}
					else if ( _userdata(value)->_typetag == TAG_ARRAY )
					{
						script_array_t *ptr = (script_array_t*)_userdataval(value);
						*(void**)addr = ptr->addr;
					}
					else
					{
						*(void**)addr = (void*)_userdataval(value);
					}

					break;
				}

				goto invalid_value_type;
			}
			case FFIType_fn:
			{
				if ( sq_type(value) == OT_INTEGER )
				{
					*(void**)addr = (void*)_integer(value);
					break;
				}
				else if ( sq_type(value) == OT_USERPOINTER )
				{
					*(void**)addr = (void*)_userpointer(value);
					break;
				}
				else if ( sq_type(value) == OT_USERDATA )
				{
					if ( _userdata(value)->_typetag == TAG_PROC )
					{
						script_proc_t *ptr = (script_proc_t*)_userdataval(value);
						*(void**)addr = ptr->func;
					}
#ifdef FFI_CLOSURES
					else if ( _userdata(value)->_typetag == TAG_CLOSURE )
					{
						script_closure_t *ptr = (script_closure_t*)_userdataval(value);
						*(void**)addr = ptr->tramp->code;
					}
#endif

					break;
				}
#ifdef FFI_CLOSURES
				else if ( sq_type(value) == OT_CLOSURE || sq_type(value) == OT_NATIVECLOSURE )
				{
					script_closure_t *ptr = PushClosureGlobal( vm, sizeof(tramp_data_t) );
					ptr->tramp = CreateTrampoline( vm, vm, type->Proc(), value, ptr + 1 );
					ptr->proc = type->obj;

					if ( ptr->tramp )
					{
						*(void**)addr = ptr->tramp->code;
					}
					else
					{
						RemoveClosureGlobal( vm, ptr );
						ret = SQ_ERROR;
					}

					break;
				}
#endif

				goto invalid_value_type;
			}
			case FFIType_struct:
			{
				if ( sq_type(value) == OT_USERDATA && _userdata(value)->_typetag == TAG_STRUCT )
				{
					script_struct_t *ptr = (script_struct_t*)_userdataval(value);
					script_struct_def_t *def = type->StructDef();

					if ( def == ptr->StructDef() )
					{
						memcpy( addr, ptr->addr, def->ffitype.size );
						break;
					}
				}

				goto invalid_value_type;
			}
			default:
				ret = sq_throwerror( vm, _SC("invalid address type") );
				break;
invalid_value_type:
				ret = sq_throwerror( vm, _SC("invalid value for address type") );
		}

		__try_end();
	}
	__except( SQ_EXCEPTION_HANDLER )
	{
		ret = ThrowSQException( vm );
	}

	return ret;

#undef _bitfieldcheck
#undef _set
}

bool DoMakeFunction( HSQUIRRELVM vm, int stackindex, parser_args_t *args )
{
	Assert( args->abi > 0 );

	int nargs = args->nargs;
	sqffitype_t &rettype = g_pParserStack[ stackindex++ ];
	sqffitype_t **argtypes = (sqffitype_t**)alloca( nargs * sizeof(sqffitype_t*) );
	memset( argtypes, 0, nargs * sizeof(sqffitype_t*) );
	int argssize = 0;
	bool variadic = false;

	if ( rettype.id == FFIType_var || rettype.ByteSize() == -1 )
	{
		sq_throwerror( vm, _SC("invalid return type") );
		return false;
	}

	for ( int i = 0; i < nargs; i++ )
	{
		sqffitype_t &t = g_pParserStack[ stackindex + i ];

		if ( t.id == FFIType_var )
		{
			if ( i + 1 != nargs )
			{
				throwerrorf( vm, _SC("invalid parameter %d, variardic must be the last argument"), i );
				return false;
			}

			nargs--;
			variadic = true;
		}

		int size = t.ByteSize();

		if ( size <= 0 )
		{
			throwerrorf( vm, _SC("invalid parameter %d type"), i );
			return false;
		}

		argssize += max( 4, size );
		argtypes[i] = &t;
	}

	int size = sizeof(script_proc_t) +
		( nargs + 1 ) * sizeof(sqffitype_t) +
		( nargs + 1 ) * sizeof(ffi_type*);

	script_proc_t *proc = (script_proc_t*)sq_newuserdata( vm, size );
	sq_setreleasehook( vm, -1, ffi_ReleaseProc );
	sq_settypetag( vm, -1, TAG_PROC );
	SetDelegateFFIMT( vm, -1 );
	memset( (void*)proc, 0, size );

	proc->nfixedargs = variadic ? -nargs : nargs;
	proc->argssize = argssize;
	proc->abi = args->abi;
	proc->flags = args->flags;
	proc->argtypes = (sqffitype_t*)( (char*)proc + sizeof(script_proc_t) );
	proc->fixed_argtypes = (ffi_type**)( (char*)proc->argtypes + ( nargs + 1 ) * sizeof(sqffitype_t) );

	int i = 0;

	for ( ; i < nargs; i++ )
	{
		proc->argtypes[i] = *argtypes[i];
		proc->argtypes[i].Normalise();
	}

	proc->argtypes[nargs] = rettype;
	proc->argtypes[nargs].Normalise();

	i = 0;

	if ( args->has_thisptr )
	{
		proc->fixed_argtypes[i] = &ffi_type_pointer;
		i++;
	}

	for ( ; i < nargs; i++ )
		proc->fixed_argtypes[i] = MapFFIType( *argtypes[i] );

	proc->fixed_argtypes[nargs] = MapFFIType( rettype );

	if ( !variadic )
	{
		if ( ffi_prep_cif( &proc->cif,
					(ffi_abi)args->abi,
					nargs,
					proc->fixed_argtypes[nargs],
					proc->fixed_argtypes ) != FFI_OK )
		{
			sq_throwerror( vm, _SC("FAILED") );
			return false;
		}
	}

	HSQOBJECT o;
	sq_getstackobj( vm, -1, &o );

	Assert( sq_type(args->type.obj) == 0 || sq_type(args->type.obj) == OT_NULL );
	args->type.obj = o;
	Assert( _userdata(args->type.obj)->_uiRef == 2 );
	sq_pop( vm, 1 );
	Assert( _userdata(args->type.obj)->_uiRef == 1 );

	return true;
}

void CloneProc( HSQUIRRELVM vm, script_proc_t *src, void *func )
{
	int nargs = src->nfixedargs;
	if ( nargs < 0 )
		nargs = -nargs;

	int size = sizeof(script_proc_t) +
		( nargs + 1 ) * sizeof(sqffitype_t) +
		( nargs + 1 ) * sizeof(ffi_type*);

	script_proc_t *proc = (script_proc_t*)sq_newuserdata( vm, size );
	sq_setreleasehook( vm, -1, ffi_ReleaseProc );
	sq_settypetag( vm, -1, TAG_PROC );
	SetDelegateFFIMT( vm, -1 );
	memset( (void*)proc, 0, size );

	if ( func == src->func )
	{
		proc->module = src->module;
		proc->thisptr = src->thisptr;
	}

	proc->func = func;
	proc->flags = src->flags;
	proc->cif = src->cif;
	proc->nfixedargs = src->nfixedargs;
	proc->argssize = src->argssize;
	proc->abi = src->abi;
	proc->argtypes = (sqffitype_t*)( (char*)proc + sizeof(script_proc_t) );
	proc->fixed_argtypes = (ffi_type**)( (char*)proc->argtypes + ( nargs + 1 ) * sizeof(sqffitype_t) );

	for ( int i = 0; i <= proc->nfixedargs; i++ )
	{
		proc->argtypes[i] = src->argtypes[i];
		proc->fixed_argtypes[i] = src->fixed_argtypes[i];
	}
}

void InitProc( script_proc_t *proc, void *func, void *thisptr, HSQOBJECT *module )
{
	proc->func = func;
	proc->thisptr = thisptr;
	if ( module )
		proc->module = *module;
}

int DoCall( HSQUIRRELVM vm, int argsBase, int nargs, script_proc_t *proc )
{
	int nfixedargs = proc->nfixedargs;
	int nvarargs = 0;
	bool variadic = false;

	if ( proc->thisptr )
	{
		// nfixedargs includes implicit 'this',
		// account for it in arg check and allocation
		nargs++;
	}

	if ( nfixedargs < 0 )
	{
		nfixedargs = -nfixedargs;
		nvarargs = nargs - nfixedargs;
		variadic = true;
	}

	if ( ( !variadic && nargs != nfixedargs ) ||
			( variadic && nargs < nfixedargs ) )
	{
		return throwerrorf( vm,
				_SC("wrong number of parameters (expected " FMT_STR "%d, got %d)"),
				variadic ? _SC("-") : _SC(""),
				nfixedargs,
				nargs );
	}

	ffi_cif *cif;
	int i = 0;
	int argvecidx = 0;
	sqffitype_t &rettype = proc->argtypes[nfixedargs];
	ffi_type *rtype = proc->fixed_argtypes[nfixedargs];
	void *rvalue = alloca( max( sizeof(ffi_arg), rtype->size ) );
	void **argmap = (void**)alloca( nargs * sizeof(void*) );
	char *argvec = (char*)alloca( proc->argssize + nvarargs * sizeof(double) );

#define _SetArg( ctype, cond, val ) \
	if ( (cond) ) \
	{ \
		argmap[i] = argvec + argvecidx; \
		*(ctype*)( argvec + argvecidx ) = (ctype)val; \
		argvecidx += max( 4, sizeof(ctype) ); \
		break; \
	} (void)0

	if ( proc->thisptr )
	{
		do {
			_SetArg( void*, 1, proc->thisptr );
		} while (0);

		i++;
		argsBase--; // maintain stack index
	}

	for ( ; i < nfixedargs; i++ )
	{
		HSQOBJECT o;
		sq_getstackobj( vm, argsBase + i, &o );

		sqffitype_t &type = proc->argtypes[i];

		switch ( type.id )
		{
			case FFIType_i8:
			{
				_SetArg( int8_t, sq_type(o) == OT_INTEGER || sq_type(o) == OT_BOOL, _integer(o) );
				_SetArg( int8_t, sq_type(o) == OT_FLOAT, _float(o) );
				goto invalid_arg_type;
			}
			case FFIType_u8:
			{
				_SetArg( uint8_t, sq_type(o) == OT_INTEGER || sq_type(o) == OT_BOOL, _integer(o) );
				_SetArg( uint8_t, sq_type(o) == OT_FLOAT, _float(o) );
				goto invalid_arg_type;
			}
			case FFIType_i16:
			{
				_SetArg( int16_t, sq_type(o) == OT_INTEGER, _integer(o) );
				_SetArg( int16_t, sq_type(o) == OT_FLOAT, _float(o) );
				goto invalid_arg_type;
			}
			case FFIType_u16:
			{
				_SetArg( uint16_t, sq_type(o) == OT_INTEGER, _integer(o) );
				_SetArg( uint16_t, sq_type(o) == OT_FLOAT, _float(o) );
				goto invalid_arg_type;
			}
			case FFIType_i32:
			{
				_SetArg( int32_t, sq_type(o) == OT_INTEGER, _integer(o) );
				_SetArg( int32_t, sq_type(o) == OT_FLOAT, _float(o) );
				goto invalid_arg_type;
			}
			case FFIType_u32:
			{
				_SetArg( uint32_t, sq_type(o) == OT_INTEGER, _integer(o) );
				_SetArg( uint32_t, sq_type(o) == OT_FLOAT, _float(o) );
				goto invalid_arg_type;
			}
			case FFIType_i64:
			{
				_SetArg( int64_t, sq_type(o) == OT_INTEGER, _integer(o) );
				_SetArg( int64_t, sq_type(o) == OT_FLOAT, _float(o) );
				goto invalid_arg_type;
			}
			case FFIType_u64:
			{
				_SetArg( uint64_t, sq_type(o) == OT_INTEGER, _integer(o) );
				_SetArg( uint64_t, sq_type(o) == OT_FLOAT, _float(o) );
				goto invalid_arg_type;
			}
			case FFIType_f32:
			{
				_SetArg( float, sq_type(o) == OT_FLOAT, _float(o) );
				_SetArg( float, sq_type(o) == OT_INTEGER, _integer(o) );
				goto invalid_arg_type;
			}
			case FFIType_f64:
			{
				_SetArg( double, sq_type(o) == OT_FLOAT, _float(o) );
				_SetArg( double, sq_type(o) == OT_INTEGER, _integer(o) );
				goto invalid_arg_type;
			}
			// Don't allow OT_STRING for non-matching string type
#ifdef SQUNICODE
			case FFIType_wstr:
#else
			case FFIType_str:
#endif
			{
				_SetArg( void*, sq_type(o) == OT_STRING, _string(o)->_val );
				_SetArg( void*, sq_type(o) == OT_INTEGER, _integer(o) );
				_SetArg( void*, sq_type(o) == OT_USERPOINTER, _userpointer(o) );
				_SetArg( void*,
						sq_type(o) == OT_USERDATA &&
						_userdata(o)->_typetag == TAG_ARRAY &&
						((script_array_t*)_userdataval(o))->type.id == CIntToFFI< SQChar >(),
						((script_array_t*)_userdataval(o))->addr );
				_SetArg( void*, sq_type(o) == OT_USERDATA, _userdataval(o) );
				_SetArg( void*, sq_type(o) == OT_NULL, NULL );
				goto invalid_arg_type;
			}
#ifdef SQUNICODE
			case FFIType_str:
#else
			case FFIType_wstr:
#endif
			{
				_SetArg( void*, sq_type(o) == OT_INTEGER, _integer(o) );
				_SetArg( void*, sq_type(o) == OT_USERPOINTER, _userpointer(o) );
				_SetArg( void*,
						sq_type(o) == OT_USERDATA &&
						_userdata(o)->_typetag == TAG_ARRAY,
						((script_array_t*)_userdataval(o))->addr );
				_SetArg( void*, sq_type(o) == OT_USERDATA, _userdataval(o) );
				_SetArg( void*, sq_type(o) == OT_NULL, NULL );
				if ( sq_type(o) == OT_STRING )
					return throwerrorf( vm, _SC("invalid string parameter %d"), i );
				goto invalid_arg_type;
			}
			case FFIType_ptr:
			{
				_SetArg( void*, sq_type(o) == OT_INTEGER, _integer(o) );
				_SetArg( void*, sq_type(o) == OT_STRING, _string(o)->_val );
				_SetArg( void*, sq_type(o) == OT_USERPOINTER, _userpointer(o) );
				_SetArg( void*,
						sq_type(o) == OT_USERDATA &&
						_userdata(o)->_typetag == TAG_ARRAY,
						((script_array_t*)_userdataval(o))->addr );
				_SetArg( void*, sq_type(o) == OT_USERDATA, _userdataval(o) );
				_SetArg( void*, sq_type(o) == OT_NULL, NULL );
				goto invalid_arg_type;
			}
			case FFIType_struct:
			{
				if ( sq_type(o) == OT_USERDATA && _userdata(o)->_typetag == TAG_STRUCT )
				{
					script_struct_t *ptr = (script_struct_t*)_userdataval(o);
					script_struct_def_t *def = type.StructDef();

					if ( def == ptr->StructDef() )
					{
						argmap[i] = argvec + argvecidx;
						memcpy( argvec + argvecidx, ptr->addr, def->ffitype.size );
						argvecidx += max( 4, def->ffitype.size );
						break;
					}
				}

				goto invalid_arg_type;
			}
			case FFIType_fn:
#ifdef FFI_CLOSURES
			{
				_SetArg( void*, sq_type(o) == OT_NULL, NULL );
				_SetArg( void*,
						sq_type(o) == OT_USERDATA &&
						_userdata(o)->_typetag == TAG_CLOSURE,
						((script_closure_t*)_userdataval(o))->tramp->code );
				if ( sq_type(o) == OT_CLOSURE || sq_type(o) == OT_NATIVECLOSURE )
				{
					script_closure_t *ptr = PushClosureGlobal( vm, sizeof(tramp_data_t) );
					ptr->tramp = CreateTrampoline( vm, vm, type.Proc(), o, ptr + 1 );
					ptr->proc = type.obj;

					if ( ptr->tramp )
					{
						_SetArg( void*, 1, ptr->tramp->code );
					}
					else
					{
						RemoveClosureGlobal( vm, ptr );
						return SQ_ERROR;
					}
				}

				goto invalid_arg_type;
			}
#endif
			case FFIType_var:
			case FFIType_void:
			case -1:
invalid_arg_type:
				return throwerrorf( vm,
						_SC("invalid parameter %d type, expected '" FMT_CSTR "', got '" FMT_CSTR "'"),
						i,
						type.Str(),
						( sq_type(o) == OT_USERDATA &&
						  ( _userdata(o)->_typetag == TAG_STRUCT ||
							_userdata(o)->_typetag == TAG_ARRAY ||
							_userdata(o)->_typetag == TAG_MEMORY ||
							_userdata(o)->_typetag == TAG_LIBRARY ||
							_userdata(o)->_typetag == TAG_PROC ) ) ?
						_userdata(o)->_typetag :
						GetType( o ) );
			default:
				UNREACHABLE();
		}
	}

#undef _SetArg

	if ( !variadic )
	{
		cif = &proc->cif;
	}
	else
	{
		ffi_type **argtypes = (ffi_type**)alloca( nargs * sizeof(ffi_type*) );
		memcpy( argtypes, proc->fixed_argtypes, nfixedargs * sizeof(ffi_type*) );

		for ( ; i < nargs; i++ )
		{
			HSQOBJECT o;
			sq_getstackobj( vm, argsBase + i, &o );

			argmap[i] = argvec + argvecidx;

			switch ( sq_type(o) )
			{
				case OT_BOOL:
				case OT_INTEGER:
				{
					if ( sizeof(SQInteger) == 8 )
					{
						argtypes[i] = &ffi_type_sint64;
					}
					else
					{
						argtypes[i] = &ffi_type_sint32;
					}

					*(SQInteger*)( argvec + argvecidx ) = _integer(o);
					break;
				}
				case OT_FLOAT:
				{
					argtypes[i] = &ffi_type_double;
					*(double*)( argvec + argvecidx ) = (double)_float(o);
					break;
				}
				case OT_STRING:
				{
					argtypes[i] = &ffi_type_pointer;
					*(void**)( argvec + argvecidx ) = _string(o)->_val;
					break;
				}
				case OT_USERDATA:
				{
					argtypes[i] = &ffi_type_pointer;
					void *p;

					if ( _userdata(o)->_typetag == TAG_STRUCT )
					{
						script_struct_t *ptr = (script_struct_t*)_userdataval(o);
						p = ptr->addr;
					}
					else if ( _userdata(o)->_typetag == TAG_ARRAY )
					{
						script_array_t *ptr = (script_array_t*)_userdataval(o);
						p = ptr->addr;
					}
					else
					{
						p = (void*)_userdataval(o);
					}

					*(void**)( argvec + argvecidx ) = p;
					break;
				}
				default:
				{
					argtypes[i] = &ffi_type_pointer;
					*(void**)( argvec + argvecidx ) = _userpointer(o);
					break;
				}
			}

			argvecidx += sizeof(double);
		}

		cif = (ffi_cif*)alloca( sizeof(ffi_cif) );

		if ( ffi_prep_cif_var( cif,
					(ffi_abi)proc->abi,
					nfixedargs,
					nargs,
					rtype,
					argtypes ) != FFI_OK )
			return sq_throwerror( vm, _SC("FAILED") );
	}

	Assert( proc->func );

	__try
	{
		ffi_call( cif, (void (*)())proc->func, rvalue, argmap );
		__try_end();
	}
	__except( SQ_EXCEPTION_HANDLER )
	{
		return ThrowSQException( vm );
	}

	if ( proc->flags & FFIFlag_ERRNO )
	{
		g_ffi_errno = errno;
	}
#ifdef _WIN32
	else if ( proc->flags & FFIFlag_GETLASTERROR )
	{
		g_ffi_errno = GetLastError();
	}
#endif

	switch ( rettype.id )
	{
		case FFIType_void:
			return 0;
		case FFIType_i8:
		case FFIType_u8:
		case FFIType_i16:
		case FFIType_u16:
		case FFIType_i32:
		case FFIType_u32:
		case FFIType_i64:
		case FFIType_u64:
		{
			sq_pushinteger( vm, (SQInteger)*(ffi_arg*)rvalue );
			return 1;
		}
		case FFIType_f32:
		{
			sq_pushfloat( vm, (SQFloat)*(float*)rvalue );
			return 1;
		}
		case FFIType_f64:
		{
			sq_pushfloat( vm, (SQFloat)*(double*)rvalue );
			return 1;
		}
		case FFIType_str:
		{
			int ret;
			__try
			{
				char *val = *(char**)rvalue;
				int srclen = strlen( val );
#ifdef SQUNICODE
				STATIC_ASSERT( sizeof(SQChar) == sizeof(wchar_t) );
				int len = UTF8ToWChar( NULL, 0, val, srclen );
				SQChar *dst = sq_getscratchpad( vm, len * sizeof(SQChar) );
				UTF8ToWChar( dst, len * sizeof(SQChar), val, srclen );
				sq_pushstring( vm, dst, len );
#else
				sq_pushstring( vm, val, srclen );
#endif
				ret = 1;
				__try_end();
			}
			__except( SQ_EXCEPTION_HANDLER )
			{
				ret = ThrowSQException( vm );
			}

			return ret;
		}
		case FFIType_wstr:
		{
			int ret;
			__try
			{
				wchar_t *val = *(wchar_t**)rvalue;
				int srclen = wcslen( val );
#ifdef SQUNICODE
				sq_pushstring( vm, val, srclen );
#else
				int len = WCharToUTF8( NULL, 0, val, srclen );
				SQChar *dst = sq_getscratchpad( vm, len );
				WCharToUTF8( dst, len, val, srclen );
				sq_pushstring( vm, dst, len );
#endif
				ret = 1;
				__try_end();
			}
			__except( SQ_EXCEPTION_HANDLER )
			{
				ret = ThrowSQException( vm );
			}

			return ret;
		}
		case FFIType_ptr:
		{
			STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );
			sq_pushinteger( vm, (SQInteger)*(void**)rvalue );
			return 1;
		}
		case FFIType_struct:
		{
			script_struct_def_t *def = rettype.StructDef();
			Assert( def );
			Assert( def->ffitype.size == rtype->size );

			script_struct_t *ptr = PushStruct( vm, def->ffitype.size );
			ptr->addr = ptr + 1;
			ptr->def = rettype.obj;

			memcpy( ptr->addr, rvalue, def->ffitype.size );
			return 1;
		}
		case FFIType_fn:
		{
			CloneProc( vm, rettype.Proc(), *(void**)rvalue );
			return 1;
		}
		default: UNREACHABLE();
	}
}

#ifdef FFI_CLOSURES
tramp_data_t *CreateTrampoline( HSQUIRRELVM caller, HSQUIRRELVM vm,
		script_proc_t *proc, HSQOBJECT closure, void *ptdata )
{
	Assert( proc );
	Assert( sq_type(closure) == OT_CLOSURE || sq_type(closure) == OT_NATIVECLOSURE );

#ifdef HAS_SQCLOSURE_TYPE
	if ( sq_type(closure) == OT_CLOSURE )
	{
		int scriptparams =
			_fp(_closure(closure)->_function)->_nparameters -
#if SQUIRREL_VERSION_NUMBER > 212
			_fp(_closure(closure)->_function)->_ndefaultparams -
#endif
			_fp(_closure(closure)->_function)->_varparams -
			1;

		if ( scriptparams != proc->nfixedargs &&
				( !_fp(_closure(closure)->_function)->_varparams || scriptparams > proc->nfixedargs ) )
		{
			throwerrorf( caller,
					_SC("wrong number of closure parameters (expected %d, got %d)"),
					proc->nfixedargs,
					scriptparams );
			return NULL;
		}
	}
#endif

	void *code;
	ffi_closure *tramp = (ffi_closure*)ffi_closure_alloc( sizeof(ffi_closure), &code );
	tramp_data_t *tdata = (tramp_data_t*)ptdata;

	if ( !ptdata )
		tdata = (tramp_data_t*)sq_malloc( sizeof(tramp_data_t) );

	if ( ffi_prep_closure_loc( tramp,
				&proc->cif,
				TrampolineCallback,
				(void*)tdata,
				code ) != FFI_OK )
	{
		if ( !ptdata )
			sq_free( tdata, sizeof(tramp_data_t) );
		ffi_closure_free( tramp );
		sq_throwerror( caller, _SC("cannot create closure") );
		return NULL;
	}

	tdata->refs = 0;
	tdata->tramp = tramp;
	tdata->code = code;
	tdata->vm = vm;
	tdata->proc = proc;
	new (&tdata->closure) SQObjectPtr( closure );
	tdata->external = ( ptdata == NULL );
	return tdata;
}

void FreeTrampoline( tramp_data_t *tdata )
{
	Assert( tdata->refs > 0 );
	tdata->refs--;

	if ( tdata->refs == 0 )
	{
		ffi_closure *tramp = tdata->tramp;
		tdata->closure.Null();
		if ( tdata->external )
			sq_free( tdata, sizeof(tramp_data_t) );
		ffi_closure_free( tramp );
	}
}

void TrampolineCallback( ffi_cif *cif, void *ret, void **args, void *ud )
{
	tramp_data_t *tdata = (tramp_data_t*)ud;
	HSQUIRRELVM vm = tdata->vm;
	script_proc_t *proc = tdata->proc;
	unsigned int nargs = cif->nargs;
#ifdef _DEBUG
	int top = vm->_top;
#endif

	Assert( sq_type(tdata->closure) == OT_CLOSURE || sq_type(tdata->closure) == OT_NATIVECLOSURE );
#ifdef HAS_SQCLOSURE_TYPE
	Assert( vm->_top + (int)nargs +
			( sq_type(tdata->closure) == OT_CLOSURE ?
			  _fp(_closure(tdata->closure)->_function)->_stacksize : 0 ) < (SQInteger)vm->_stack.size() );
#endif

	sq_pushobject( vm, tdata->closure );
	sq_pushnull( vm );

	Assert( (int)cif->nargs == proc->nfixedargs );

	for ( unsigned int i = 0; i < nargs; i++ )
	{
		sqffitype_t &type = proc->argtypes[i];
		Assert( MapFFIType( type ) == cif->arg_types[i] );

		switch ( type.id )
		{
			case FFIType_i8:
			{
				sq_pushinteger( vm, (SQInteger)*(int8_t*)args[i] );
				break;
			}
			case FFIType_u8:
			{
				sq_pushinteger( vm, (SQInteger)*(uint8_t*)args[i] );
				break;
			}
			case FFIType_i16:
			{
				sq_pushinteger( vm, (SQInteger)*(int16_t*)args[i] );
				break;
			}
			case FFIType_u16:
			{
				sq_pushinteger( vm, (SQInteger)*(uint16_t*)args[i] );
				break;
			}
			case FFIType_i32:
			{
				sq_pushinteger( vm, (SQInteger)*(int32_t*)args[i] );
				break;
			}
			case FFIType_u32:
			{
				sq_pushinteger( vm, (SQInteger)*(uint32_t*)args[i] );
				break;
			}
			case FFIType_i64:
			{
				sq_pushinteger( vm, (SQInteger)*(int64_t*)args[i] );
				break;
			}
			case FFIType_u64:
			{
				sq_pushinteger( vm, (SQInteger)*(uint64_t*)args[i] );
				break;
			}
			case FFIType_f32:
			{
				sq_pushfloat( vm, (SQFloat)*(float*)args[i] );
				break;
			}
			case FFIType_f64:
			{
				sq_pushfloat( vm, (SQFloat)*(double*)args[i] );
				break;
			}
			case FFIType_str:
			{
				__try
				{
					char *val = *(char**)args[i];
					int srclen = strlen( val );
#ifdef SQUNICODE
					STATIC_ASSERT( sizeof(SQChar) == sizeof(wchar_t) );
					int len = UTF8ToWChar( NULL, 0, val, srclen );
					SQChar *dst = sq_getscratchpad( vm, len * sizeof(SQChar) );
					UTF8ToWChar( dst, len * sizeof(SQChar), val, srclen );
					sq_pushstring( vm, dst, len );
#else
					sq_pushstring( vm, val, srclen );
#endif
					__try_end();
					break;
				}
				__except( SQ_EXCEPTION_HANDLER )
				{
					ThrowSQException( vm );
				}

				STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );
				sq_pushinteger( vm, (SQInteger)*(void**)args[i] );
				break;
			}
			case FFIType_wstr:
			{
				__try
				{
					wchar_t *val = *(wchar_t**)args[i];
					int srclen = wcslen( val );
#ifdef SQUNICODE
					sq_pushstring( vm, val, srclen );
#else
					int len = WCharToUTF8( NULL, 0, val, srclen );
					SQChar *dst = sq_getscratchpad( vm, len );
					WCharToUTF8( dst, len, val, srclen );
					sq_pushstring( vm, dst, len );
#endif
					__try_end();
					break;
				}
				__except( SQ_EXCEPTION_HANDLER )
				{
					ThrowSQException( vm );
				}

				STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );
				sq_pushinteger( vm, (SQInteger)*(void**)args[i] );
				break;
			}
			case FFIType_ptr:
			{
				STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );
				sq_pushinteger( vm, (SQInteger)*(void**)args[i] );
				break;
			}
			case FFIType_struct:
			{
				script_struct_def_t *def = type.StructDef();
				script_struct_t *ptr = PushStruct( vm, def->ffitype.size );
				ptr->addr = ptr + 1;
				ptr->def = type.obj;
				memcpy( ptr->addr, args[i], def->ffitype.size );
				break;
			}
			case FFIType_fn:
			{
				script_proc_t *subproc = type.Proc();
				CloneProc( vm, subproc, *(void**)args[i] );
				break;
			}
			default:
				UNREACHABLE();
		}
	}

	sqffitype_t &rettype = proc->argtypes[proc->nfixedargs];
	Assert( MapFFIType( rettype ) == cif->rtype );

	bool succ = SQ_SUCCEEDED( sq_call( vm, nargs + 1, (SQBool)( rettype.id != FFIType_void ), SQTrue ) );

	if ( rettype.id == FFIType_void )
	{
		sq_pop( vm, 1 );
		Assert( vm->_top == top );
		return;
	}

	HSQOBJECT o;
	sq_getstackobj( vm, -1, &o );

	switch ( rettype.id )
	{
		case FFIType_i8:
		case FFIType_u8:
		case FFIType_i16:
		case FFIType_u16:
		case FFIType_i32:
		case FFIType_u32:
		case FFIType_i64:
		case FFIType_u64:
		{
			if ( succ )
			{
				if ( sq_type(o) == OT_INTEGER )
				{
					*(ffi_arg*)ret = (ffi_arg)_integer(o);
					break;
				}
				else if ( sq_type(o) == OT_FLOAT )
				{
					*(ffi_arg*)ret = (ffi_arg)_float(o);
					break;
				}
			}

			*(ffi_arg*)ret = 0;
			break;
		}
		case FFIType_f32:
		{
			if ( succ )
			{
				if ( sq_type(o) == OT_FLOAT )
				{
					*(float*)ret = (float)_float(o);
					break;
				}
				else if ( sq_type(o) == OT_INTEGER )
				{
					*(float*)ret = (float)_integer(o);
					break;
				}
			}

			*(float*)ret = 0;
			break;
		}
		case FFIType_f64:
		{
			if ( succ )
			{
				if ( sq_type(o) == OT_FLOAT )
				{
					*(double*)ret = (double)_float(o);
					break;
				}
				else if ( sq_type(o) == OT_INTEGER )
				{
					*(double*)ret = (double)_integer(o);
					break;
				}
			}

			*(double*)ret = 0;
			break;
		}
#ifdef SQUNICODE
		case FFIType_wstr:
#else
		case FFIType_str:
#endif
		{
			if ( succ )
			{
				if ( sq_type(o) == OT_INTEGER )
				{
					STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );
					*(void**)ret = (void*)_integer(o);
					break;
				}
				else if ( sq_type(o) == OT_USERPOINTER )
				{
					*(void**)ret = (void*)_userpointer(o);
					break;
				}
				else if ( sq_type(o) == OT_USERDATA )
				{
					if ( _userdata(o)->_typetag == TAG_ARRAY )
					{
						script_array_t *ptr = (script_array_t*)_userdataval(o);

						if ( ptr->type.ElementSize() == CIntToFFI< SQChar >() )
						{
							*(void**)ret = ptr->addr;
						}
					}
					else
					{
						*(void**)ret = (void*)_userdataval(o);
					}

					break;
				}
				else if ( sq_type(o) == OT_STRING )
				{
					*(void**)ret = (void*)_string(o)->_val;
					break;
				}
			}

			*(void**)ret = 0;
			break;
		}
#ifdef SQUNICODE
		case FFIType_str:
#else
		case FFIType_wstr:
#endif
		case FFIType_ptr:
		{
			if ( succ )
			{
				if ( sq_type(o) == OT_INTEGER )
				{
					STATIC_ASSERT( sizeof(SQInteger) >= sizeof(void*) );
					*(void**)ret = (void*)_integer(o);
					break;
				}
				else if ( sq_type(o) == OT_USERPOINTER )
				{
					*(void**)ret = (void*)_userpointer(o);
					break;
				}
				else if ( sq_type(o) == OT_USERDATA )
				{
					if ( _userdata(o)->_typetag == TAG_STRUCT )
					{
						script_struct_t *ptr = (script_struct_t*)_userdataval(o);
						*(void**)ret = ptr->addr;
					}
					else if ( _userdata(o)->_typetag == TAG_ARRAY )
					{
						script_array_t *ptr = (script_array_t*)_userdataval(o);
						*(void**)ret = ptr->addr;
					}
					else
					{
						*(void**)ret = (void*)_userdataval(o);
					}

					break;
				}
			}

			*(void**)ret = 0;
			break;
		}
		case FFIType_struct:
		{
			if ( succ && sq_type(o) == OT_USERDATA && _userdata(o)->_typetag == TAG_STRUCT )
			{
				script_struct_t *ptr = (script_struct_t*)_userdataval(o);
				script_struct_def_t *def = rettype.StructDef();

				if ( def == ptr->StructDef() )
				{
					Assert( def->ffitype.size == cif->rtype->size );
					memcpy( ret, ptr->addr, cif->rtype->size );
					break;
				}
			}

			memset( ret, 0, cif->rtype->size );
			break;
		}
		case FFIType_fn:
		{
			if ( succ && ( sq_type(o) == OT_CLOSURE || sq_type(o) == OT_NATIVECLOSURE ) )
			{
				script_closure_t *ptr = PushClosureGlobal( vm, sizeof(tramp_data_t) );
				ptr->tramp = CreateTrampoline( vm, vm, rettype.Proc(), o, ptr + 1 );
				ptr->proc = rettype.obj;

				if ( ptr->tramp )
				{
					*(void**)ret = ptr->tramp->code;
					break;
				}
				else
				{
					RemoveClosureGlobal( vm, ptr );
				}
			}

			*(void**)ret = 0;
			break;
		}
		default:
			UNREACHABLE();
	}

	sq_pop( vm, succ ? 2 : 1 );
	Assert( vm->_top == top );
}
#endif

// GetFunction( module, signature, flags = 0 )
// GetFunction( module, returnType, functionName, argType... )
SQInteger ffi_GetFunction( HSQUIRRELVM vm )
{
	const SQChar *signature = NULL;
	int base = sq_gettop( vm );
	parser_args_t args = {};

	HSQOBJECT hModule, arg2;
	sq_getstackobj( vm, 2, &hModule );
	sq_getstackobj( vm, 3, &arg2 );
	Assert( sq_type(hModule) == OT_USERDATA );

	if ( _userdata(hModule)->_typetag != TAG_LIBRARY )
		return sq_throwerror( vm, _SC("invalid library") );

	// TODO: Ordinal support
	if ( sq_type(arg2) == OT_STRING )
	{
		if ( base == 4 )
		{
			HSQOBJECT arg3;
			sq_getstackobj( vm, 4, &arg3 );

			if ( sq_type(arg3) != OT_INTEGER )
				return sq_throwerror( vm, _SC("expected integer") );

			args.flags = (int)_integer(arg3);
		}
		else if ( base != 3 )
		{
			return throwerrorf( vm,
					_SC("wrong number of parameters (expected " FMT_STR "%d, got %d)"),
					_SC(""),
					3,
					base );
		}

		signature = _string(arg2)->_val;
		Parser_Init( signature );
		Parser_ReadType( vm, &signature, &args );

		if ( args.type.id == -1 )
			return SQ_ERROR;

		if ( !args.type.IsFunction() )
			return sq_throwerror( vm, _SC("expected function type") );
	}
	else
	{
		if ( SQ_FAILED( sq_getstring( vm, 4, &args.name ) ) )
			return sq_throwerror( vm, _SC("expected function name") );

		args.namelen = scstrlen( args.name );
	}

	if ( args.namelen > 511 )
		return sq_throwerror( vm, _SC("name is too long") );

	char *procname = (char*)alloca( args.namelen + 1 );
#ifdef SQUNICODE
	WCharToUTF8( procname, args.namelen, args.name, args.namelen );
#else
	memcpy( procname, args.name, args.namelen );
#endif
	procname[args.namelen] = 0;

	void *module = *(void**)_userdataval(hModule);
#ifdef _WIN32
	void *func = (void*)GetProcAddress( (HMODULE)module, procname );
#else
	void *func = dlsym( module, procname );
#endif

	if ( !func )
	{
#ifdef _WIN32
		Errorf( "could not find '%s'", procname );
		PrintWin32Error( ": %s\n", GetLastError() );
#else
		Errorf( "%s\n", dlerror() );
#endif
		return 0;
	}

	if ( !signature )
	{
		args.type.id = FFIType_fn;
		args.nargs = base - 4;
		args.abi = FFI_DEFAULT_ABI;

		if ( !InitParserStack( vm, args.nargs, 3, 5 ) )
			return SQ_ERROR;

		if ( !DoMakeFunction( vm, 0, &args ) )
			return SQ_ERROR;
	}

	InitProc( args.type.Proc(), func, NULL, &hModule );
	sq_pushobject( vm, args.type.obj );

	return 1;
}

// MakeFunction( returnType, functionAddress, argType... )
SQInteger ffi_MakeFunction( HSQUIRRELVM vm )
{
	void *func;
	int base = sq_gettop( vm );
	parser_args_t args = {};

	HSQOBJECT o;
	sq_getstackobj( vm, 3, &o );

	if ( sq_type(o) == OT_INTEGER )
	{
		func = (void*)_integer(o);
	}
	else if ( sq_type(o) == OT_USERPOINTER )
	{
		func = (void*)_userpointer(o);
	}
	else if ( sq_type(o) == OT_USERDATA )
	{
		if ( _userdata(o)->_typetag != TAG_MEMORY )
			return sq_throwerror( vm, _SC("invalid input") );

		func = (void*)_userdataval(o);
	}
	else UNREACHABLE();

	args.type.id = FFIType_fn;
	args.nargs = base - 3;
	args.abi = FFI_DEFAULT_ABI;

	if ( !InitParserStack( vm, args.nargs, 2, 4 ) )
		return SQ_ERROR;

	if ( !DoMakeFunction( vm, 0, &args ) )
		return SQ_ERROR;

	InitProc( args.type.Proc(), func, NULL, NULL );
	sq_pushobject( vm, args.type.obj );

	return 1;
}

// GetFunctions( targetContainer, module, signatures )
SQInteger ffi_GetFunctions( HSQUIRRELVM vm )
{
	HSQOBJECT hModule, container;
	parser_args_t args = {};
	const SQChar *signature = NULL;
	void *module;
	void *func;
	char procname[512];

	sq_getstackobj( vm, 2, &container );
	sq_getstackobj( vm, 3, &hModule );
	sq_getstring( vm, 4, &signature );

	Assert( sq_type(hModule) == OT_USERDATA );

	if ( _userdata(hModule)->_typetag != TAG_LIBRARY )
		return sq_throwerror( vm, _SC("invalid library") );

	module = *(void**)_userdataval(hModule);
	Parser_Init( signature );
	Parser_SkipWhitespace( &signature );

parse:
	for (;;)
	{
		int res = Parser_Typedef2( vm, &signature );

		if ( res == -1 )
			return SQ_ERROR;

		if ( res == 1 )
			break;
	}

	args = {};
	Parser_ReadType( vm, &signature, &args );

	if ( args.type.id == -1 )
		return SQ_ERROR;

	if ( !args.type.IsFunction() )
	{
		fileindex_t pidx = Parser_GetIndex( signature );
		return throwerrorf( vm, _SC("expected function type @L%d:%d"), pidx.line, pidx.col );
	}

	if ( args.namelen > (int)sizeof(procname) - 1 )
	{
		fileindex_t pidx = Parser_GetIndex( signature );
		return throwerrorf( vm, _SC("name is too long @L%d:%d"), pidx.line, pidx.col );
	}

	if ( args.namelen == 0 )
	{
		fileindex_t pidx = Parser_GetIndex( signature );
		return throwerrorf( vm, _SC("expected name @L%d:%d"), pidx.line, pidx.col );
	}

	if ( !Parser_Skip( &signature, ';' ) )
	{
		fileindex_t pidx = Parser_GetIndex( signature );
		return throwerrorf( vm, _SC("expected '%c' @L%d:%d"), ';', pidx.line, pidx.col );
	}

#ifdef SQUNICODE
	WCharToUTF8( procname, args.namelen, args.name, args.namelen );
#else
	memcpy( procname, args.name, args.namelen );
#endif
	procname[args.namelen] = 0;

#ifdef _WIN32
	func = (void*)GetProcAddress( (HMODULE)module, procname );
#else
	func = dlsym( module, procname );
#endif

	if ( !func )
	{
		sq_pushstring( vm, args.name, args.namelen );
		sq_pushnull( vm );
		sq_newslot( vm, 2, SQTrue );

#ifdef _WIN32
		Errorf( "could not find '%s'", procname );
		PrintWin32Error( ": %s\n", GetLastError() );
#else
		Errorf( "%s\n", dlerror() );
#endif
	}
	else
	{
		InitProc( args.type.Proc(), func, NULL, &hModule );

		sq_pushstring( vm, args.name, args.namelen );
		sq_pushobject( vm, args.type.obj );
		sq_newslot( vm, 2, SQTrue );
	}

	if ( *signature != 0 )
		goto parse;

	return 0;
}

static void **GetVTable( void *thisptr )
{
	void **vtbl = NULL;

	__try
	{
		vtbl = *(void***)thisptr;
		__try_end();
	}
	__except( SQ_EXCEPTION_HANDLER )
	{
		ThrowSQException( g_vm );
	}

	return vtbl;
}

// GetVirtualClass( targetContainer, instanceAddress, virtualClass )
SQInteger ffi_GetVirtualClass( HSQUIRRELVM vm )
{
	const SQChar *signature = NULL;
	void **vtbl;
	void *thisptr;
	int vfnidx = 0;
	int isClass = 0;
	int ret;
	HSQOBJECT o, container;
	parser_args_t args = {};
	int _g_nParserTypedefsIndex = g_nParserTypedefsIndex;

	sq_getstackobj( vm, 2, &container );
	sq_getstackobj( vm, 3, &o );
	sq_getstring( vm, 4, &signature );

	if ( sq_type(o) == OT_INTEGER )
	{
		thisptr = (void*)_integer(o);
	}
	else if ( sq_type(o) == OT_USERPOINTER )
	{
		thisptr = (void*)_userpointer(o);
	}
	else if ( sq_type(o) == OT_USERDATA )
	{
		if ( _userdata(o)->_typetag != TAG_MEMORY )
			return sq_throwerror( vm, _SC("invalid input") );

		thisptr = (void*)_userdataval(o);
	}
	else UNREACHABLE();

	if ( !thisptr )
		return sq_throwerror( vm, _SC("nullptr") );

	vtbl = GetVTable( thisptr );

	if ( !vtbl )
		return SQ_ERROR;

	Parser_Init( signature );
	Parser_SkipWhitespace( &signature );

	isClass = Parser_Skip( &signature, _SC("class") );

	if ( !isClass )
		Parser_Skip( &signature, _SC("struct") );

	int len;
	Parser_ReadIdentifier( &signature, &len );

	if ( !Parser_Skip( &signature, '{' ) )
		return throwerrorf( vm, _SC("expected '%c'"), '{' );

parse_class_func:
	if ( Parser_Skip( &signature, '}' ) )
	{
		Parser_Skip( &signature, ';' );

		if ( *signature != 0 )
		{
			ret = throwerrorf( vm, _SC("expected end of string, got '%c'"), *signature );
			goto exit;
		}

		ret = 0;
		goto exit;
	}

	while ( Parser_Skip( &signature, _SC("typedef") ) )
	{
		if ( !Parser_Typedef( vm, &signature ) )
		{
			ret = SQ_ERROR;
			goto exit;
		}
	}

	if ( isClass )
	{
		if ( Parser_Skip( &signature, _SC("public") ) && !Parser_Skip( &signature, ':' ) )
		{
			ret = throwerrorf( vm, _SC("expected '%c'"), ':' );
			goto exit;
		}

		while ( Parser_Skip( &signature, _SC("typedef") ) )
		{
			if ( !Parser_Typedef( vm, &signature ) )
			{
				ret = SQ_ERROR;
				goto exit;
			}
		}

		Parser_Skip( &signature, _SC("virtual") );
	}

	args = {};
	args.has_thisptr = 1;
#if defined(_WIN32) && !defined(_WIN64)
	args.abi = FFI_THISCALL;
#else
	args.abi = FFI_DEFAULT_ABI;
#endif

	Parser_ReadType( vm, &signature, &args );

	if ( args.type.id == -1 )
	{
		ret = SQ_ERROR;
		goto exit;
	}

	if ( args.type.id != FFIType_fndec )
	{
		ret = sq_throwerror( vm, _SC("expected function declaration") );
		goto exit;
	}

	if ( Parser_Skip( &signature, '=' ) )
	{
		if ( !( Parser_Skip( &signature, '0' ) && Parser_Skip( &signature, ';' ) ) )
		{
			ret = sq_throwerror( vm, _SC("expected '= 0;'") );
			goto exit;
		}
	}
	else if ( !Parser_Skip( &signature, ';' ) )
	{
		ret = throwerrorf( vm, _SC("expected '%c'"), ';' );
		goto exit;
	}

	InitProc( args.type.Proc(), vtbl[vfnidx], thisptr, NULL );
	vfnidx++;

	sq_pushstring( vm, args.name, args.namelen );
	sq_pushobject( vm, args.type.obj );
	sq_newslot( vm, 2, SQTrue );

	goto parse_class_func;

exit:
	Parser_Typedef_Restore( _g_nParserTypedefsIndex );
	return ret;
}

static bool UnpackStruct( HSQUIRRELVM vm,
		const SQChar **signature,
		sqffitype_t &hsrc,
		script_struct_def_t *dst,
		int &membercount,
		bool firstpass )
{
	script_struct_def_t *src = hsrc.StructDef();

	for ( int i = 0; i < src->membercount; i++ )
	{
		script_struct_def_t::member_t *member_src = &src->members[i];

		if ( sq_type(member_src->key) != OT_STRING && member_src->type.id == FFIType_struct )
		{
			if ( !UnpackStruct( vm, signature, member_src->type, dst, membercount, firstpass ) )
				return false;
		}

		if ( !firstpass )
		{
			if ( sq_type(member_src->key) == OT_STRING )
			{
				script_struct_def_t::member_t *dup = FindMember( dst, _string(member_src->key) );
				if ( dup )
				{
					fileindex_t pidx = Parser_GetIndex( *signature );
					throwerrorf( vm,
							_SC("'" FMT_CSTR " " FMT_STR "::" FMT_VSTR "' "
								"redefinition on anonymous struct unpack @L%d:%d"),
							dup->type.Str(),
							_string(dst->name)->_val,
							_string(member_src->key)->_len, _string(member_src->key)->_val,
							pidx.line, pidx.col );
					return false;
				}
			}

			script_struct_def_t::member_t *member_dst = &dst->members[ membercount ];
			*member_dst = *member_src;
		}

		membercount++;
	}

	if ( !firstpass )
	{
		RemoveStructDef( vm, hsrc.obj );
		hsrc.Release();
	}

	return true;
}

static int ParseStructMember( HSQUIRRELVM vm,
		const SQChar **signature,
		int alignment,
		int packing,
		bool isUnion,
		int &structsize,
		int &structalign,
		script_struct_def_t *def,
		bool firstpass )
{
	int membercount = 0;
#ifndef SQ_DISABLE_BITFIELDS
	int bitsalign = 0;
	int bitsnextalign = 0;
	int bitsused = 0;
	int bits;
#endif
	parser_args_t args = {};
	script_struct_def_t::member_t *member = NULL;

parse_member:
	if ( firstpass )
	{
		Assert( !g_bCreateFunc );
		g_bCreateFunc = true;

		while ( Parser_Skip( signature, _SC("typedef") ) )
		{
			if ( !Parser_Typedef( vm, signature ) )
				return -1;
		}

		g_bCreateFunc = false;
	}
	else
	{
		while ( Parser_Skip( signature, _SC("typedef") ) )
			Parser_Typedef_Skip( vm, signature );
	}

	if ( Parser_Skip( signature, '}' ) )
	{
		if ( firstpass )
		{
			if ( membercount )
			{
				if ( alignment )
				{
					if ( structalign < alignment )
						structalign = alignment;
				}
				else if ( packing && packing < structalign )
				{
					structalign = packing;
				}

				int pad = ( ( structalign - ( structsize & ( structalign - 1 ) ) ) & ( structalign - 1 ) );
				structsize += pad;
			}
			else
			{
				Assert( structsize == 0 );
				structalign = 1;
				structsize = 1;
			}

			return membercount;
		}
		else
		{
#ifdef _DEBUG
			if ( membercount )
			{
				if ( alignment )
				{
					if ( structalign < alignment )
						structalign = alignment;
				}
				else if ( packing && packing < structalign )
				{
					structalign = packing;
				}

				int pad = ( ( structalign - ( structsize & ( structalign - 1 ) ) ) & ( structalign - 1 ) );
				structsize += pad;
			}
			else
			{
				Assert( structsize == 0 );
				structalign = 1;
				structsize = 1;
			}

			Assert( (int)def->ffitype.size == structsize );
			Assert( def->membercount == membercount );
#endif
			return membercount;
		}
	}

	args = {};

parse_member_1:
#ifndef SQ_DISABLE_BITFIELDS
	bits = 0;
#endif

	if ( firstpass )
	{
		Parser_ReadType( vm, signature, &args );

		if ( args.type.id == -1 )
			return -1;

		if ( args.type.id == FFIType_fndec )
		{
			fileindex_t pidx = Parser_GetIndex( *signature );
			throwerrorf( vm, _SC("unexpected function declaration @L%d:%d"), pidx.line, pidx.col );
			return -1;
		}
	}
	else
	{
		Parser_ReadType( vm, signature, &args );
		Assert( args.type.id != -1 );
	}

	if ( !firstpass )
	{
		member = &def->members[ membercount ];
		member->type = args.type;
		member->type.Normalise();
		Assert( member->type.IsValid() );

		if ( !isUnion )
			def->ffitype.elements[ membercount ] = MapFFIType( args.type );

		if ( args.namelen )
		{
			script_struct_def_t::member_t *dup = FindMember( def, args.name, args.namelen );
			if ( dup )
			{
				fileindex_t pidx = Parser_GetIndex( *signature );
				throwerrorf( vm, _SC("'" FMT_CSTR " " FMT_STR "::" FMT_VSTR "' redefinition @L%d:%d"),
						dup->type.Str(),
						_string(def->name)->_val,
						args.namelen, args.name,
						pidx.line, pidx.col );
				return -1;
			}

			HSQOBJECT tmp;
			sq_pushstring( vm, args.name, args.namelen );
			sq_getstackobj( vm, -1, &tmp );
			member->key = tmp;
			sq_pop( vm, 1 );
		}
	}

	if ( Parser_Skip( signature, ':' ) )
	{
#ifndef SQ_DISABLE_BITFIELDS
		if ( firstpass )
		{
			if ( !( args.type.id >= FFIType_INTEGRAL_BEGIN && args.type.id <= FFIType_INTEGRAL_END ) ||
					args.type.isarray )
			{
				fileindex_t pidx = Parser_GetIndex( *signature );
				throwerrorf( vm, _SC("bit field must have integral type @L%d:%d"), pidx.line, pidx.col );
				return -1;
			}
		}
		else
		{
			Assert( ( args.type.id >= FFIType_INTEGRAL_BEGIN && args.type.id <= FFIType_INTEGRAL_END ) &&
					!args.type.isarray );
		}

		const SQChar *pStart = *signature;
		do { (*signature)++; } while ( _isdigit( **signature ) );
		bits = atoi( pStart, *signature - pStart );

		if ( firstpass )
		{
			if ( bits < 0 || bits > args.type.ElementSize() * 8 )
			{
				fileindex_t pidx = Parser_GetIndex( *signature );
				throwerrorf( vm, _SC("bit field value '%d' does not fit type '" FMT_CSTR "' @L%d:%d"),
						bits,
						args.type.Str(),
						pidx.line, pidx.col );
				return -1;
			}

			if ( bits == 0 && args.namelen != 0 )
			{
				fileindex_t pidx = Parser_GetIndex( *signature );
				throwerrorf( vm, _SC("named bit field cannot have zero width @L%d:%d"),
						pidx.line, pidx.col );
				return -1;
			}
		}
		else
		{
			Assert( !( bits < 0 || bits > args.type.ElementSize() * 8 ) );
		}

		// To differentiate none and zero
		if ( bits == 0 )
			bits = -1;
#else
		Assert( firstpass );
		fileindex_t pidx = Parser_GetIndex( *signature );
		throwerrorf( vm, _SC("bit fields are disabled @L%d:%d"),
				pidx.line, pidx.col );
		return -1;
#endif
	}

	if ( firstpass )
	{
#ifdef SQ_DISABLE_BITFIELDS
		if ( args.namelen == 0 && args.type.id != FFIType_structdef )
#else
		if ( args.namelen == 0 && args.type.id != FFIType_structdef && bits != -1 )
#endif
		{
			fileindex_t pidx = Parser_GetIndex( *signature );
			throwerrorf( vm, _SC("expected member name @L%d:%d"), pidx.line, pidx.col );
			return -1;
		}
	}
	else
	{
#ifdef SQ_DISABLE_BITFIELDS
		Assert( !( args.namelen == 0 && args.type.id != FFIType_structdef ) );
#else
		Assert( !( args.namelen == 0 && args.type.id != FFIType_structdef && bits != -1 ) );
#endif
	}

	int memberalign = args.type.Alignment();
	int membersize = args.type.ByteSize();
	int memberpad = ( packing && packing < memberalign ) ? packing : memberalign;

#ifndef SQ_DISABLE_BITFIELDS
	// zero width bitfield
	if ( bits == -1 )
	{
		// only the first one counts
		if ( !bitsnextalign )
		{
			bitsnextalign = memberalign;
			bitsalign = memberalign;
			bitsused = 0;

			int pad = ( ( memberpad - ( structsize & ( memberpad - 1 ) ) ) & ( memberpad - 1 ) );
			structsize = (int)( membercount != 0 ) * ( pad + structsize );

			if ( firstpass && structalign < memberalign )
				structalign = memberalign;
		}
	}
	else
#endif
	{
		if ( firstpass && structalign < memberalign )
			structalign = memberalign;

		if ( !isUnion )
		{
#ifndef SQ_DISABLE_BITFIELDS
			// occupies the previous space
			if ( bits && bitsused &&
					// msvc behaviour, always aligned on type
					// gcc ignores the underlying type on bitfields and packing
					bitsalign == memberalign &&
					bitsused + bits <= memberalign * 8 )
			{
				if ( !firstpass )
				{
					Assert( membercount && structsize );
					member->offset = member[-1].offset;
					member->type.bits = bits + bitsused;
					member->type.shift = bitsused;
				}

				bitsused += bits;
			}
			// new space
			else
#endif
			{
				int pad = ( ( memberpad - ( structsize & ( memberpad - 1 ) ) ) & ( memberpad - 1 ) );
				structsize = (int)( membercount != 0 ) * ( pad + structsize );

				if ( !firstpass )
					member->offset = structsize;

				structsize += membersize;

#ifndef SQ_DISABLE_BITFIELDS
				if ( !firstpass )
				{
					member->type.bits = bits;
					member->type.shift = 0;
				}

				bitsalign = memberalign;
				bitsused = bits;
#endif
			}
		}
		else // union
		{
			if ( !firstpass )
				Assert( member->offset == 0 );

			if ( structsize < membersize )
				structsize = membersize;

#ifndef SQ_DISABLE_BITFIELDS
			if ( !firstpass )
			{
				member->type.bits = bits;
				member->type.shift = 0;
			}

			bitsused = 0;
#endif
		}

#ifndef SQ_DISABLE_BITFIELDS
		bitsnextalign = 0;
#endif

		// unnamed struct in union
		if ( args.namelen == 0 )
		{
			Assert( args.type.IsStruct() );

			if ( !UnpackStruct( vm, signature, args.type, def, membercount, firstpass ) )
				return -1;
		}
		else
		{
			membercount++;
		}
	}

	if ( Parser_Skip( signature, ',' ) )
	{
		// Same type, new name
		args.has_type = 1;

		if ( args.tdef )
		{
			args.type = *args.tdef;
		}
		else
		{
			args.type.StripPtr();
		}

		goto parse_member_1;
	}
	else
	{
		if ( firstpass )
		{
			if ( !Parser_Skip( signature, ';' ) )
			{
				fileindex_t pidx = Parser_GetIndex( *signature );
				throwerrorf( vm, _SC("expected '%c' @L%d:%d"), ';', pidx.line, pidx.col );
				return -1;
			}
		}
		else
		{
			Parser_Skip( signature, ';' );
		}
	}

	goto parse_member;
}

bool DefineStruct( HSQUIRRELVM vm,
		const SQChar **signature,
		const SQChar *name,
		int namelen,
		int alignment,
		int packing,
		bool isUnion,
		SQObjectPtr &out )
{
	STACKCHECK( vm );

	if ( !Parser_Skip( signature, '{' ) )
	{
		fileindex_t pidx = Parser_GetIndex( *signature );
		throwerrorf( vm, _SC("expected '%c' @L%d:%d"), '{', pidx.line, pidx.col );
		return false;
	}

	if ( g_bCreateFunc && namelen && FindStructDef( vm, name, namelen, out ) )
	{
#ifndef SQ_DISABLE_TYPE_REDEFINITION
		errorfunc( vm, _SC("Warning: struct " FMT_VSTR ": type redefinition\n"), namelen, name );
		RemoveStructDef( vm, out );
		out.Null();
#else
		fileindex_t pidx = Parser_GetIndex( *signature );
		return throwerrorf( vm, _SC("redefinition of type " FMT_VSTR " @L%d:%d"),
				namelen, name, pidx.line, pidx.col );
#endif
	}

	const SQChar *signature_start = *signature;
	int membercount = 0;
	int structsize = 0;
	int structalign = 0;
	script_struct_def_t *def;
	int _g_nParserTypedefsIndex = g_nParserTypedefsIndex;

	bool _g_bCreateFunc = g_bCreateFunc;
	g_bCreateFunc = false;
	membercount = ParseStructMember( vm, signature, alignment, packing, isUnion,
			structsize, structalign, NULL, true );
	g_bCreateFunc = _g_bCreateFunc;

	if ( membercount == -1 )
	{
		Parser_Typedef_Restore( _g_nParserTypedefsIndex );
		return false;
	}

	int size =
		membercount * sizeof(script_struct_def_t::member_t) +
		( membercount + 1 ) * sizeof(ffi_type*);

	if ( namelen )
	{
		def = PushStructDefGlobal( vm, size, out );
		sq_pushstring( vm, name, namelen );
	}
	else
	{
		def = PushStructDef( vm, size, out );
		static unsigned int g_count = 0;
		SQChar tmp[ STRLEN("$unnamed4294967295") + 1 ];
		namelen = scsprintf( tmp, ARRAYSIZE(tmp), _SC("$unnamed%u"), g_count++ );
		sq_pushstring( vm, tmp, namelen );
	}

	{
		HSQOBJECT tmp;
		sq_getstackobj( vm, -1, &tmp );
		def->name = tmp;
		sq_pop( vm, 1 );
	}

	def->membercount = membercount;
	def->ffitype.size = structsize;
	def->ffitype.alignment = structalign;
	def->ffitype.type = FFI_TYPE_STRUCT;

	if ( !isUnion )
	{
		def->ffitype.elements = (ffi_type**)( (char*)def +
				sizeof(script_struct_def_t) +
				membercount * sizeof(script_struct_def_t::member_t) );
	}

	*signature = signature_start;
	structsize = 0;

	membercount = ParseStructMember( vm, signature, alignment, packing, isUnion,
			structsize, structalign, def, false );

	Parser_Typedef_Restore( _g_nParserTypedefsIndex );

	if ( membercount == -1 )
		return false;

	return true;
}

// MakeArray( type, address, count )
SQInteger ffi_MakeArray( HSQUIRRELVM vm )
{
	HSQOBJECT o, type;
	parser_args_t args = {};
	void *addr;
	SQInteger count;

	sq_getstackobj( vm, 2, &type );
	sq_getstackobj( vm, 3, &o );

	if ( sq_gettop( vm ) == 4 )
	{
		sq_getinteger( vm, 4, &count );
	}
	else
	{
		// unbound by default
		count = INT_MAX >> 1;
	}

	if ( count < 0 || count > (INT_MAX >> 1) )
		return sq_throwerror( vm, _SC("invalid count") );

	if ( sq_type(type) != OT_INTEGER || FFITypeByteSize( _integer(type) ) <= 0 )
		return sq_throwerror( vm, _SC("invalid type") );

	args.type.count = count;
	args.type.id = _integer(type);

	if ( sq_type(o) == OT_INTEGER )
	{
		addr = (void*)_integer(o);
	}
	else if ( sq_type(o) == OT_USERPOINTER )
	{
		addr = (void*)_userpointer(o);
	}
	else if ( sq_type(o) == OT_USERDATA )
	{
		if ( _userdata(o)->_typetag == TAG_ARRAY )
		{
			script_array_t *ptr = (script_array_t*)_userdataval(o);
			addr = ptr->addr;
		}
		else
		{
			addr = (void*)_userdataval(o);
		}
	}
	else UNREACHABLE();

	Assert( args.type.IsValid() );

	script_array_t *ptr = PushArray( vm, 0 );
	ptr->addr = addr;
	ptr->type = args.type;
	return 1;
}

#ifdef FFI_CLOSURES
SQInteger ffi_MakeClosure( HSQUIRRELVM vm )
{
	const SQChar *signature = NULL;
	parser_args_t args = {};
	SQObjectPtr proc;

	HSQOBJECT o, fun, thread = {};
	sq_getstackobj( vm, 2, &o );
	sq_getstackobj( vm, 3, &fun );

	if ( sq_gettop( vm ) > 3 )
	{
		sq_getstackobj( vm, 4, &thread );
		Assert( sq_type(thread) == OT_THREAD );
	}

	Assert( sq_type(fun) == OT_CLOSURE || sq_type(fun) == OT_NATIVECLOSURE );

	if ( sq_type(o) == OT_STRING )
	{
		signature = _string(o)->_val;
		Parser_Init( signature );
		Parser_ReadType( vm, &signature, &args );

		if ( args.type.id == -1 )
			return SQ_ERROR;

		Parser_Skip( &signature, ';' );

		if ( *signature != 0 )
			return throwerrorf( vm, _SC("expected end of string, got '%c'"), *signature );

		if ( !args.type.IsFunction() )
			return sq_throwerror( vm, _SC("expected function type") );

		proc = args.type.obj;
	}
	else if ( sq_type(o) == OT_USERDATA )
	{
		if ( _userdata(o)->_typetag == TAG_PROC )
		{
			STACKCHECK( vm );
			HSQOBJECT tmp;
			CloneProc( vm, (script_proc_t*)_userdataval(o), NULL );
			sq_getstackobj( vm, -1, &tmp );
			Assert( sq_type(tmp) == OT_USERDATA );
			proc = tmp;
			sq_pop( vm, 1 );
		}
		else
		{
			return sq_throwerror( vm, _SC("invalid input") );
		}
	}
	else UNREACHABLE();

	script_closure_t *ptr = PushClosure( vm, sizeof(tramp_data_t) );
	ptr->tramp = CreateTrampoline( vm,
			( sq_type(thread) == OT_THREAD ) ? _thread(thread) : vm,
			(script_proc_t*)_userdataval(proc),
			fun,
			ptr + 1 );
	ptr->proc = proc;

	if ( sq_type(thread) == OT_THREAD && _thread(thread) != _thread(_ss(vm)->_root_vm) )
		ptr->thread = thread;

	if ( ptr->tramp )
		return 1;

	sq_pop( vm, 1 );
	return SQ_ERROR;
}

SQInteger ffi_Clone( HSQUIRRELVM vm )
{
#ifndef HAS_SQCLOSURE_TYPE
	return sq_throwerror( vm, _SC("cannot clone in DLL build") );
#else
	HSQOBJECT o;
	sq_getstackobj( vm, 2, &o );

	if ( sq_type(o) == OT_CLOSURE )
	{
		if ( CLOSURE_ENV_ISVALID( _closure(o)->_env ) )
			return sq_throwerror( vm, _SC("cannot clone closure with bound env") );

		HSQOBJECT ret;
		sq_type(ret) = OT_CLOSURE;
		_closure(ret) = _closure(o)->Clone();
		sq_pushobject( vm, ret );
	}
	else if ( sq_type(o) == OT_NATIVECLOSURE )
	{
		if ( CLOSURE_ENV_ISVALID( _nativeclosure(o)->_env ) )
			return sq_throwerror( vm, _SC("cannot clone closure with bound env") );

		HSQOBJECT ret;
		sq_type(ret) = OT_NATIVECLOSURE;
		_nativeclosure(ret) = _nativeclosure(o)->Clone();
		sq_pushobject( vm, ret );
	}
	else if ( sq_type(o) == OT_USERDATA )
	{
		if ( _userdata(o)->_typetag == TAG_PROC )
		{
			script_proc_t *proc = (script_proc_t*)_userdataval(o);
			CloneProc( vm, proc, proc->func );
		}
		else
		{
			return sq_throwerror( vm, _SC("invalid input") );
		}
	}
	else UNREACHABLE();

	return 1;
#endif
}
#endif

void Parser_Init( const SQChar *str )
{
	g_pParserStart = str;
}

fileindex_t Parser_GetIndex( const SQChar *ptr )
{
	const SQChar *pStart = g_pParserStart;
	int line = 0;
	int col = 0;

	for ( ; pStart < ptr; pStart++ )
	{
		if ( *pStart == '\n' )
		{
			line++;
			col = 0;
		}
		else
		{
			col++;
		}
	}

	return { line, col };
}

void Parser_SkipWhitespace( const SQChar **str )
{
	for (;;)
	{
		if ( **str == ' ' || **str == '\t' || **str == '\n' || **str == '\r' )
		{
			(*str)++;
			continue;
		}

		// also skip comments
		if ( (*str)[0] == '/' && (*str)[1] == '*' )
		{
			(*str) += 2;

			for (;;)
			{
				if ( **str == 0 )
					return;

				if ( (*str)[0] == '*' && (*str)[1] == '/' )
				{
					(*str) += 2;
					break;
				}

				(*str)++;
			}

			continue;
		}

		if ( (*str)[0] == '/' && (*str)[1] == '/' )
		{
			(*str) += 2;

			for (;;)
			{
				if ( **str == 0 )
					return;

				if ( **str == '\n' )
				{
					(*str)++;
					break;
				}

				(*str)++;
			}

			continue;
		}

		break;
	}
}

const SQChar *Parser_PeekIdentifier( const SQChar *str, int *len )
{
	const SQChar *id = str;
	while ( _isalnum( *str ) || *str == '_' || *str == '@' || *str == '?' )
		str++;

	*len = str - id;
	return id;
}

const SQChar *Parser_ReadIdentifier( const SQChar **str, int *len )
{
	const SQChar *id = Parser_PeekIdentifier( *str, len );
	*str += *len;
	Parser_SkipWhitespace( str );
	return id;
}

template < int SIZE >
int Parser_Skip( const SQChar **str, const SQChar (&target)[SIZE] )
{
	int len;
	const SQChar *id = Parser_PeekIdentifier( *str, &len );

	if ( STRCMP( id, len, target ) )
	{
		*str += len;
		Parser_SkipWhitespace( str );
		return 1;
	}
	else
	{
		return 0;
	}
}

int Parser_Skip( const SQChar **str, SQChar ch )
{
	if ( **str == ch )
	{
		(*str)++;
		Parser_SkipWhitespace( str );
		return 1;
	}
	else
	{
		return 0;
	}
}

int Parser_GetKeyword( const SQChar *id, int len )
{
#define _check( kw ) \
	if ( STRCMP( id, len, _SC(#kw) ) ) \
	{ \
		return Token_##kw; \
	}

	_check( const )
	_check( unsigned )
	_check( signed )
	_check( void )
	_check( char )
	_check( short )
	_check( long )
	_check( int )
	_check( float )
	_check( double )
	_check( wchar_t )
	_check( size_t )
	_check( int8_t )
	_check( uint8_t )
	_check( int16_t )
	_check( uint16_t )
	_check( int32_t )
	_check( uint32_t )
	_check( int64_t )
	_check( uint64_t )
	_check( intptr_t )
	_check( uintptr_t )
	_check( struct )
	_check( class )
	_check( virtual )
	_check( public )
	_check( private )
	_check( typedef )
	_check( alignas )

	return 0;
#undef _check
}

bool IsUserDef( parsertypedef_t *def )
{
	return ( def - g_pParserTypedefs >= g_nParserTypedefsUserDefIndex );
}

parsertypedef_t *Parser_GetTypedef( const SQChar *id, int len )
{
#ifndef SQ_DISABLE_TYPE_REDEFINITION
	for ( int i = 0; i < g_nParserTypedefsUserDefIndex; i++ )
	{
		parsertypedef_t *def = &g_pParserTypedefs[i];
		Assert( sq_type(def->name) == OT_STRING && _string(def->name) );
		if ( _string(def->name)->_len == len &&
				!memcmp( _string(def->name)->_val, id, len * sizeof(SQChar) ) )
			return def;
	}

	// Go backwards to check latest definitions first for multiple local typedefs
	for ( int i = g_nParserTypedefsIndex - 1; i >= g_nParserTypedefsUserDefIndex; i-- )
	{
		parsertypedef_t *def = &g_pParserTypedefs[i];
		Assert( sq_type(def->name) == OT_STRING && _string(def->name) );
		if ( _string(def->name)->_len == len &&
				!memcmp( _string(def->name)->_val, id, len * sizeof(SQChar) ) )
			return def;
	}
#else
	for ( int i = 0; i < g_nParserTypedefsIndex; i++ )
	{
		parsertypedef_t *def = &g_pParserTypedefs[i];
		Assert( sq_type(def->name) == OT_STRING && _string(def->name) );
		if ( _string(def->name)->_len == len &&
				!memcmp( _string(def->name)->_val, id, len * sizeof(SQChar) ) )
			return def;
	}
#endif
	return NULL;
}

void Parser_Typedef( HSQUIRRELVM vm, const SQChar *id, int len, const sqffitype_t &type )
{
	Assert( type.count > 0 );
	Assert( type.IsValid() );
	Assert( len > 0 );

	int size = g_nParserTypedefsCapacity;

	if ( g_nParserTypedefsIndex + 1 > size )
	{
		do
		{
			size += size / 2;
		}
		while ( g_nParserTypedefsIndex + 1 > size );

		g_pParserTypedefs = (parsertypedef_t*)realloc(
				(void*)g_pParserTypedefs,
				size * sizeof(*g_pParserTypedefs) );
		g_nParserTypedefsCapacity = size;
		memset( (void*)( g_pParserTypedefs + g_nParserTypedefsIndex ),
				0,
				( g_nParserTypedefsCapacity - g_nParserTypedefsIndex ) * sizeof(*g_pParserTypedefs) );
	}

	parsertypedef_t *def = &g_pParserTypedefs[ g_nParserTypedefsIndex++ ];

	HSQOBJECT tmp;
	sq_pushstring( vm, id, len );
	sq_getstackobj( vm, -1, &tmp );
	def->name = tmp;
	sq_pop( vm, 1 );

	*(sqffitype_t*)def = type;
	def->Normalise();
	Assert( def->IsValid() );
}

bool Parser_Typedef( HSQUIRRELVM vm, const SQChar **str )
{
#ifndef SQ_DISABLE_TYPE_REDEFINITION
	const SQChar *cached = *str;
#endif
	bool sameTypeRedef = false;
	parser_args_t args = {};

parse_def:
	Parser_ReadType( vm, str, &args );

	if ( args.type.id == -1 )
		return false;

	if ( args.namelen == 0 )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("expected typedef name @L%d:%d"), pidx.line, pidx.col );
		return false;
	}

	parsertypedef_t *def = Parser_GetTypedef( args.name, args.namelen );

	if ( def )
	{
#ifndef SQ_DISABLE_TYPE_REDEFINITION
		int idx = def - g_pParserTypedefs;
		Assert( idx < g_nParserTypedefsIndex );

		if ( idx < g_nParserTypedefsUserDefIndex )
#else
		// Allow redefinition to the same type
		sameTypeRedef = def->id == args.type.id &&
			def->isarray == args.type.isarray &&
			def->type.count == args.type.count &&
			_userdata(def->obj) == _userdata(args.type.obj);

		if ( !sameTypeRedef )
#endif
		{
			fileindex_t pidx = Parser_GetIndex( *str );
			throwerrorf( vm, _SC("redefinition of type " FMT_VSTR " @L%d:%d"),
					args.namelen, args.name, pidx.line, pidx.col );
			return false;
		}

#ifndef SQ_DISABLE_TYPE_REDEFINITION
		if ( args.type.id == FFIType_structdef )
			RemoveStructDef( vm, args.type.obj );

		def->Release();

		if ( g_nParserTypedefsIndex - ( idx + 1 ) > 0 )
		{
			memmove( (void*)( g_pParserTypedefs + idx ),
					g_pParserTypedefs + idx + 1,
					( g_nParserTypedefsIndex - ( idx + 1 ) ) * sizeof(*g_pParserTypedefs) );
		}

		g_nParserTypedefsIndex--;
		memset( (void*)( g_pParserTypedefs + g_nParserTypedefsIndex ), 0, sizeof(*g_pParserTypedefs) );

		*str = cached;
		return Parser_Typedef( vm, str );
#endif
	}

	if ( !sameTypeRedef )
	{
		if ( Parser_GetKeyword( args.name, args.namelen ) != 0 )
		{
			fileindex_t pidx = Parser_GetIndex( args.name );
			throwerrorf( vm, _SC("token '" FMT_VSTR "' is unexpected @L%d:%d"),
					args.namelen, args.name, pidx.line, pidx.col );
			return false;
		}

		Parser_Typedef( vm, args.name, args.namelen, args.type );
	}

	if ( Parser_Skip( str, ',' ) )
	{
		// Same type, new name
		args.has_type = 1;

		if ( args.tdef )
		{
			args.type = *args.tdef;
		}
		else
		{
			args.type.StripPtr();
		}

		goto parse_def;
	}
	else if ( !Parser_Skip( str, ';' ) )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("expected '%c' @L%d:%d"), ';', pidx.line, pidx.col );
		return false;
	}

	return true;
}

int Parser_Typedef2( HSQUIRRELVM vm, const SQChar **str )
{
	if ( Parser_Skip( str, _SC("typedef") ) )
	{
		if ( !Parser_Typedef( vm, str ) )
			return -1;

		return 0;
	}

	bool isUnion = false;

	// Allow struct definition without typedef
	if ( Parser_Skip( str, _SC("struct") ) != 0 ||
			( isUnion = (bool)Parser_Skip( str, _SC("union") ) ) != 0 )
	{
		int namelen;
		const SQChar *name = Parser_ReadIdentifier( str, &namelen );
		int align = 0;
		int pack = 0;

		if ( !namelen )
		{
			fileindex_t pidx = Parser_GetIndex( *str );
			throwerrorf( vm, _SC("expected struct name @L%d:%d"), pidx.line, pidx.col );
			return -1;
		}

		for (;;)
		{
			if ( STRCMP( name, namelen, _SC("alignas") ) )
			{
				align = Parser_ReadAlignas( vm, str );

				if ( align == -1 )
					return -1;

				name = Parser_ReadIdentifier( str, &namelen );
			}
			// #pragma pack
			else if ( STRCMP( name, namelen, _SC("pack") ) )
			{
				pack = Parser_ReadStructPack( vm, str );

				if ( pack == -1 )
					return -1;

				name = Parser_ReadIdentifier( str, &namelen );
			}
			else break;
		}

		SQObjectPtr hDef;

		if ( !DefineStruct( vm, str, name, namelen, align, pack, isUnion, hDef ) )
			return -1;

		if ( !Parser_Skip( str, ';' ) )
		{
			fileindex_t pidx = Parser_GetIndex( *str );
			throwerrorf( vm, _SC("expected '%c' @L%d:%d"), ';', pidx.line, pidx.col );
			return -1;
		}

		return 0;
	}

	return 1;
}

void Parser_Typedef_Skip( HSQUIRRELVM vm, const SQChar **str )
{
	parser_args_t args = {};
	bool _g_bCreateFunc = g_bCreateFunc;
	g_bCreateFunc = false;
	Parser_ReadType( vm, str, &args );
	g_bCreateFunc = _g_bCreateFunc;
	Parser_Skip( str, ';' );
}

void Parser_Typedef_Restore( int cache )
{
	for ( int i = cache; i < g_nParserTypedefsIndex; i++ )
	{
		parsertypedef_t &def = g_pParserTypedefs[i];
		def.Release();
	}

	g_nParserTypedefsIndex = cache;
}

int Parser_ReadAttribute( HSQUIRRELVM vm, const SQChar **str )
{
	int flags = 0;
	Assert( (*str)[0] == '[' && (*str)[1] == '[' );

	if ( (*str)[2] == 's' && (*str)[3] == 'q' && (*str)[4] == ':' && (*str)[5] == ':' )
	{
		*str += 6;

		int len;
		const SQChar *id = Parser_PeekIdentifier( *str, &len );
		*str += len;

		if ( STRCMP( id, len, _SC("errno") ) )
		{
			flags = FFIFlag_ERRNO;
		}
		else if ( STRCMP( id, len, _SC("GetLastError") ) )
		{
			flags = FFIFlag_GETLASTERROR;
		}
		else
		{
			goto unkn_attr;
		}
	}
	else
	{
unkn_attr:
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("unknown attribute @L%d:%d"), pidx.line, pidx.col );
		return -1;
	}

	if ( (*str)[0] == ']' && (*str)[1] == ']' )
	{
		*str += 2;
		Parser_SkipWhitespace( str );
		return flags;
	}
	else
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("expected ']]' @L%d:%d"), pidx.line, pidx.col );
		return -1;
	}
}

int Parser_ReadAlignas( HSQUIRRELVM vm, const SQChar **str )
{
	int value = 0;

	if ( !Parser_Skip( str, '(' ) )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("expected '%c' @L%d:%d"), '(', pidx.line, pidx.col );
		return -1;
	}

	if ( _isdigit( **str ) )
	{
		const SQChar *pStart = *str;
		do { (*str)++; } while ( _isdigit( **str ) );
		value = atoi( pStart, *str - pStart );
	}
	else
	{
		parser_args_t args = {};
		bool _g_bCreateFunc = g_bCreateFunc;
		g_bCreateFunc = false;
		Parser_ReadType( vm, str, &args );
		g_bCreateFunc = _g_bCreateFunc;

		if ( args.type.id == -1 )
			return -1;

		value = args.type.ElementSize();
	}

	if ( !Parser_Skip( str, ')' ) )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("expected '%c' @L%d:%d"), ')', pidx.line, pidx.col );
		return -1;
	}

	if ( ( value & ( value - 1 ) ) != 0 )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("alignment must be a power of 2 @L%d:%d"), pidx.line, pidx.col );
		return -1;
	}

	if ( value < 0 )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("invalid alignment size @L%d:%d"), pidx.line, pidx.col );
		return -1;
	}

	Parser_SkipWhitespace( str );
	return value;
}

int Parser_ReadStructPack( HSQUIRRELVM vm, const SQChar **str )
{
	int value = 0;

	if ( !Parser_Skip( str, '(' ) )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("expected '%c' @L%d:%d"), '(', pidx.line, pidx.col );
		return -1;
	}

	if ( !_isdigit( **str ) )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("expected digit @L%d:%d"), pidx.line, pidx.col );
		return -1;
	}

	const SQChar *pStart = *str;
	do { (*str)++; } while ( _isdigit( **str ) );
	value = atoi( pStart, *str - pStart );

	if ( !Parser_Skip( str, ')' ) )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("expected '%c' @L%d:%d"), ')', pidx.line, pidx.col );
		return -1;
	}

	if ( ( value & ( value - 1 ) ) != 0 )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("packing alignment must be a power of 2 @L%d:%d"), pidx.line, pidx.col );
		return -1;
	}

	if ( value < 1 || value > 16 )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("invalid packing size @L%d:%d"), pidx.line, pidx.col );
		return -1;
	}

	Parser_SkipWhitespace( str );
	return value;
}

void Parser_ReadType( HSQUIRRELVM vm, const SQChar **str, parser_args_t *args )
{
	STACKCHECK( vm );
	const SQChar *id;
	int len;
	int mod_ptr = 0;
	int mod_varptr = 0;
	int mod_bracketptr = 0;
	char mod_const = 0;
	char mod_unsigned = 0;
	char mod_call = 0;
	char isfunc = 0;
	bool isUnion = 0;
	int funcflags = 0;
	int elemcount = 1;
	int name_paren;
	parsertypedef_t *tdef;
	sqffitype_t ret = {};
	ret.id = -1;

	// For error reporting
	int structnamelen = 0;
	const SQChar *structname = 0;

	int unknnamelen = 0;
	const SQChar *unknname = 0;
	const SQChar *prev = 0;

	Parser_SkipWhitespace( str );

	if ( !args->has_type )
	{
		// Attributes are only used for function flags for the moment
		if ( (*str)[0] == '[' && (*str)[1] == '[' )
		{
			funcflags = Parser_ReadAttribute( vm, str );
			if ( funcflags == -1 )
				goto fail;
		}

		mod_const = Parser_Skip( str, _SC("const") );
		Parser_SkipWhitespace( str );

		id = Parser_ReadIdentifier( str, &len );

		// Sign specifiers are only supported in the beginning of the type
		// NOT allowed: long int unsigned long
		// allowed: unsigned long long int
		if ( STRCMP( id, len, _SC("unsigned") ) )
		{
			// Lazy hack to make 'unsigned' without 'int' work
			prev = *str;
			id = Parser_ReadIdentifier( str, &len );
			mod_unsigned = 1;
		}
		else if ( STRCMP( id, len, _SC("signed") ) )
		{
			prev = *str;
			id = Parser_ReadIdentifier( str, &len );
			mod_unsigned = -1;
		}

		if ( STRCMP( id, len, _SC("char") ) )
		{
			ret.id = mod_unsigned == 1 ? CIntToFFI< unsigned char >() : CIntToFFI< char >();
		}
		// short int
		else if ( STRCMP( id, len, _SC("short") ) )
		{
			Parser_Skip( str, _SC("int") );
			ret.id = mod_unsigned == 1 ? CIntToFFI< unsigned short >() : CIntToFFI< short >();
		}
		// long long int
		else if ( STRCMP( id, len, _SC("long") ) )
		{
			ret.id = mod_unsigned == 1 ? CIntToFFI< unsigned long >() : CIntToFFI< long >();

			if ( Parser_Skip( str, _SC("long") ) )
				ret.id = mod_unsigned == 1 ? CIntToFFI< unsigned long long >() : CIntToFFI< long long >();

			Parser_Skip( str, _SC("int") );
		}
		// int long long
		else if ( STRCMP( id, len, _SC("int") ) )
		{
			ret.id = mod_unsigned == 1 ? CIntToFFI< unsigned int >() : CIntToFFI< int >();

			if ( Parser_Skip( str, _SC("short") ) )
			{
				ret.id = mod_unsigned == 1 ? CIntToFFI< unsigned short >() : CIntToFFI< short >();
			}
			else if ( Parser_Skip( str, _SC("long") ) )
			{
				ret.id = mod_unsigned == 1 ? CIntToFFI< unsigned long >() : CIntToFFI< long >();

				if ( Parser_Skip( str, _SC("long") ) )
					ret.id = mod_unsigned == 1 ? CIntToFFI< unsigned long long >() : CIntToFFI< long long >();
			}
		}
		else if ( STRCMP( id, len, _SC("struct") ) || ( isUnion = STRCMP( id, len, _SC("union") ) ) != 0 )
		{
			if ( mod_unsigned != 0 )
			{
				fileindex_t pidx = Parser_GetIndex( id );
				throwerrorf( vm, _SC("'unsigned' cannot be used with type '" FMT_STR "' on arg %d @L%d:%d"),
						_SC("struct"), args->nargs + 1, pidx.line, pidx.col );
				goto fail;
			}

			int align = 0;
			int pack = 0;

			// Get struct name
			id = Parser_ReadIdentifier( str, &len );

			for (;;)
			{
				if ( STRCMP( id, len, _SC("alignas") ) )
				{
					align = Parser_ReadAlignas( vm, str );

					if ( align == -1 )
						goto fail;

					id = Parser_ReadIdentifier( str, &len );
				}
				// #pragma pack
				else if ( STRCMP( id, len, _SC("pack") ) )
				{
					pack = Parser_ReadStructPack( vm, str );

					if ( pack == -1 )
						goto fail;

					id = Parser_ReadIdentifier( str, &len );
				}
				else break;
			}

			structname = id;
			structnamelen = len;

			// Define
			if ( **str == '{' )
			{
				if ( !DefineStruct( vm, str, id, len, align, pack, isUnion, ret.obj ) )
					goto fail;

				ret.id = FFIType_structdef;
			}
			// Reference
			else if ( len )
			{
				if ( align )
					errorfunc( vm, _SC("Warning: 'alignas' on struct " FMT_VSTR " declaration\n"), len, id );

				FindStructDef( vm, id, len, ret.obj );
				ret.id = FFIType_struct;
			}
			else
			{
				fileindex_t pidx = Parser_GetIndex( id );
				throwerrorf( vm, _SC("expected struct name @L%d:%d"), pidx.line, pidx.col );
				goto fail;
			}
		}
		else if ( ( tdef = Parser_GetTypedef( id, len ) ) != NULL )
		{
			if ( mod_unsigned != 0 )
			{
				fileindex_t pidx = Parser_GetIndex( id );
				throwerrorf( vm, _SC("'unsigned' cannot be used with type '" FMT_VSTR "' on arg %d @L%d:%d"),
						len, id, args->nargs + 1, pidx.line, pidx.col );
				goto fail;
			}

			if ( IsUserDef( tdef ) )
				args->tdef = tdef;

			ret = *tdef;

			if ( ret.id == FFIType_struct )
			{
				script_struct_def_t *def = ret.StructDef();
				structname = _string(def->name)->_val;
				structnamelen = _string(def->name)->_len;
			}
			else if ( ret.id == FFIType_fn )
			{
				ret.id = FFIType_fndef;
			}
		}
		else if ( mod_unsigned == 1 )
		{
			ret.id = CIntToFFI< unsigned >();
			*str = prev;
		}
		else if ( mod_unsigned == -1 )
		{
			ret.id = CIntToFFI< signed >();
			*str = prev;
		}
		else
		{
			unknname = id;
			unknnamelen = len;
		}
	}
	else
	{
		args->has_type = 0;
		ret = args->type;
	}

	// int const a
	if ( !mod_const )
		mod_const = Parser_Skip( str, _SC("const") );

	while ( Parser_Skip( str, '*' ) )
	{
		// const int * const a
		// int const * const a
		mod_const = Parser_Skip( str, _SC("const") ) | mod_const;
		mod_ptr++;
	}

	// void fn()
	// void (fn)()
	// void __cdecl fn()
	// void (__cdecl fn)()
	// void (*)()
	// void (*fn)()
	// void (__cdecl *fn)()
	name_paren = Parser_Skip( str, '(' );

	// name or modifier
	id = Parser_ReadIdentifier( str, &len );

	if ( len )
	{
		if ( STRCMP( id, len, _SC("__cdecl") ) )
		{
#if defined(_WIN32) && !defined(_WIN64)
			mod_call = FFI_MS_CDECL;
#else
			mod_call = -1;
#endif
		}
		else if ( STRCMP( id, len, _SC("__stdcall") ) )
		{
#if defined(_WIN32) && !defined(_WIN64)
			mod_call = FFI_STDCALL;
#else
			mod_call = -1;
#endif
		}
		else if ( STRCMP( id, len, _SC("__fastcall") ) )
		{
#if defined(_WIN32) && !defined(_WIN64)
			mod_call = FFI_FASTCALL;
#else
			mod_call = -1;
#endif
		}
		else if ( STRCMP( id, len, _SC("__thiscall") ) )
		{
#if defined(_WIN32) && !defined(_WIN64)
			mod_call = FFI_THISCALL;
#else
			mod_call = -1;
#endif
		}
#ifdef _WIN32
		else if ( STRCMP( id, len, _SC("CALLBACK") ) || STRCMP( id, len, _SC("WINAPI") ) )
		{
#if defined(_WIN32) && !defined(_WIN64)
			mod_call = FFI_STDCALL;
#else
			mod_call = -1;
#endif
		}
#endif
	}

	if ( name_paren )
	{
		while ( Parser_Skip( str, '*' ) )
		{
			mod_const = Parser_Skip( str, _SC("const") ) | mod_const;
			mod_varptr++;
		}
	}

	// Have modifier, get name
	if ( mod_call || mod_varptr )
		id = Parser_ReadIdentifier( str, &len );

	if ( Parser_GetKeyword( id, len ) != 0 )
	{
		fileindex_t pidx = Parser_GetIndex( id );
		throwerrorf( vm, _SC("token '" FMT_VSTR "' is unexpected @L%d:%d"), len, id, pidx.line, pidx.col );
		goto fail;
	}

	args->name = id;
	args->namelen = len;

	if ( name_paren && Parser_Skip( str, ')' ) )
		name_paren = 0;

	// int []
	// int a[]
	// int (a)[]
	// int (a[])
	if ( Parser_Skip( str, '[' ) )
	{
		do
		{
			const SQChar *pStart = *str;

			for (;;)
			{
				if ( !**str )
					break;

				if ( _isdigit( **str ) )
				{
					(*str)++;
					continue;
				}

				len = *str - pStart;
				Parser_SkipWhitespace( str );

				if ( **str == ']' )
				{
					(*str)++;

					if ( len )
					{
						// Just multiply all
						// sizeof( int[4][32] ) == 512
						elemcount *= atoi( pStart, len );

						if ( elemcount <= 0 )
						{
							fileindex_t pidx = Parser_GetIndex( pStart );
							throwerrorf( vm, _SC("invalid count @L%d:%d"), pidx.line, pidx.col );
							goto fail;
						}
					}
					// else treat zero sized arrays as 1 for simplicity
				}
				else
				{
					fileindex_t pidx = Parser_GetIndex( *str );
					throwerrorf( vm, _SC("expected '%c' @L%d:%d"), ']', pidx.line, pidx.col );
					goto fail;
				}

				break;
			}

			Parser_SkipWhitespace( str );
			mod_bracketptr++;
		}
		while ( Parser_Skip( str, '[' ) );
	}

	if ( name_paren && !Parser_Skip( str, ')' ) )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("expected '%c' @L%d:%d"), ')', pidx.line, pidx.col );
		goto fail;
	}

	// int fn()
	// int (fn)()
	// int (*)()
	// int (*fn)()
	if ( **str == '(' )
	{
		if ( !( len != 0 && mod_varptr <= 1 ) && !( len == 0 && mod_varptr == 1 ) )
		{
			fileindex_t pidx = Parser_GetIndex( *str );
			throwerrorf( vm, _SC("expected function type @L%d:%d"), pidx.line, pidx.col );
			goto fail;
		}

		// int (*fn[])()
		if ( mod_bracketptr != 0 )
		{
			fileindex_t pidx = Parser_GetIndex( *str );
			throwerrorf( vm, _SC("unsupported syntax @L%d:%d"), pidx.line, pidx.col );
			goto fail;
		}

		isfunc = 1;
	}
	// int *(*(a[])[])[]
	else if ( **str == '[' )
	{
		fileindex_t pidx = Parser_GetIndex( *str );
		throwerrorf( vm, _SC("unsupported syntax @L%d:%d"), pidx.line, pidx.col );
		goto fail;
	}

	// int  (*a[])
	// int *( a[])
	// int *(*a[])
	if ( name_paren && !isfunc )
	{
		mod_ptr += mod_varptr;
		mod_varptr = 0;
	}

	if ( mod_bracketptr )
	{
		if ( mod_varptr )
		{
			// int  (*a)[]
			// int *(*a)[]
			if ( mod_varptr == 1 && ret.id != -1 )
			{
				ret.id |= FFIType_ptr;
			}
			// int  (**a)[]
			// int *(**a)[]
			else
			{
				ret.id = FFIType_ptr;
			}

			elemcount = 1;
		}
		// int  a[]
		// int *a[]
		else
		{
			ret.isarray = 1;
		}
	}

	if ( mod_ptr == 1 && ret.id != -1 && !( ret.id & FFIType_ptr ) )
	{
		ret.id |= FFIType_ptr;
	}
	else if ( mod_ptr )
	{
		ret.id = FFIType_ptr;
	}

	ret.count = !ret.count ? elemcount : ret.count * elemcount;
	Assert( elemcount >= 1 );
	Assert( ret.count >= 1 );

	if ( ret.id == -1 )
	{
		fileindex_t pidx = Parser_GetIndex( unknname );
		if ( unknnamelen )
		{
			throwerrorf( vm, _SC("unknown type '" FMT_VSTR "' on arg %d @L%d:%d"),
					unknnamelen, unknname, args->nargs + 1, pidx.line, pidx.col );
		}
		else
		{
			throwerrorf( vm, _SC("expected typename @L%d:%d"), pidx.line, pidx.col );
		}

		goto fail;
	}
	else if ( ret.id == FFIType_struct && ( sq_type(ret.obj) == 0 || sq_type(ret.obj) == OT_NULL ) )
	{
		fileindex_t pidx = Parser_GetIndex( structname );
		throwerrorf( vm, _SC("struct '" FMT_VSTR "' is not defined @L%d:%d"),
				structnamelen, structname, pidx.line, pidx.col );
		goto fail;
	}

	if ( isfunc )
	{
		if ( ret.isarray )
		{
			fileindex_t pidx = Parser_GetIndex( args->name );
			throwerrorf( vm, _SC("function returns array @L%d:%d"), pidx.line, pidx.col );
			goto fail;
		}

		if ( mod_call > 0 )
		{
#if !( defined(_WIN32) && !defined(_WIN64) )
			args->abi = FFI_DEFAULT_ABI;
#else
			args->abi = mod_call;
#endif
		}
		else
		{
			args->abi = FFI_DEFAULT_ABI;
		}

		args->flags = funcflags;
		args->rettype = ret;

		if ( mod_varptr <= 1 && !Parser_ReadFuncArgs( vm, str, args ) )
			goto fail;

		// int (*fn)()[]
		if ( **str == '[' )
		{
			fileindex_t pidx = Parser_GetIndex( args->name );
			throwerrorf( vm, _SC("function returns array @L%d:%d"), pidx.line, pidx.col );
			goto fail;
		}

		ret = {};
		ret.count = 1;
		ret.obj = args->type.obj;

		// function declaration
		if ( mod_varptr == 0 )
		{
			ret.id = FFIType_fndec;
		}
		// function pointer
		else if ( mod_varptr == 1 )
		{
			ret.id = FFIType_fn;
		}
		// pointer to function pointer
		else
		{
			ret.id = FFIType_ptr | FFIType_fn;
		}
	}
	else if ( funcflags )
	{
		fileindex_t pidx = Parser_GetIndex( args->name );
		throwerrorf( vm, _SC("attributes are unexpected for type @L%d:%d"), pidx.line, pidx.col );
		goto fail;
	}

	Assert( !g_bCreateFunc || ret.IsValid() );
	args->type = ret;
	return;

fail:
	args->type = {};
	args->type.id = -1;
	return;
}

bool Parser_ReadFuncArgs( HSQUIRRELVM vm, const SQChar **str, parser_args_t *args )
{
	STACKCHECK( vm );
	int stackindex = g_nParserStackIndex;
	parser_args_t subargs = {};

	Assert( **str == '(' );
	Assert( sq_type(args->type.obj) == 0 || sq_type(args->type.obj) == OT_NULL );
	Assert( args->nargs == 0 );
	Assert( args->rettype.id >= FFIType_void );

	Parser_Skip( str, '(' );

	if ( g_nParserStackIndex + 1 > g_nParserStackCapacity )
	{
		sq_throwerror( vm, _SC("too many arguments") );
		return false;
	}

	g_pParserStack[ g_nParserStackIndex++ ] = args->rettype;

	if ( args->has_thisptr )
	{
		if ( g_nParserStackIndex + 1 > g_nParserStackCapacity )
		{
			sq_throwerror( vm, _SC("too many arguments") );
			return false;
		}

		g_pParserStack[ g_nParserStackIndex++ ].id = FFIType_ptr;
		args->nargs++;
	}

	if ( Parser_Skip( str, ')' ) )
		goto end;

	for (;;)
	{
		if ( (*str)[0] == '.' && (*str)[1] == '.' && (*str)[2] == '.' )
		{
			*str += 3;
			Parser_SkipWhitespace( str );

			if ( **str != ')' )
			{
				sq_throwerror( vm, _SC("variadic must be the last argument") );
				goto fail;
			}

			subargs.type.id = FFIType_var;
		}
		else
		{
			subargs = {};
			Parser_ReadType( vm, str, &subargs );

			if ( subargs.type.id == -1 )
				goto fail;
		}

		if ( subargs.type.id != FFIType_void )
		{
			if ( g_nParserStackIndex + 1 > g_nParserStackCapacity )
			{
				sq_throwerror( vm, _SC("too many arguments") );
				goto fail;
			}

			g_pParserStack[ g_nParserStackIndex++ ] = subargs.type;
			args->nargs++;
		}

		if ( Parser_Skip( str, ',' ) )
		{
			if ( subargs.type.id == FFIType_void )
			{
				sq_throwerror( vm, _SC("'void' cannot be used as a function parameter") );
				goto fail;
			}

			continue;
		}

		if ( Parser_Skip( str, ')' ) )
		{
end:
			// Function pointer needs to have a prepared CIF to be used to
			// create the trampoline (ffi closure)
			// Since CIF can only be prepared when argument types are known,
			// and variadic arguments on callback function pointers are
			// unknown when they are used in script,
			// variadic callbacks are not supported

			if ( g_bCreateFunc && !DoMakeFunction( vm, stackindex, args ) )
				goto fail;

			for ( int i = stackindex; i < g_nParserStackIndex; i++ )
				g_pParserStack[i].Release();

			g_nParserStackIndex = stackindex;
			return true;
		}

		throwerrorf( vm, _SC("expected ',' or ')' on arg %d"), args->nargs + 1 );
		goto fail;
	}

fail:
	for ( int i = stackindex; i < g_nParserStackIndex; i++ )
		g_pParserStack[i].Release();

	g_nParserStackIndex = stackindex;
	return false;
}

bool InitParserStack( HSQUIRRELVM vm, int nargs, int retBase, int argsBase )
{
	if ( nargs + 1 > g_nParserStackCapacity )
	{
		sq_throwerror( vm, _SC("too many arguments") );
		return false;
	}

	SQInteger typid;

	if ( SQ_FAILED( sq_getinteger( vm, retBase, &typid ) ) )
	{
		sq_throwerror( vm, _SC("invalid return type") );
		return false;
	}

	g_pParserStack[0].id = typid;

	for ( int i = 0; i < nargs; i++ )
	{
		if ( SQ_FAILED( sq_getinteger( vm, argsBase + i, &typid ) ) )
		{
			throwerrorf( vm, _SC("invalid parameter %d type"), i );
			return false;
		}

		g_pParserStack[ 1 + i ].id = typid;
	}

	return true;
}

SQInteger ffi_GetLastError( HSQUIRRELVM vm )
{
	sq_pushinteger( vm, g_ffi_errno );
	return 1;
}

void RegisterFFI( HSQUIRRELVM vm, vm_init_options opt )
{
	(void)opt;
	STACKCHECK( vm );

	sq_pushstring( vm, _SC("ffi"), STRLEN("ffi") );
	sq_newtable( vm );
	{
		push_func( vm, "_tostring", ffi_MT_ToString, 1, "u" );
		push_func( vm, "_call", ffi_MT_Call, -1, "u" );
		push_func( vm, "_add", ffi_MT_Add, 2, "ui" );
		push_func( vm, "_sub", ffi_MT_Sub, 2, "ui" );
		push_func( vm, "_get", ffi_MT_Get, 2, "us|i" );
		push_func( vm, "_set", ffi_MT_Set, 3, "us|iu|p|i|f|c" );

		push_func( vm, "malloc", ffi_malloc, 2, ".i|s" );
		push_func( vm, "get", ffi_get, 3, ".i|su|p|i" );
		push_func( vm, "set", ffi_set, 4, ".i|su|p|iu|p|i|f" );
		push_func( vm, "type", ffi_typeof, 2, ".s" );
		push_func( vm, "sizeof", ffi_sizeof, 2, ".s|i|u" );
		push_func( vm, "alignof", ffi_alignof, 2, ".s|i|u" );
		push_func( vm, "offsetof", ffi_offsetof, -2, ".u|ss|i" );
		push_func( vm, "typedef", ffi_typedef, 2, ".s" );

		push_func( vm, "LoadLibrary", ffi_LoadLibrary, 2, ".s" );
		push_func( vm, "GetFunction", ffi_GetFunction, -3, ".us|i" );
		push_func( vm, "MakeFunction", ffi_MakeFunction, -3, ".iu|p|i" );
		push_func( vm, "GetFunctions", ffi_GetFunctions, 4, ".t|yus" );
		push_func( vm, "GetVirtualClass", ffi_GetVirtualClass, 4, ".t|yu|p|is" );
		push_func( vm, "GetLastError", ffi_GetLastError, 1, "." );
		push_func( vm, "MakeArray", ffi_MakeArray, -3, ".iu|p|ii" );
#ifdef FFI_CLOSURES
		push_func( vm, "MakeClosure", ffi_MakeClosure, -3, ".u|scv" );
		push_func( vm, "Clone", ffi_Clone, 2, ".u|c" );
#endif

		push_int( vm, "t_void", FFIType_void );
		push_int( vm, "t_i8", FFIType_i8 );
		push_int( vm, "t_u8", FFIType_u8 );
		push_int( vm, "t_i16", FFIType_i16 );
		push_int( vm, "t_u16", FFIType_u16 );
		push_int( vm, "t_i32", FFIType_i32 );
		push_int( vm, "t_u32", FFIType_u32 );
		push_int( vm, "t_i64", FFIType_i64 );
		push_int( vm, "t_u64", FFIType_u64 );
		push_int( vm, "t_f32", FFIType_f32 );
		push_int( vm, "t_f64", FFIType_f64 );
		push_int( vm, "t_var", FFIType_var );
		push_int( vm, "t_ptr", FFIType_ptr );
		push_int( vm, "t_str", FFIType_str );
		push_int( vm, "t_wstr", FFIType_wstr );

		push_int( vm, "t_bool", CIntToFFI< bool >() );
		push_int( vm, "t_char", CIntToFFI< char >() );
		push_int( vm, "t_uchar", CIntToFFI< unsigned char >() );
		push_int( vm, "t_wchar", CIntToFFI< wchar_t >() );
		push_int( vm, "t_short", CIntToFFI< short >() );
		push_int( vm, "t_ushort", CIntToFFI< unsigned short >() );
		push_int( vm, "t_int", CIntToFFI< int >() );
		push_int( vm, "t_uint", CIntToFFI< unsigned int >() );
		push_int( vm, "t_long", CIntToFFI< long >() );
		push_int( vm, "t_ulong", CIntToFFI< unsigned long >() );
		push_int( vm, "t_longlong", CIntToFFI< long long >() );
		push_int( vm, "t_ulonglong", CIntToFFI< unsigned long long >() );
		push_int( vm, "t_intptr", CIntToFFI< intptr_t >() );
		push_int( vm, "t_uintptr", CIntToFFI< uintptr_t >() );
		push_int( vm, "t_float", FFIType_f32 );
		push_int( vm, "t_double", FFIType_f64 );
		push_int( vm, "t_sqchar", CIntToFFI< SQChar >() );

		if ( sizeof(SQChar) == sizeof(char) )
		{
			push_int( vm, "t_sqstr", FFIType_str );
		}
		else
		{
			push_int( vm, "t_sqstr", FFIType_wstr );
		}
	}

	// Keep ref in registry for metamethods
	sq_pushregistrytable( vm );
	sq_push( vm, -3 );
	sq_push( vm, -3 );
	sq_newslot( vm, -3, SQFalse );
	sq_pop( vm, 1 );
	sq_newslot( vm, -3, SQFalse );

	sq_pushregistrytable( vm );
	sq_pushstring( vm, _SC("ffi_structs"), STRLEN("ffi_structs") );
	sq_newarray( vm, 0 );
	sq_newslot( vm, -3, SQFalse );
	sq_pop( vm, 1 );

#ifdef FFI_CLOSURES
	sq_pushregistrytable( vm );
	sq_pushstring( vm, _SC("ffi_closures"), STRLEN("ffi_closures") );
	sq_newarray( vm, 0 );
	sq_newslot( vm, -3, SQFalse );
	sq_pop( vm, 1 );
#endif

	Assert( !g_pParserTypedefs );
	g_nParserTypedefsCapacity = 256;
	g_pParserTypedefs = (parsertypedef_t*)malloc( g_nParserTypedefsCapacity * sizeof(*g_pParserTypedefs) );
	memset( (void*)g_pParserTypedefs, 0, g_nParserTypedefsCapacity * sizeof(*g_pParserTypedefs) );

	Assert( !g_pParserStack );
	g_pParserStack = (sqffitype_t*)malloc( g_nParserStackCapacity * sizeof(*g_pParserStack) );
	memset( (void*)g_pParserStack, 0, g_nParserStackCapacity * sizeof(*g_pParserStack) );

	sqffitype_t type = {};
	type.count = 1;

#define _check( ctype, ffitype ) \
	type.id = FFIType_##ffitype; \
	Parser_Typedef( vm, _SC(#ctype), STRLEN(#ctype), type )

#define _check_int( ctype ) \
	type.id = CIntToFFI< ctype >(); \
	Parser_Typedef( vm, _SC(#ctype), STRLEN(#ctype), type )

	_check( void, void );
	_check_int( bool );
	_check( float, f32 );
	_check( double, f64 );
	_check_int( wchar_t );
	_check_int( size_t );
	_check_int( int8_t );
	_check_int( uint8_t );
	_check_int( int16_t );
	_check_int( uint16_t );
	_check_int( int32_t );
	_check_int( uint32_t );
	_check_int( int64_t );
	_check_int( uint64_t );
	_check_int( intptr_t );
	_check_int( uintptr_t );
	_check_int( time_t );

#ifdef _WIN32
	if ( opt.windef )
	{
		_check_int( BOOL );
		_check_int( BOOLEAN );

		_check_int( BYTE );
		_check_int( TBYTE );

		_check_int( CHAR );
		_check_int( UCHAR );
		_check_int( TCHAR );
		_check_int( WCHAR );

		_check_int( DWORD );
		_check_int( DWORD32 );
		_check_int( DWORD64 );
		_check_int( DWORD_PTR );

		_check( FLOAT, f32 );

		_check_int( INT );
		_check_int( INT_PTR );
		_check_int( INT8 );
		_check_int( INT16 );
		_check_int( INT32 );
		_check_int( INT64 );

		_check_int( UINT );
		_check_int( UINT_PTR );
		_check_int( UINT8 );
		_check_int( UINT16 );
		_check_int( UINT32 );
		_check_int( UINT64 );

		_check_int( LONG );
#ifndef _M_IX86
		_check_int( LONGLONG );
		_check_int( ULONGLONG );
#else
		_check( LONGLONG, f64 );
		_check( ULONGLONG, f64 );
#endif
		_check_int( LONG_PTR );
		_check_int( LONG32 );
		_check_int( LONG64 );
		_check_int( ULONG );
		_check_int( ULONG_PTR );
		_check_int( ULONG32 );
		_check_int( ULONG64 );

		_check_int( WORD );
		_check_int( SHORT );
		_check_int( USHORT );

		_check_int( SIZE_T );
		_check_int( SSIZE_T );

		_check( LPBOOL, ptr );
		_check( LPBYTE, ptr );
		_check( LPCSTR, str );
		_check( LPCWSTR, wstr );
		_check( LPCTSTR, tstr );
		_check( LPCVOID, ptr );
		_check( LPDWORD, ptr );
		_check( LPHANDLE, ptr );
		_check( LPINT, ptr );
		_check( LPLONG, ptr );
		_check( LPSTR, str );
		_check( LPWSTR, wstr );
		_check( LPTSTR, tstr );
		_check( LPVOID, ptr );

		_check( PBOOL, ptr );
		_check( PBYTE, ptr );
		_check( PCSTR, str );
		_check( PCWSTR, wstr );
		_check( PCTSTR, tstr );
		_check( PCVOID, ptr );
		_check( PDWORD, ptr );
		_check( PHANDLE, ptr );
		_check( PINT, ptr );
		_check( PLONG, ptr );
		_check( PSTR, str );
		_check( PWSTR, wstr );
		_check( PTSTR, tstr );

		_check( PFLOAT, ptr );
		_check( PHANDLE, ptr );
		_check( PINT, ptr );
		_check( PINT_PTR, ptr );
		_check( PINT8, ptr );
		_check( PINT16, ptr );
		_check( PINT32, ptr );
		_check( PINT64, ptr );
		_check( PLONG, ptr );
		_check( PLONGLONG, ptr );
		_check( PLONG_PTR, ptr );
		_check( PLONG32, ptr );
		_check( PLONG64, ptr );
		_check( PSIZE_T, ptr );
		_check( PSSIZE_T, ptr );
		_check( PVOID, ptr );

		_check( HANDLE, ptr );
		_check_int( HRESULT );
		_check_int( HFILE );
		_check_int( LRESULT );
		_check_int( LPARAM );
		_check_int( WPARAM );
	}
#endif // _WIN32

#undef _check
#undef _check_int

#ifndef SQ_DISABLE_TYPE_REDEFINITION
	g_nParserTypedefsUserDefIndex = g_nParserTypedefsIndex;
#endif
}
#endif


////////////////////////////////////////////////

void dummyprintfunc( HSQUIRRELVM, const SQChar *, ... )
{
}

SQRESULT throwerrorf( HSQUIRRELVM vm, const SQChar *fmt, ... )
{
	SQChar buf[256];
	va_list va;
	va_start( va, fmt );
	int len = scvsprintf( buf, sizeof(buf) / sizeof(SQChar), fmt, va );
	va_end( va );

	if ( len < 0 || len > (int)sizeof(buf)-1 )
	{
		len = sizeof(buf)-1;
#if defined(_MSC_VER) && _MSC_VER < 1900
		buf[len] = 0;
#endif
	}

	return sq_throwerror( vm, buf );
}

void printfunc( HSQUIRRELVM, const SQChar *fmt, ... )
{
#ifdef SQUNICODE
	SQChar buf[4096];
	va_list va;
	va_start( va, fmt );
	int len = scvsprintf( buf, sizeof(buf) / sizeof(SQChar), fmt, va );
	va_end( va );

	if ( len < 0 || len > (int)sizeof(buf)-1 )
	{
		len = sizeof(buf)-1;
#if defined(_MSC_VER) && _MSC_VER < 1900
		buf[len] = 0;
#endif
	}

	PrintWCharToUTF8( g_StreamOut, buf, len );
#else
	va_list va;
	va_start( va, fmt );
	scvprintf( g_StreamOut, fmt, va );
	va_end( va );
#endif
}

void errorfunc( HSQUIRRELVM, const SQChar *fmt, ... )
{
	if ( g_bVT )
		fputs( TERM_CLR_FG_RED, stderr );

#ifdef SQUNICODE
	SQChar buf[4096];
	va_list va;
	va_start( va, fmt );
	int len = scvsprintf( buf, sizeof(buf) / sizeof(SQChar), fmt, va );
	va_end( va );

	if ( len < 0 || len > (int)sizeof(buf)-1 )
	{
		len = sizeof(buf)-1;
#if defined(_MSC_VER) && _MSC_VER < 1900
		buf[len] = 0;
#endif
	}

	PrintWCharToUTF8( stderr, buf, len );
#else
	va_list va;
	va_start( va, fmt );
	scvprintf( stderr, fmt, va );
	va_end( va );
#endif

	if ( g_bVT )
		fputs( TERM_CLR_FG_RESTORE, stderr );
}

#ifdef _DEBUG
SQInteger sq_getregistrytable( HSQUIRRELVM vm )
{
	sq_pushregistrytable( vm );
	return 1;
}
#endif

SQInteger sq_printl( HSQUIRRELVM vm )
{
	bool _g_bTableIndent = g_bTableIndent;
	g_bTableIndent = false;

	const SQChar *str = NULL;
	SQInteger len;
	int top = sq_gettop( vm );
	int i = 2;
	HSQOBJECT o;

	// for each arg
	for (;;)
	{
		sq_getstackobj( vm, i, &o );

		if ( sq_type(o) == OT_INTEGER ||
				sq_type(o) == OT_BOOL ||
				sq_type(o) == OT_FLOAT ||
				sq_type(o) == OT_ARRAY ||
				sq_type(o) == OT_TABLE ||
				sq_type(o) == OT_CLASS ||
				sq_type(o) == OT_NULL )
		{
			PrintObj( vm, o );
		}
		else if ( sq_type(o) == OT_STRING )
		{
#ifdef SQUNICODE
			PrintWCharToUTF8( g_StreamOut, _string(o)->_val, _string(o)->_len );
#else
			Printw( _string(o)->_val, _string(o)->_len );
#endif
		}
		else
		{
#if SQUIRREL_VERSION_NUMBER >= 300
			if ( !SQ_SUCCEEDED( sq_tostring( vm, i ) ) )
			{
				g_bTableIndent = _g_bTableIndent;
				return SQ_ERROR;
			}
#else
			sq_tostring( vm, i );
#endif

			sq_getstringandsize( vm, -1, &str, &len );

#ifdef SQUNICODE
			PrintWCharToUTF8( g_StreamOut, str, len );
#else
			Printw( str, len );
#endif
		}

		if ( ++i > top )
			break;

		Printc( ' ' );
	}

	Printc( '\n' );

	g_bTableIndent = _g_bTableIndent;
	return 0;
}

SQInteger sq_sleep( HSQUIRRELVM vm )
{
	SQInteger ms = 0;
	sq_getinteger( vm, -1, &ms );
	_sleep( (int)ms );
	return 0;
}

#ifdef NOSQSTDLIB
void compilererrorfunc( HSQUIRRELVM vm,
		const SQChar *err,
		const SQChar *source,
		SQInteger line,
		SQInteger column )
{
	errorfunc( vm,
			_SC("" FMT_STR " line = (%d) column = (%d) : error " FMT_STR "\n"),
			source, line, column, err );
}
#endif

static SQInteger sq_lex( SQUserPointer file )
{
	char ch;
	if ( fread( &ch, 1, 1, (FILE*)file ) > 0 )
		return ch;
	return 0;
}

int PushArgv( HSQUIRRELVM vm, char **argv )
{
	Assert( argv );
	int argc = 0;

	while ( *argv )
	{
		int srclen = strlen( *argv );
#ifdef SQUNICODE
		int len = UTF8ToWChar( NULL, 0, *argv, srclen );
		SQChar *dst = sq_getscratchpad( vm, len * sizeof(SQChar) );
		UTF8ToWChar( dst, len * sizeof(SQChar), *argv, srclen );
		sq_pushstring( vm, dst, len );
#else
		sq_pushstring( vm, *argv, srclen );
#endif
		argv++;
		argc++;
	}

	return argc;
}

void DoExecuteFile( HSQUIRRELVM vm, const char *filename, const SQChar *sourcename, char **argv )
{
	STACKCHECK( vm );
	int succ;
	FILE *file = fopen( filename, "rb" );

	if ( file )
	{
		if ( SQ_SUCCEEDED( sq_compile( vm, sq_lex, file, sourcename, SQTrue ) ) )
		{
			sq_pushroottable( vm );

			int argc = 1;
			if ( argv )
			{
#if SQUIRREL_VERSION_NUMBER < 300 && defined(HAS_SQCLOSURE_TYPE)
				HSQOBJECT tmp;
				sq_getstackobj( vm, -2, &tmp );
				Assert( sq_type(tmp) == OT_CLOSURE && !_fp(_closure(tmp)->_function)->_varparams );
				_fp(_closure(tmp)->_function)->_varparams = true;
#endif
				argc += PushArgv( vm, argv );
			}

			if ( SQ_SUCCEEDED( sq_call( vm, argc, SQTrue, SQTrue ) ) )
			{
				HSQOBJECT ret;
				sq_getstackobj( vm, -1, &ret );
				if ( sq_type(ret) != OT_NULL )
					PrintObj( vm, ret );
				sq_pop( vm, 1 );
			}

			sq_pop( vm, 1 );
		}

		fclose( file );
		succ = SQ_OK;
	}
	else
	{
		throwerrorf( vm, _SC("could not open file '" FMT_STR "'"), sourcename );
		succ = SQ_ERROR;
	}

#if 0
	(void)filename;

	if ( SQ_SUCCEEDED( sqstd_loadfile( vm, sourcename, SQTrue ) ) )
	{
		sq_pushroottable( vm );

		if ( SQ_SUCCEEDED( sq_call( vm, 1, SQTrue, SQTrue ) ) )
		{
			HSQOBJECT ret;
			sq_getstackobj( vm, -1, &ret );
			if ( sq_type(ret) != OT_NULL )
				PrintObj( vm, ret );
			sq_pop( vm, 1 );
		}

		sq_pop( vm, 1 );
		succ = SQ_OK;
	}
	else
	{
		succ = SQ_ERROR;
	}
#endif

	if ( SQ_FAILED( succ ) )
	{
		Error( "<ERROR:" );
		g_StreamOut = stderr;
		PrintObj( vm, vm->_lasterror );
		g_StreamOut = stdout;
		Errorc( '>' );
		Errorc( '\n' );
	}
}

void InitVM( HSQUIRRELVM *vm, vm_init_options opt )
{
	*vm = sq_open( opt.stacksize );
	g_vm = *vm;

	STACKCHECK( *vm );

#ifndef NOSQSTDLIB
	sqstd_seterrorhandlers( *vm );
#else
	sq_setcompilererrorhandler( *vm, compilererrorfunc );
#endif

	setprintfunc( *vm, dummyprintfunc, dummyprintfunc );
	sqdbg_attach_debugger( *vm );
	setprintfunc( *vm, printfunc, errorfunc );
	// Reassign print funcs
	sqdbg_attach_debugger( *vm );

	sq_pushroottable( *vm );

#ifndef NOSQSTDLIB
	if ( !opt.nostdlib )
	{
		sqstd_register_bloblib( *vm );
		sqstd_register_iolib( *vm );
		sqstd_register_systemlib( *vm );
		sqstd_register_mathlib( *vm );
		sqstd_register_stringlib( *vm );
	}
#endif

#if SQUIRREL_VERSION_NUMBER < 301
	push_int( *vm, "_versionnumber_", SQUIRREL_VERSION_NUMBER );
#endif
#ifdef _DEBUG
	push_func( *vm, "getregistrytable", sq_getregistrytable, 1, "." );
#endif
	push_func( *vm, "printl", sq_printl, -2, ".." );
	push_func( *vm, "sleep", sq_sleep, 2, ".n" );

#ifdef SQFFI
#ifdef _WIN32
	push_int( *vm, "_WIN32", 1 );
#else
	push_int( *vm, "_WIN32", 0 );
#endif
	push_int( *vm, "_ptrsize_", (int)sizeof(void*) );
	RegisterFFI( *vm, opt );
#endif

#ifdef SQDBG_NATIVE_STACKTRACE
	if ( InitSymbolHandler( &g_hProcess, *vm ) )
	{
		push_func( *vm, "stacktrace", StackTrace, -1, "..n" );
		sq_newclosure( *vm, StackTrace, 0 );
		sq_seterrorhandler( *vm );
		// Reassign error handler
		sqdbg_attach_debugger( *vm );
	}
#endif

	sq_pop( *vm, 1 );
}

void PrintVersion()
{
	Printf( FMT_STR "\n" FMT_STR " (%s)\n",
			SQUIRREL_VERSION,
			SQUIRREL_COPYRIGHT,
			SQ_HASH_SQUIRREL ? SQ_HASH_SQUIRREL : "" );
	Printf( "Squirrel Debugger (sqdbg) by samisalreadytaken (%s)\n",
			SQ_HASH_SQDBG );
	Printf( "\nsq%s (%s)\n",
			SQ_BUILD_TAG,
			SQ_HASH_SQ );
#ifdef NO_GARBAGE_COLLECTOR
	Print( "NO_GARBAGE_COLLECTOR\n" );
#endif
#ifdef SQDBG_DISABLE_PROFILER
	Print( "SQDBG_DISABLE_PROFILER\n" );
#endif
#ifdef SQDBG_DISABLE_PROFILER_AUTO
	Print( "SQDBG_DISABLE_PROFILER_AUTO\n" );
#endif
#ifdef SQDBG_DISABLE_COMPILER
	Print( "SQDBG_DISABLE_COMPILER\n" );
#endif
#ifdef SQFFI
	Print( "SQFFI\n" );
#endif
	Printf( "Build %s %s\n", __TIME__, __DATE__ );
}

void PrintHelp( const char *cmd, bool ex )
{
	Printf( "Usage: %s [(file [-- [arg]...])|[option]...]\n", cmd );
	Print( "Options:\n" );
	Printf( "  %-24s%s\n", "-x, --exec[=]INPUT", "Execute string" );
	Printf( "  %-24s%s\n", "--file[=]FILENAME", "Execute file" );
	Printf( "  %-24s%s\n", "--server[=]PORT", "Start Squirrel Debugger as a server on port" );
	Printf( "  %-24s%s\n", "--stacksize[=]VALUE", "Set initial stack size" );
	Printf( "  %-24s%s\n", "--nostdlib", "Do not register standard libraries" );
	Printf( "  %-24s%s\n", "--json", "Output tables in JSON format" );
	Printf( "  %-24s%s\n", "--oneline", "Output tables in single line" );
#if defined(SQFFI) && defined(_WIN32)
	Printf( "  %-24s%s\n", "--windef", "Register base Windows type definitions" );
#endif
	Printf( "  %-24s%s\n", "--repl", "Start interactive interpreter" );
	Printf( "  %-24s%s\n", "-", "Execute from stdin" );
	Printf( "  %-24s%s\n", "--", "Pass remaining arguments to (file or stdin) script" );
	Printf( "  %-24s%s\n", "-v, --version", "Print version" );
	Printf( "  %-24s%s\n", "-h, --help", "Print help" );
	if ( ex )
	{
		Print( "\n"
				"Single line inputs do not support control statements (if; for; while...),\n"
				"however they support '?' operator and debugger format specifiers.\n"
				"Multi line inputs do not support format specifiers.\n"
				"If the input ends with double semicolon (;;), the result is not output.\n"
				"\n"
				"    INPUT: sq -x 0xAA,bb\n"
				"    OUTPUT: 10101010\n"
				"\n"
				"    INPUT: sq -x 13 -x 37\n"
				"    OUTPUT: 1337\n"
				"\n"
				"    INPUT: sq -x \"0 ? a <- 10 : a <- 20; a * 100,x2\"\n"
				"    OUTPUT: 0x07d0\n"
				"\n"
				"    INPUT: echo -e \"local a = 10\\nreturn a*2\" | sq -\n"
				"    OUTPUT: 20\n"
				"\n"
				"    INPUT: echo return 2 > t.nut && sq -x 1 --file t.nut -x 3\n"
				"    OUTPUT: 123\n"
				"\n"
				"    INPUT: sq -x \"A <- [0, 0];;\" -x \"A[0]=4;A[1]=5;;\" -x A\n"
				"    OUTPUT: [4, 5]\n"
				"\n"
				"    INPUT: echo printl(vargv) > t.nut && sq --exec=7 --file=t.nut -- 1 2 3\n"
				"    OUTPUT: 7[\"1\", \"2\", \"3\"]\n"
				"\n"
				"Interactive interpreter input supports basic navigation and editing,\n"
				"however it is not a text editor. Navigation will break on word-wrapped lines.\n"
				"\n"
				"Multiple lines can be input by opening a bracket '{' on the first line,\n"
				"and it can be executed by closing a bracket '}' on the beginning of a line,\n"
				"or by pressing ^J (CTRL-RETURN).\n"
				"Input is automatically indented on curly brackets.\n"
				"\n"
				"   |" STR_PROMPT_PLAIN "for ( local i = 0; i < 10; ++i ) {\n"
				"   |    printl(i);\n"
				"   |}\n"
				"\n"
				"Pressing F1 will connect the Squirrel Debugger to the\n"
				"port input in the command buffer.\n"
				"\n"
				"\n"
				"FFI USAGE\n"
				"\n"
				"Functions:\n"
				"   // Returns a reference counted handle to library\n"
				"-  module    ffi.LoadLibrary( name )\n"
				"\n"
				"   // Get function from module by symbol\n"
				"   // Holds a reference to the module\n"
				"   // Variadic argument must be the last specified type\n"
				"   //\n"
				"   // If 'returnType' is 't_str' or 't_wstr', the returned pointer is interpreted as C string\n"
				"   // and a new Squirrel string is returned with automatic unicode conversion.\n"
				"   // If 'argType' is 't_str' (or 't_wstr' on unicode build),\n"
				"   // the Squirrel string pointer is passed to the function.\n"
				"   //\n"
				"   // Alternatively C function signature can be used\n"
				"   // Primitive and sized '[u]int#_t' types are allowed.\n"
				"   // 'char*' and 'wchar_t*' are interpreted as string options\n"
				"   // 'const' and pointer types are ignored\n"
				"   //\n"
				"   // On Windows, base Win32 types are also allowed\n"
				"-  function  ffi.GetFunction( module, signature, flags = 0 )\n"
				"-  function  ffi.GetFunction( module, returnType, functionName, argType... )\n"
				"-  function  ffi.MakeFunction( returnType, functionAddress, argType... )\n"
				"\n"
				"   // Given any number of function declarations with optional global type definitions,\n"
				"   // get them from the module and put them in the target container table or class.\n"
				"-            ffi.GetFunctions( targetContainer, module, signatures )\n"
				"\n"
				"   // Given the declaration of a no inheritance pure virtual class and\n"
				"   // the address of an instance to it, bind every function in the class\n"
				"   // to the instance, passing it as the first parameter,\n"
				"   // then put them in the target container table or class.\n"
				"   // Tries to get vtable from instance. May not work on all platforms.\n"
				"   // Local type definitions are allowed within the class definition.\n"
				"-            ffi.GetVirtualClass( targetContainer, instanceAddress, virtualClass )\n"
				"\n"
				"   // Returns script accessible array object\n"
				"   // 'ptr', 'count', 'size', 'align' are accessible members\n"
				"   // 'ptr', 'count' are modifiable members\n"
				"   // Pointer increment in C `p++` can be replicated with `ptr += align`\n"
				"   // Modifying 'ptr' automatically adjusts 'count'\n"
				"   // Array of arrays access is not supported, 'int[2][3]' is parsed as 'int[6]'\n"
				"   // It could be manually replicated with pointer arrays and manual interpretation.\n"
				"   // Identical to ffi.get with array signature.\n"
				"   //    ffi.MakeArray( t_i32, addr, 10 )\n"
				"   //    ffi.get( \"int32_t[10]\", addr )\n"
				"-  array    ffi.MakeArray( type, address, count )\n"
				"\n"
				"   // Creates a native closure which executes the script function,\n"
				"   // it is used to pass to function pointer fields/parameters.\n"
				"   // Memory is reference counted. Passing script functions directly\n"
				"   // will allocate this closure automatically which will not be released\n"
				"   // until the end of the program.\n"
				"   //\n"
				"   //    local ptr = ffi.malloc( \"struct { void (*func)(); }\" );\n"
				"   //    // Memory is not freed\n"
				"   //    ptr.func = function() { printl(1) }\n"
				"   //    ptr.func();\n"
				"   //    // Memory is freed when 'closure' exits scope\n"
				"   //    local closure = ffi.MakeClosure( \"void (*func)();\", function() { printl(2) } );\n"
				"   //    ptr.func = closure;\n"
				"   //    ptr.func();\n"
				"   //\n"
				"-  closure   ffi.MakeClosure( signature|proc, function, thread = null )\n"
				"\n"
				"   // Registers one or more type definitions\n"
				"   // Keyword 'typedef' must be followed by a known type and target name.\n"
				"   // Named struct definitions also register the struct name.\n"
				"   // Local type definitions are allowed within a struct definition.\n"
				"   // Unnamed structs are supported in unions.\n"
				"   // Custom struct alignment can be specified on struct definition using 'alignas' keyword.\n"
				"   // Custom struct packing alignment can be specified on struct definition using 'pack' keyword,\n"
				"   // this is equivalent to '#pragma pack'.\n"
				"   // Bit fields are supported.\n"
				"   //\n"
				"   // Note that struct alignment, packing and bit fields are implementation defined,\n"
				"   // certain member arrangements and alignments may differ between compilers and\n"
				"   // cause ABI incompatibility if they are passed to foreign functions.\n"
				"   // Most conditions here match little-endian MSVC behaviour,\n"
				"   // see TARGET_MS_BITFIELD_LAYOUT_P for some GCC differences.\n"
				"   //    ffi.typedef(@\"\n"
				"   //            typedef int A;\n"
				"   //            typedef A B[2];\n"
				"   //            typedef struct { A a; } S1;\n"
				"   //            struct pack(1) alignas(8) S2 { typedef S1 sub_t; sub_t b; char c[3]; };\n"
				"   //            union clr32 {\n"
				"   //                struct {\n"
				"   //                    uint8_t r, g, b, a;\n"
				"   //                };\n"
				"   //                struct {\n"
				"   //                    uint32_t r1 : 8, g1 : 8, b1 : 8, a1 : 8;\n"
				"   //                };\n"
				"   //                uint32_t rgba;\n"
				"   //                uint8_t v[4];\n"
				"   //            };\n"
				"   //    \")\n"
				"-            ffi.typedef( signatures )\n"
				"\n"
				"   // Get error number if last called function had one of the following attributes:\n"
				"   // [[sq::errno]], [[sq::GetLastError]]\n"
				"-  int       ffi.GetLastError()\n"
				"\n"
				"   // Dereference address and set value\n"
				"   // Type can be signature\n"
				"   // Equivalent to C code: *(type*)address = (type)value\n"
				"-            ffi.set( addrType, address, value )\n"
				"\n"
				"   // Dereference address and get value\n"
				"   // Type can be signature\n"
				"   // Equivalent to C code: *(type*)address\n"
				"   // If type is 't_str' or 't_wstr', the address is interpreted as C string\n"
				"   // and a new Squirrel string is returned with automatic unicode conversion.\n"
				"-  any       ffi.get( addrType, address )\n"
				"\n"
				"   // Allocate reference counted memory\n"
				"   // Array and struct allocations can be aligned using 'alignas' keyword\n"
				"   //    local ptr = ffi.malloc( \"alignas(32) int[1]\" )\n"
				"   //    assert( ( ptr.ptr % 32 ) == 0 );\n"
				"-  pointer   ffi.malloc( bytes|signature )\n"
				"\n"
				"   // Returns type ID of type signature\n"
				"   //    ffi.type(\"char\") == ffi.t_char\n"
				"-  int       ffi.type( signature )\n"
				"\n"
				"   // Returns byte count of type id, struct, array or signature\n"
				"   //    ffi.sizeof(ffi.t_i8) == 1\n"
				"   //    ffi.sizeof(\"short\") == 2\n"
				"   //    ffi.sizeof( ffi.get( \"struct TESTS { int32_t a[5]; }\", 0 ) ) == 20\n"
				"   //    ffi.sizeof(\"struct TESTS\") == 20\n"
				"-  int       ffi.sizeof( type|struct|array|signature )\n"
				"\n"
				"   // Returns alignment of type id, struct, array or signature\n"
				"-  int       ffi.alignof( type|struct|array|signature )\n"
				"\n"
				"   // Returns struct member byte offset\n"
				"   // If a member is not specified, returns the memory address of the input\n"
				"-  int       ffi.offsetof( struct|signature, member )\n"
				"-  int       ffi.offsetof( struct|array )\n"
				"\n"
				"Types:\n"
				"   ffi.t_void, ffi.t_ptr, ffi.t_str, ffi.t_wstr\n"
				"   ffi.t_i8, ffi.t_u8, ffi.t_i16, ffi.t_u16\n"
				"   ffi.t_i32, ffi.t_u32, ffi.t_i64, ffi.t_u64\n"
				"   ffi.t_f32, ffi.t_f64\n"
				"   ffi.t_var\n"
				"\n"
				"Type aliases:\n"
				"   ffi.t_bool, ffi.t_char, ffi.t_uchar, ffi.t_wchar,\n"
				"   ffi.t_short, ffi.t_ushort ffi.t_int, ffi.t_uint,\n"
				"   ffi.t_long, ffi.t_ulong, ffi.t_longlong, ffi.t_ulonglong,\n"
				"   ffi.t_intptr, ffi.t_uintptr, ffi.t_float, ffi.t_double,\n"
				"   ffi.t_sqchar, ffi.t_sqstr\n"
				"\n"
				"Constants:\n"
				"   _ptrsize_, _WIN32\n"
				"\n"
				"\n"
				"EXAMPLES\n"
				"\n"
				"    if ( _WIN32 )\n"
				"    {\n"
				"        user32 <- ffi.LoadLibrary( \"user32.dll\" );\n"
				"        MessageBox <- ffi.GetFunction( user32,\n"
				"            _charsize_ == 1 ?\n"
				"                \"int MessageBoxA( HANDLE, LPCSTR lpText, LPCSTR lpCaption, UINT uType )\" :\n"
				"                \"int MessageBoxW( HANDLE, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType )\" );\n"
				"        local ret = MessageBox( null, \"message\", \"title\", 0 );\n"
				"        printl( \"ret:\", ret );\n"
				"    }\n"
				"\n"
				"    cstdlib <- ffi.LoadLibrary( _WIN32 ? \"msvcrt.dll\" : \"libc.so.6\" );\n"
				"    cmalloc <- ffi.GetFunction( cstdlib, ffi.t_ptr, \"malloc\", ffi.t_int );\n"
				"    cfree <- ffi.GetFunction( cstdlib, ffi.t_void, \"free\", ffi.t_ptr );\n"
				"    cprintf <- ffi.GetFunction( cstdlib,\n"
				"                                ffi.t_int,\n"
				"                                _charsize_ == 1 ? \"printf\" : \"wprintf\",\n"
				"                                _charsize_ == 1 ? ffi.t_str : ffi.t_wstr,\n"
				"                                ffi.t_var );\n"
				"\n"
				"    // Using C function signatures:\n"
				"    cmalloc <- ffi.GetFunction( stdlib, \"void *malloc(int)\" );\n"
				"    cfree <- ffi.GetFunction( stdlib, \"void free(void*)\" );\n"
				"    cprintf <- ffi.GetFunction( stdlib,\n"
				"                                _charsize_ == 1 ?\n"
				"                                    \"int printf(const char *, ...)\" :\n"
				"                                    \"int wprintf(const wchar_t *, ...)\" );\n"
				"\n"
				"    // Getting them in batch:\n"
				"    ffi.GetFunctions( this, stdlib, @\"\n"
				"            void *malloc(int);\n"
				"            void free(void*);\n"
				"            int printf(const char *, ...);\n"
				"\n"
				"            typedef int (__cdecl *SORTFUNC)(const void *, const void *);\n"
				"            void qsort(void *, size_t, size_t, SORTFUNC);\n"
				"    \" );\n"
				"\n"
				"    // output: test 1 2 3 0.400000 1\n"
				"    cprintf( \"%s %d %d %d %f %d\\n\", \"test\", 1, 2, 3, 0.4, true );\n"
				"\n"
				"    // Replicating the following C code:\n"
				"    /*\n"
				"        char *pStr = malloc( sizeof(char) * 4 );\n"
				"        pStr[0] = 0x41;\n"
				"        pStr[1] = 0x42;\n"
				"        pStr[2] = 0x43;\n"
				"        pStr[3] = 0;\n"
				"        int nVal = *(int*)pStr;\n"
				"        printf( \"%hs 0x%08x\\n\", pStr, nVal );\n"
				"        free( pStr );\n"
				"    */\n"
				"\n"
				"    local pStr = cmalloc( ffi.sizeof(\"char\") * 4 );\n"
				"    ffi.set( ffi.t_char, pStr + 0, 0x41 );\n"
				"    ffi.set( ffi.t_char, pStr + 1, 0x42 );\n"
				"    ffi.set( ffi.t_char, pStr + 2, 0x43 );\n"
				"    ffi.set( ffi.t_char, pStr + 3, 0 );\n"
				"    local nVal = ffi.get( ffi.t_int, pStr );\n"
				"    local sqStr = ffi.get( ffi.t_str, pStr );\n"
				"    cprintf( \"%hs 0x%08x %s(%d)\\n\", pStr, nVal, sqStr, sqStr.len() );\n"
				"    cfree( pStr );\n"
				"\n"
				"    // Alternatively using reference counted memory and array\n"
				"    local pStr = ffi.malloc( \"char[4]\" );\n"
				"    pStr[0] = 0x41;\n"
				"    pStr[1] = 0x42;\n"
				"    pStr[2] = 0x43;\n"
				"    pStr[3] = 0;\n"
				"    local nVal = ffi.get( \"int\", pStr );\n"
				"    local sqStr = ffi.get( \"char*\", pStr );\n"
				"    cprintf( \"%hs 0x%08x %s(%d)\\n\", pStr.ptr, nVal, sqStr, sqStr.len() );\n"
				"    pStr = null;\n"
				"\n"
				"    // Using an unnamed struct in a union\n"
				"    ffi.typedef(@\"\n"
				"        union clr32 {\n"
				"            struct {\n"
				"                uint8_t r, g, b, a;\n"
				"            };\n"
				"            uint32_t rgba;\n"
				"            uint8_t v[4];\n"
				"        };\n"
				"    \");\n"
				"\n"
				"    assert( ffi.sizeof(\"union clr32\") == ffi.sizeof(\"uint32_t\") );\n"
				"    local ptr = ffi.malloc( \"union clr32\" );\n"
				"    ptr.rgba = 0x12345678;\n"
				"\n"
				"    printf( \"clr32.rgba   %08x\\n\", ptr.rgba ); // 12345678\n"
				"    printf( \"clr32.r      %02x\\n\", ptr.r );    // 78\n"
				"    printf( \"clr32.g      %02x\\n\", ptr.g );    // 56\n"
				"    printf( \"clr32.b      %02x\\n\", ptr.b );    // 34\n"
				"    printf( \"clr32.a      %02x\\n\", ptr.a );    // 12\n"
				"    printf( \"clr32.v[0]   %02x\\n\", ptr.v[0] ); // 78\n"
				"    printf( \"clr32.v[1]   %02x\\n\", ptr.v[1] ); // 56\n"
				"    printf( \"clr32.v[2]   %02x\\n\", ptr.v[2] ); // 34\n"
				"    printf( \"clr32.v[3]   %02x\\n\", ptr.v[3] ); // 12\n"
				"\n"
				"    // Given the following C++ library test.dll:\n"
				"    /*\n"
				"        class ITest\n"
				"        {\n"
				"        public:\n"
				"            typedef int RETTYPE;\n"
				"\n"
				"            virtual RETTYPE fn1() = 0;\n"
				"            virtual RETTYPE fn2(int i) = 0;\n"
				"        };\n"
				"\n"
				"        class CTest : public ITest\n"
				"        {\n"
				"        public:\n"
				"            int m_a;\n"
				"            CTest() { m_a = 1337; }\n"
				"            RETTYPE fn1() override { return m_a; }\n"
				"            RETTYPE fn2(int i) override { return m_a + i; }\n"
				"        };\n"
				"\n"
				"        extern \"C\" __declspec(dllexport)\n"
				"        void *CreateInterface()\n"
				"        {\n"
				"            return (void*)( new CTest() );\n"
				"        }\n"
				"    */\n"
				"\n"
				"    // Call CTest member functions\n"
				"    local test = {}\n"
				"    local CreateInterface = ffi.GetFunction( ffi.LoadLibrary( \"test.dll\" ),\n"
				"                                             \"void *CreateInterface()\" );\n"
				"\n"
				"    ffi.GetVirtualClass( test, CreateInterface(),\n"
				"        @\"class ITest\n"
				"        {\n"
				"        public:\n"
				"            typedef int RETTYPE;\n"
				"\n"
				"            virtual RETTYPE fn1() = 0;\n"
				"            virtual RETTYPE fn2(int i) = 0;\n"
				"        };\" );\n"
				"\n"
				"    printl( test.fn1() );  // 1337\n"
				"    printl( test.fn2(3) ); // 1340\n"
				"\n"
				"    // Given the following C library test.dll with nested callbacks:\n"
				"    /*\n"
				"        void sub(void (*fn)())\n"
				"        {\n"
				"            printf(\"3\");\n"
				"            fn();\n"
				"        }\n"
				"\n"
				"        extern \"C\" __declspec(dllexport)\n"
				"        void nested(void (*fn)(void (*)(void (*)())))\n"
				"        {\n"
				"            printf(\"1\");\n"
				"            fn( &sub );\n"
				"        }\n"
				"    */\n"
				"\n"
				"    // The following Squirrel code outputs: 1234\n"
				"    local nested = ffi.GetFunction( ffi.LoadLibrary( \"test.dll\" ),\n"
				"                                    \"void nested(void (*)(void (*)(void (*)())))\" );\n"
				"    nested( function(fn) {\n"
				"        print(2);\n"
				"        fn( function() {\n"
				"            print(4);\n"
				"        } );\n"
				"    } );\n"
				"\n"
				"\n"
				"Native exceptions throw Squirrel exceptions to indicate crash type and location.\n"
		);
	}
}

int main( int argc, char **argv )
{
#if defined(_WIN32) && defined(_DEBUG)
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_WNDW );
	_CrtSetDbgFlag( _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG ) |
			_CRTDBG_ALLOC_MEM_DF |
			_CRTDBG_LEAK_CHECK_DF );
#endif

#ifdef _WIN32
	(void)_setmode( _fileno( stdout ), _O_BINARY );
	(void)_setmode( _fileno( stdin ), _O_BINARY );
#endif

	g_StreamOut = stdout;
	g_StreamIn = stdin;

	HSQUIRRELVM vm = NULL;
	HSQDEBUGSERVER dbg = NULL;
	vm_init_options opt = {};
	opt.stacksize = 1024;

	int port = -1;
	bool repl = false;

	if ( argc == 1 )
		repl = true;

	for ( int ai = 1; ai < argc; ai++ )
	{
		char *value = argv[ ai + 1 ];
		char *eq = NULL;
		char *command = NULL;

		if ( argv[ai][0] == '-' )
		{
			command = argv[ai] + 1;

			if ( command[0] == '-' )
			{
				command++;
				eq = strchr( command, '=' );

				if ( eq )
				{
					*eq = 0;
					value = eq + 1;
				}

				if ( !strcmp( command, "exec" ) )
				{
arg_exec:
					if ( value )
					{
						if ( !eq )
							++ai;

						if ( !vm )
							InitVM( &vm, opt );

						Execute( vm, value, strlen( value ) );
					}
					else
					{
						Error( "expected string to execute\n" );
						return -1;
					}
				}
				else if ( !strcmp( command, "file" ) )
				{
arg_file:
					if ( value )
					{
						if ( !eq )
							++ai;

#ifdef SQUNICODE
						SQChar filename[512];
						int len = strlen( value );
						UTF8ToWChar( filename, sizeof(filename), value, len );
						filename[len] = 0;
#else
						const SQChar *filename = value;
#endif

						if ( !vm )
							InitVM( &vm, opt );

						char **av = NULL;

						// sq file --
						// sq --file=file --
						if ( argv[ai+1] &&
								argv[ai+1][0] == '-' && argv[ai+1][1] == '-' && argv[ai+1][2] == 0 )
						{
							av = argv + ai + 2;
						}

						DoExecuteFile( vm, value, filename, av );

						if ( av )
							break;
					}
					else
					{
						Error( "expected file name\n" );
						return -1;
					}
				}
				else if ( !strcmp( command, "server" ) )
				{
					if ( value )
					{
						if ( !eq )
							++ai;

						port = atoi( value, strlen( value ) );

						if ( port < 0 || port > 0xffff ||
								( !port && !CHARCMP( value, '0' ) ) )
						{
							Errorf( "invalid port '%s'\n", value );
							return -1;
						}

						if ( !vm )
							InitVM( &vm, opt );

						if ( !dbg )
							dbg = sqdbg_attach_debugger( vm );

						sqdbg_listen_socket( dbg, (unsigned short)port );
					}
					else
					{
						Error( "expected server port\n" );
						return -1;
					}
				}
				else if ( !strcmp( command, "repl" ) )
				{
					repl = true;
				}
				else if ( !strcmp( command, "json" ) )
				{
					g_bTableJSONOutput = true;
				}
				else if ( !strcmp( command, "oneline" ) )
				{
					g_bTableIndent = false;
				}
				else if ( !strcmp( command, "stacksize" ) )
				{
					if ( vm )
					{
						Errorf( "option '%s' was specified too late\n", command );
						return -1;
					}

					if ( value )
					{
						if ( !eq )
							++ai;

						opt.stacksize = atoi( value, strlen( value ) );

						if ( opt.stacksize <= 0 )
						{
							Errorf( "invalid stack size '%s'\n", value );
							return -1;
						}
					}
					else
					{
						Error( "expected stack size\n" );
						return -1;
					}
				}
				else if ( !strcmp( command, "nostdlib" ) )
				{
					if ( vm )
					{
						Errorf( "option '%s' was specified too late\n", command );
						return -1;
					}

					opt.nostdlib = true;
				}
				else if ( !strcmp( command, "windef" ) )
				{
					if ( vm )
					{
						Errorf( "option '%s' was specified too late\n", command );
						return -1;
					}

#if defined(SQFFI) && defined(_WIN32)
					opt.windef = true;
#endif
				}
				else if ( !strcmp( command, "version" ) )
				{
arg_version:
					PrintVersion();
				}
				else if ( !strcmp( command, "help" ) )
				{
					eq = (char*)1;
arg_help:
					PrintHelp( argv[0], ( eq != NULL ) );
				}
				else
				{
arg_unknown:
					Errorf( "unknown argument '%s'\n", argv[ai] );
					return -1;
				}
			}
			else
			{
				if ( CHARCMP( command, 'x' ) )
				{
					goto arg_exec;
				}
				else if ( CHARCMP( command, 'v' ) )
				{
					goto arg_version;
				}
				else if ( CHARCMP( command, 'h' ) )
				{
					goto arg_help;
				}
				else if ( command[0] == 0 )
				{
					if ( !vm )
						InitVM( &vm, opt );

					char **av = NULL;

					if ( argv[ai+1] &&
							argv[ai+1][0] == '-' && argv[ai+1][1] == '-' && argv[ai+1][2] == 0 )
					{
						av = argv + ai + 2;
					}

					ReadStdin( vm, av );

					if ( av )
						break;
				}
				else
				{
					goto arg_unknown;
				}
			}
		}
		else
		{
			if ( ai == 1 &&
					( argc == 2 || ( argv[ai+1] &&
									 argv[ai+1][0] == '-' && argv[ai+1][1] == '-' && argv[ai+1][2] == 0 ) ) )
			{
				value = argv[ai];
				eq = (char*)1;
				goto arg_file;
			}

			goto arg_unknown;
		}
	}

	if ( repl )
	{
		if ( !InitTerm() )
			return -1;

		if ( argc == 1 )
			Printf( FMT_STR "\n", SQUIRREL_VERSION );

		if ( !vm )
			InitVM( &vm, opt );

		Assert( !g_pCmdBuf );
		g_nCmdCap = CMD_BUF_INIT_SIZE;
		g_pCmdBuf = (char*)malloc( g_nCmdCap );
		memset( g_pCmdBuf, 0, g_nCmdCap );

		g_pCmdPtr = g_pCmdBuf;
		g_pCmdEnd = g_pCmdBuf;

		Assert( !g_pHistory );
		g_nHistoryCapacity = CMD_BUF_INIT_SIZE;
		g_pHistory = (char*)malloc( g_nHistoryCapacity );
		memset( g_pHistory, 0, g_nHistoryCapacity );

		Assert( !g_pHistoryMap );
		g_pHistoryMap = (int*)malloc( HISTORY_COUNT * sizeof(int) );
		memset( (void*)g_pHistoryMap, 0, HISTORY_COUNT * sizeof(int) );

		Input( vm, dbg );

#if defined(_WIN32) && defined(_DEBUG)
		InvalidateHistoryIndex();
		free( g_pCmdBuf );
		free( g_pHistory );
		free( g_pHistoryMap );
		g_pCmdBuf = NULL;
		g_pHistory = NULL;
		g_pHistoryMap = NULL;
#endif

		RestoreTerm();
	}
	else if ( dbg )
	{
		for (;;)
		{
			sqdbg_frame( dbg );
			_sleep( 25 );
		}
	}

	// Leak on release
#if defined(_WIN32) && defined(_DEBUG)
	if ( vm )
	{
#ifdef SQFFI
		Parser_Typedef_Restore( 0 );
		free( g_pParserTypedefs );
		g_pParserTypedefs = NULL;

		Assert( g_nParserStackIndex == 0 );
		free( g_pParserStack );
		g_pParserStack = NULL;
#endif

		// Quiet debugger print
		setprintfunc( vm, dummyprintfunc, dummyprintfunc );
		sqdbg_attach_debugger( vm );
		sqdbg_destroy_debugger( vm );

		g_vm = NULL;
		sq_close( vm );

#ifdef SQDBG_NATIVE_STACKTRACE
		TerminateSymbolHandler( &g_hProcess );
#endif
	}

	//_CrtDumpMemoryLeaks();
#endif

	return 0;
}
