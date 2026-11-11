#include <cstdint>
#include "HLTV/common/InfoString.h"

#include "HLTV/common/mem.h"

#include <climits>
#include <cstddef>
#include <stdlib.h>
#include <string.h>

namespace {

unsigned int InfoStringCapacity(std::size_t length, unsigned int minimum) {
    if (length >= static_cast<std::size_t>(INT_MAX) || minimum > static_cast<unsigned int>(INT_MAX)) {
        return 0;
    }
    const std::size_t required = length + 1;
    return static_cast<unsigned int>(required < minimum ? minimum : required);
}

} // namespace

InfoString::InfoString()
{
	m_String = NULL;
	m_MaxSize = 0;
}

InfoString::InfoString( unsigned int maxSize )
{
	m_String = NULL;
	m_MaxSize = 0;
	SetMaxSize( maxSize );
}

InfoString::InfoString( char* string )
{
    constexpr unsigned int DefaultCapacity = 256;
    m_String = NULL;
    m_MaxSize = 0;

    const unsigned int size = InfoStringCapacity(strlen(string), DefaultCapacity);
    if (size == 0) {
        return;
    }

	SetMaxSize( size );
	SetString( string );
}

InfoString::InfoString( char* string, unsigned int maxSize )
{
    m_String = NULL;
    m_MaxSize = 0;

    const unsigned int size = InfoStringCapacity(strlen(string), maxSize);
    if (size == 0) {
        return;
    }

	SetMaxSize( size );
	SetString( string );
}

InfoString::~InfoString()
{
	if ( m_String )
		free( m_String );
}

bool InfoString::SetString( char* string )
{
	if ( !m_String || !string )
		return false;

	if ( strlen( string ) < (unsigned int)m_MaxSize )
	{
		strncpy( m_String, string, m_MaxSize - 1 );
		m_String[m_MaxSize - 1] = 0;
		return true;
	}

	return false;
}

void InfoString::SetMaxSize( unsigned int maxSize )
{
    if (maxSize > static_cast<unsigned int>(INT_MAX)) {
        return;
    }
	char* buf;
	char* old;

	buf = (char*)Mem_ZeroMalloc( maxSize );
	if ( !buf )
		return;

	old = m_String;
	if ( old )
	{
		if ( maxSize > strlen( old ) )
			strncpy( buf, old, maxSize - 1 );

		free( old );
	}

    m_MaxSize = static_cast<int>(maxSize);
	m_String = buf;
}

int InfoString::GetMaxSize()
{
	return m_MaxSize;
}

int InfoString::GetCurrentSize()
{
	return (int)strlen( m_String );
}

void InfoString::Clear()
{
	if ( !m_String )
		return;

	memset( m_String, 0, m_MaxSize );
}

char* InfoString::GetString()
{
	return m_String;
}

char* InfoString::ValueForKey( const char* key )
{
	static char value[4][512];
	static int valueindex;
	char pkey[512];
	char* s;
	char* out;
	char* pk;
	char* dst;

	s = m_String;
	valueindex = ( valueindex + 1 ) % 4;
	out = value[valueindex];

	if ( *s == '\\' )
		s++;

	for ( ;; )
	{
		pk = pkey;
		if ( *s != '\\' )
		{
			if ( *s == 0 )
				return (char*)"";

			do
			{
				*pk++ = *s++;
				if ( *s == 0 )
					return (char*)"";
			}
			while ( *s != '\\' );
		}

		*pk = 0;
		s++;
		dst = out;
		if ( *s != '\\' )
		{
			while ( *s != 0 && *s != '\\' )
				*dst++ = *s++;
		}

		*dst = 0;
		if ( !strcmp( key, pkey ) )
			return out;

		if ( *s == 0 )
			return (char*)"";

		s++;
	}
}

bool InfoString::RemoveKey( const char* key )
{
	char pkey[512];
	char value[512];
	char* s;
	char* start;
	char* pk;
	char* dst;

	if ( strstr( key, "\\" ) )
		return false;

	s = m_String;
	for ( ;; )
	{
		start = s;
		if ( *s == '\\' )
		{
			s++;
			if ( *s == '\\' )
			{
				pk = pkey;
			}
			else
			{
				if ( *s == 0 )
					return false;

				pk = pkey;
				do
				{
					*pk++ = *s++;
					if ( *s == 0 )
						return false;
				}
				while ( *s != '\\' );
			}
		}
		else
		{
			if ( *s == 0 )
				return false;

			pk = pkey;
			do
			{
				*pk++ = *s++;
				if ( *s == 0 )
					return false;
			}
			while ( *s != '\\' );
		}

		*pk = 0;
		s++;
		dst = value;
		if ( *s != '\\' )
		{
			while ( *s != 0 && *s != '\\' )
				*dst++ = *s++;
		}

		*dst = 0;
		if ( !strcmp( key, pkey ) )
		{
#if INTPTR_MAX > INT32_MAX
            // Removing a key shifts the remaining suffix within this same buffer.
            memmove(start, s, strlen(s) + 1);
#else
			strcpy( start, s );
#endif
			return true;
		}

		if ( *s == 0 )
			return false;
	}
}

void InfoString::RemovePrefixedKeys( char prefix )
{
	char* s;
	char first;
	char* end;
	char stop;

	s = m_String;
	if ( *s != '\\' )
		return;

	s++;
	for ( ;; )
	{
		first = *s;
		if ( first != '\\' )
		{
			if ( first == 0 )
				return;

			end = s;
			do
			{
				end++;
				if ( *end == 0 )
					return;
			}
			while ( *end != '\\' );
		}
		else
		{
			end = s;
		}

		end++;
		stop = *end;
		if ( stop != '\\' )
		{
			while ( stop != 0 && stop != '\\' )
			{
				end++;
				stop = *end;
			}
		}

		if ( first == prefix )
		{
			char ch;
			int i;

			if ( stop == 0 )
				return;

			ch = end[1];
			if ( ch != 0 )
			{
				i = 0;
				do
				{
					s[i] = ch;
					i++;
					ch = end[i + 1];
				}
				while ( ch != 0 );

				stop = *s;
				end = s;
			}
			else
			{
				stop = prefix;
				end = s;
			}
		}

		if ( stop == 0 )
			return;

		s = end + 1;
	}
}

bool InfoString::SetValueForStarKey( const char* key, const char* value )
{
	char newtoken[258];
	char* dst;
	char* v;

	if ( strstr( key, "\\" ) || strstr( value, "\\" ) )
		return false;

	if ( strstr( key, "\"" ) || strstr( value, "\"" ) )
		return false;

	if ( strlen( key ) > 127 )
		return false;

	if ( strlen( value ) > 127 )
		return false;

	RemoveKey( key );
	if ( !value || !value[0] )
		return true;

	strcpy( newtoken, "\\" );
	strcat( newtoken, key );
	strcat( newtoken, "\\" );
	strcat( newtoken, value );

    const std::size_t currentLength = strlen(m_String);
    const std::size_t tokenLength = strlen(newtoken);
    if (currentLength >= static_cast<std::size_t>(m_MaxSize)
        || tokenLength >= static_cast<std::size_t>(m_MaxSize) - currentLength) {
        return false;
    }

	dst = m_String + strlen( m_String );
	v = newtoken;
	while ( *v )
	{
		unsigned char c;

		c = (unsigned char)( *v & 127 );
		v++;
		if ( c > 13 )
			*dst++ = (char)c;
	}

	*dst = 0;
	return true;
}

bool InfoString::SetValueForKey( const char* key, const char* value )
{
	return SetValueForStarKey( key, value );
}
