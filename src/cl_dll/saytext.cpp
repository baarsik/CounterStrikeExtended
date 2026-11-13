/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// saytext.cpp
//
// implementation of CHudSayText class
//

#include "hud.h"
#include "localization_client.h"
#include "../common/legacy_text_encoding.h"
#include <set>
#include <vector>
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>
#if defined(_WIN32)
#include <malloc.h>
#endif // _alloca

#include "vgui_TeamFortressViewport.h"

extern float *GetClientColor( int clientIndex );

#define MAX_LINES	5
#define MAX_CHARS_PER_LINE	256  /* it can be less than this, depending on char size */

// allow 20 pixels on either side of the text
#define MAX_LINE_WIDTH  ( ScreenWidth - 40 )
#define LINE_START  10
static float SCROLL_SPEED = 5;

static char g_szLineBuffer[ MAX_LINES + 1 ][ MAX_CHARS_PER_LINE ];
static float *g_pflNameColors[ MAX_LINES + 1 ];
static int g_iNameLengths[ MAX_LINES + 1 ];
static std::shared_ptr<HudTextSource> g_lineSources[MAX_LINES + 1];
static float flScrollTime = 0;  // the time at which the lines next scroll up

static int Y_START = 0;
static int line_height = 0;

DECLARE_MESSAGE( m_SayText, SayText );

int CHudSayText :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( SayText );

	InitHUDData();

	m_HUD_saytext =			gEngfuncs.pfnRegisterVariable( "hud_saytext", "1", 0 );
	m_HUD_saytext_time =	gEngfuncs.pfnRegisterVariable( "hud_saytext_time", "5", 0 );

	m_iFlags |= HUD_INTERMISSION; // is always drawn during an intermission

	return 1;
}


void CHudSayText :: InitHUDData( void )
{
	memset( g_szLineBuffer, 0, sizeof g_szLineBuffer );
	memset( g_pflNameColors, 0, sizeof g_pflNameColors );
	memset( g_iNameLengths, 0, sizeof g_iNameLengths );
    for (auto& source : g_lineSources) {
        source.reset();
    }
}

int CHudSayText :: VidInit( void )
{
	return 1;
}


int ScrollTextUp( void )
{
    if (g_lineSources[0]) {
        ++g_lineSources[0]->discardedLines;
    }
    for (int index = 0; index < MAX_LINES; ++index) {
        g_lineSources[index] = std::move(g_lineSources[index + 1]);
    }
    g_lineSources[MAX_LINES].reset();
	ConsolePrint( g_szLineBuffer[0] ); // move the first line into the console buffer
	g_szLineBuffer[MAX_LINES][0] = 0;
	memmove( g_szLineBuffer, g_szLineBuffer + 1, sizeof(g_szLineBuffer) - sizeof(g_szLineBuffer[0]) ); // overwrite the first line
	memmove( &g_pflNameColors[0], &g_pflNameColors[1], sizeof(g_pflNameColors) - sizeof(g_pflNameColors[0]) );
	memmove( &g_iNameLengths[0], &g_iNameLengths[1], sizeof(g_iNameLengths) - sizeof(g_iNameLengths[0]) );
	g_szLineBuffer[MAX_LINES-1][0] = 0;

	if ( g_szLineBuffer[0][0] == ' ' ) // also scroll up following lines
	{
		g_szLineBuffer[0][0] = 2;
		return 1 + ScrollTextUp();
	}

	return 1;
}

