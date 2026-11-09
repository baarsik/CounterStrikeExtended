#include "VGUI.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* (*staticMalloc)(size_t) = malloc;
static void ( *staticFree )( void* ) = free;

void* operator new(size_t size) {
    return staticMalloc(size);
}

void operator delete(void* memblock) noexcept {
    staticFree(memblock);
}

void operator delete(void* memblock, size_t) noexcept {
    staticFree(memblock);
}

void vgui_setMalloc(void* (*theMalloc)(size_t)) {
    if (theMalloc != NULL) {
        staticMalloc = theMalloc;
    } else {
        staticMalloc = malloc;
    }
}

void vgui_setFree( void ( *theFree )( void* ) )
{
	if ( theFree != NULL )
	{
		staticFree = theFree;
	}
	else
	{
		staticFree = free;
	}
}

namespace vgui
{

void vgui_strcpy( char* dst, int dstLen, const char* src )
{
	int srcLen = (int)strlen( src ) + 1;
	if ( srcLen > dstLen )
	{
		srcLen = dstLen;
	}
	memcpy( dst, src, srcLen - 1 );
	dst[srcLen - 1] = 0;
}

char* vgui_strdup( const char* src )
{
	int len = (int)strlen( src );
	char* dst = new char[len + 1];
	memcpy( dst, src, len );
	dst[len] = 0;
	return dst;
}

int vgui_printf(const char* format, ...) {
    constexpr size_t formatBufferBytes = 2048;
    char buf[formatBufferBytes] = {};
    va_list argList;
    va_start(argList, format);
    const int ret = vsnprintf(buf, sizeof(buf), format, argList);
    va_end(argList);
    printf("%s", buf);
    return ret;
}

}
