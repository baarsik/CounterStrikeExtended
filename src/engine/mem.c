// mem.c
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <stdio.h>
#endif

#if defined(_WIN32) && defined(NDEBUG)
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* expression, const wchar_t* filename,
	unsigned int line );
#endif

void* Mem_ZeroMalloc( size_t size )
{
	void* p = malloc( size );
#if defined(_WIN32)
	if ( !p )
		_wassert( L"p", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\mem.c", 18 );
#else
    if (p == NULL) {
        // The retained UCRT assertion is fatal even in Release. A disabled C
        // assert must not permit memset to continue with a null allocation.
        fputs("Mem_ZeroMalloc: allocation failed\n", stderr);
        abort();
    }
#endif
#if defined(_MSC_VER)
#pragma warning( suppress: 6387 )
#endif
	memset( p, 0, size );
	return p;
}