int CHudSayText :: Draw( float flTime )
{
	int y = Y_START;

	if ( ( gViewPort && gViewPort->AllowedToPrintText() == FALSE) || !m_HUD_saytext->value )
		return 1;

	// make sure the scrolltime is within reasonable bounds,  to guard against the clock being reset
	flScrollTime = min( flScrollTime, flTime + m_HUD_saytext_time->value );

	// make sure the scrolltime is within reasonable bounds,  to guard against the clock being reset
	flScrollTime = min( flScrollTime, flTime + m_HUD_saytext_time->value );

	if ( flScrollTime <= flTime )
	{
		if ( *g_szLineBuffer[0] )
		{
			flScrollTime = flTime + m_HUD_saytext_time->value;
			// push the console up
			ScrollTextUp();
		}
		else
		{ // buffer is empty,  just disable drawing of this section
			m_iFlags &= ~HUD_ACTIVE;
		}
	}

	for ( int i = 0; i < MAX_LINES; i++ )
	{
		if ( *g_szLineBuffer[i] )
		{
			if ( *g_szLineBuffer[i] == 2 && g_pflNameColors[i] )
			{
				// it's a saytext string
				char buf[MAX_CHARS_PER_LINE];
#if defined(_WIN32)
				int lineLength = strlen( g_szLineBuffer[i] );
#else
                // Stored display lines are bounded by MAX_CHARS_PER_LINE.
                int lineLength = static_cast<int>(strlen(g_szLineBuffer[i]));
#endif
				if ( lineLength < 0 || lineLength >= MAX_CHARS_PER_LINE )
					continue;

				int nameLength = max( 0, min( g_iNameLengths[i], lineLength ) );
				int prefixLength = min( nameLength, MAX_PLAYER_NAME_LENGTH + 31 );
				memcpy( buf, g_szLineBuffer[i], prefixLength );
				buf[prefixLength] = 0;
				gEngfuncs.pfnDrawSetTextColor( g_pflNameColors[i][0], g_pflNameColors[i][1], g_pflNameColors[i][2] );
				int x = LINE_START;
				if ( prefixLength > 0 )
					x = DrawConsoleString( LINE_START, y, buf + 1 );
				int textLength = lineLength - nameLength;
				if ( textLength > 0 )
					textLength--;
				if ( textLength < 0 || textLength >= MAX_CHARS_PER_LINE )
					continue;

				memcpy( buf, g_szLineBuffer[i] + nameLength, textLength );
				buf[textLength] = 0;
				// color is reset after each string draw
				DrawConsoleString( x, y, buf );
			}
			else
			{
				// normal draw
				DrawConsoleString( LINE_START, y, g_szLineBuffer[i] );
			}
		}

		y += line_height;
	}

	return 1;
}

int CHudSayText :: MsgFunc_SayText( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int client_index = READ_BYTE();		// the client who spoke the message
	SayTextPrint( READ_STRING(), iSize - 1,  client_index );
	
	return 1;
}

void CHudSayText::SayTextPrint(const char* pszBuf, int iBufSize, int clientIndex,
    std::shared_ptr<HudTextSource> localization)
{
	if ( !pszBuf || iBufSize <= 0 )
		return;

	if ( gViewPort && gViewPort->AllowedToPrintText() == FALSE )
	{
		// Print it straight to the console
		ConsolePrint( pszBuf );
		return;
	}

	int i;
	// find an empty string slot
	for ( i = 0; i < MAX_LINES; i++ )
	{
		if ( ! *g_szLineBuffer[i] )
			break;
	}
	if ( i == MAX_LINES )
	{
		// force scroll buffer up
		ScrollTextUp();
		i = MAX_LINES - 1;
	}

    g_lineSources[i] = std::move(localization);
	g_iNameLengths[i] = 0;
	g_pflNameColors[i] = NULL;

	// if it's a say message, search for the players name in the string
	if ( *pszBuf == 2 && clientIndex > 0 && clientIndex <= MAX_PLAYERS )
	{
		gEngfuncs.pfnGetPlayerInfo( clientIndex, &g_PlayerInfoList[clientIndex] );
		const char *pName = g_PlayerInfoList[clientIndex].name;

		if ( pName )
		{
			const char *nameInString = strstr( pszBuf, pName );

			if ( nameInString )
			{
#if defined(_WIN32)
				g_iNameLengths[i] = strlen( pName ) + (nameInString - pszBuf);
#else
                // This is a matched name's byte offset within the bounded message,
                // rather than an address or native allocation size.
                g_iNameLengths[i] = static_cast<int>(strlen(pName) + (nameInString - pszBuf));
#endif
				g_pflNameColors[i] = GetClientColor( clientIndex );
			}
		}
	}

	int copyLength = min( iBufSize, MAX_CHARS_PER_LINE - 1 );
	strncpy( g_szLineBuffer[i], pszBuf, copyLength );
	g_szLineBuffer[i][copyLength] = 0;

	// make sure the text fits in one line
	EnsureTextFitsInOneLineAndWrapIfHaveTo( i );

	// Set scroll time
	if ( i == 0 )
	{
		flScrollTime = gHUD.m_flTime + m_HUD_saytext_time->value;
	}

	m_iFlags |= HUD_ACTIVE;
	PlaySound( "misc/talk.wav", 1 );

	Y_START = ScreenHeight - 60 - ( line_height * (MAX_LINES+2) );
}

void CHudSayText :: EnsureTextFitsInOneLineAndWrapIfHaveTo( int line )
{
	int line_width = 0;
	GetConsoleStringSize( g_szLineBuffer[line], &line_width, &line_height );

	if ( (line_width + LINE_START) > MAX_LINE_WIDTH )
	{ // string is too long to fit on line
		// scan the string until we find what word is too long,  and wrap the end of the sentence after the word
		int length = LINE_START;
		int tmp_len = 0;
		char *last_break = NULL;
		for ( char *x = g_szLineBuffer[line]; *x != 0; x++ )
		{
			// check for a color change, if so skip past it
			if ( x[0] == '/' && x[1] == '(' )
			{
				x += 2;
				// skip forward until past mode specifier
				while ( *x != 0 && *x != ')' )
					x++;

				if ( *x != 0 )
					x++;

				if ( *x == 0 )
					break;
			}

			char buf[2];
			buf[1] = 0;

			if ( *x == ' ' && x != g_szLineBuffer[line] )  // store each line break,  except for the very first character
				last_break = x;

			buf[0] = *x;  // get the length of the current character
			GetConsoleStringSize( buf, &tmp_len, &line_height );
			length += tmp_len;

			if ( length > MAX_LINE_WIDTH )
			{  // needs to be broken up
				if ( !last_break )
				{
					if ( x == g_szLineBuffer[line] )
						return;
					last_break = x-1;
				}

				x = last_break;

				// find an empty string slot
				int j;
				do 
				{
					for ( j = 0; j < MAX_LINES; j++ )
					{
						if ( ! *g_szLineBuffer[j] )
							break;
					}
					if ( j == MAX_LINES )
					{
						// need to make more room to display text, scroll stuff up then fix the pointers
						int linesmoved = ScrollTextUp();
						line -= linesmoved;
						if ( line < 0 )
							return;

						last_break = last_break - (sizeof(g_szLineBuffer[0]) * linesmoved);
					}
				}
				while ( j == MAX_LINES );

				// copy remaining string into next buffer,  making sure it starts with a space character
				if ( (char)*last_break == (char)' ' )
				{
#if defined(_WIN32)
					int linelen = strlen(g_szLineBuffer[j]);
					int remaininglen = strlen(last_break);
#else
                    // Both strings point into the retained fixed-size display slots.
                    int linelen = static_cast<int>(strlen(g_szLineBuffer[j]));
                    int remaininglen = static_cast<int>(strlen(last_break));
#endif

					if ( linelen + remaininglen < MAX_CHARS_PER_LINE )
						strcat( g_szLineBuffer[j], last_break );
				}
				else
				{
					if ( strlen(g_szLineBuffer[j]) + strlen(last_break) + 1 < MAX_CHARS_PER_LINE )
					{
						strcat( g_szLineBuffer[j], " " );
						strcat( g_szLineBuffer[j], last_break );
					}
				}

                g_lineSources[j] = g_lineSources[line];
				*last_break = 0; // cut off the last string

				EnsureTextFitsInOneLineAndWrapIfHaveTo( j );
				break;
			}
		}
	}
}

namespace {
std::vector<std::string> WrapLocalizedSayText(std::string remaining) {
    std::vector<std::string> lines;
    while (!remaining.empty()) {
        std::size_t lastSpace = std::string::npos;
        std::size_t boundary = remaining.size();
        for (std::size_t offset = 0; offset < remaining.size();) {
            const auto character = cse::legacy_text::detail::DecodeUtf8(
                reinterpret_cast<const unsigned char*>(remaining.c_str() + offset));
            const std::size_t next = offset + character.consumed;
            if (remaining[offset] == ' ' && offset > 0) {
                lastSpace = offset;
            }
            int wide = 0;
            const std::string prefix = remaining.substr(0, next);
            GetConsoleStringSize(prefix.c_str(), &wide, &line_height);
            if (wide + LINE_START > MAX_LINE_WIDTH && offset > 0) {
                boundary = lastSpace != std::string::npos ? lastSpace : offset;
                if (boundary == 1 && remaining[0] == ' ') {
                    boundary = next;
                }
                break;
            }
            offset = next;
        }
        lines.push_back(remaining.substr(0, boundary));
        remaining.erase(0, boundary);
        if (!remaining.empty() && remaining[0] != ' ') {
            remaining.insert(remaining.begin(), ' ');
        }
    }
    return lines;
}
}

void CHudSayText::RefreshLanguage() {
    struct DisplayLine {
        std::string text;
        std::shared_ptr<HudTextSource> source;
        float* color;
        int nameLength;
    };
    std::vector<DisplayLine> lines;
    std::set<HudTextSource*> seen;
    for (int index = 0; index < MAX_LINES; ++index) {
        const auto& source = g_lineSources[index];
        if (!g_szLineBuffer[index][0]) {
            continue;
        }
        if (!source) {
            lines.push_back({g_szLineBuffer[index], {}, g_pflNameColors[index], g_iNameLengths[index]});
            continue;
        }
        if (!seen.insert(source.get()).second) {
            continue;
        }
        const std::string translated = source->Render();
        if (translated.empty()) {
            for (int row = index; row < MAX_LINES && g_lineSources[row] == source; ++row) {
                lines.push_back({g_szLineBuffer[row], source, g_pflNameColors[row], g_iNameLengths[row]});
            }
            continue;
        }
        const auto wrapped = WrapLocalizedSayText(translated);
        for (std::size_t row = static_cast<std::size_t>(source->discardedLines); row < wrapped.size(); ++row) {
            lines.push_back({wrapped[row], source, nullptr, 0});
        }
    }
    const std::size_t first = lines.size() > MAX_LINES ? lines.size() - MAX_LINES : 0;
    for (std::size_t index = 0; index < first; ++index) {
        if (lines[index].source) {
            ++lines[index].source->discardedLines;
        }
    }
    for (int row = 0; row <= MAX_LINES; ++row) {
        g_szLineBuffer[row][0] = 0;
        g_pflNameColors[row] = nullptr;
        g_iNameLengths[row] = 0;
        g_lineSources[row].reset();
        const std::size_t index = first + static_cast<std::size_t>(row);
        if (row < MAX_LINES && index < lines.size()) {
            strncpy(g_szLineBuffer[row], lines[index].text.c_str(), MAX_CHARS_PER_LINE - 1);
            g_szLineBuffer[row][MAX_CHARS_PER_LINE - 1] = 0;
            g_pflNameColors[row] = lines[index].color;
            g_iNameLengths[row] = lines[index].nameLength;
            g_lineSources[row] = lines[index].source;
        }
    }
    // Reflow preserves flScrollTime, flags, sound and console history.
    Y_START = ScreenHeight - 60 - line_height * (MAX_LINES + 2);
}
