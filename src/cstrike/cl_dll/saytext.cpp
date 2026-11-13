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
#include "cl_util.h"
#include "parsemsg.h"
#include "../public/game_controls/iviewport.h"
#include "../public/utlvector.h"
#include "../../engine/controls.h"
#include "../../external/steamworks/isteamfriends.h"
#include <wchar.h>
#include <wctype.h>
#include <memory>
#include <algorithm>

#define MAX_LINES 5
#define MAX_CHARS_PER_LINE 256  /* it can be less than this, depending on char size */
#define LINE_START 10

// allow 20 pixels on either side of the text
#define MAX_LINE_WIDTH ( ScreenWidth - 40 )

enum TextColor
{
	COLOR_MIN,
	COLOR_NORMAL,
	COLOR_USEOLDCOLORS,
	COLOR_PLAYERNAME,
	COLOR_LOCATION,
	COLOR_MAX
};

struct TextRange
{
	int start;
	int end;
	float* color;
};

// Only structured catalogue templates are rerendered; arguments remain owned payloads.
struct ChatSource {
    std::string token;
    std::string titleKey;
    std::string argumentTitleKeys[4];
    std::wstring arguments[4];
    std::string argumentTokens[4];
    std::wstring fallback;
    int client = 0;
    std::size_t discardedLines = 0;
};

std::wstring RenderChatSource(const ChatSource& source) {
    std::string token = source.token;
    if (!source.titleKey.empty()) {
        const client_textmessage_t* title = TextMessageGet(source.titleKey.c_str());
        if (title && title->pMessage) {
            token = title->pMessage;
            while (!token.empty() && (token.back() == '\n' || token.back() == '\r')) {
                token.pop_back();
            }
        }
    }
    const wchar_t* format = token.empty() ? nullptr : vgui2::localize()->Find(token.c_str());
    wchar_t literalFormat[MAX_CHARS_PER_LINE]{};
    if (!format && !source.titleKey.empty()) {
        vgui2::localize()->ConvertANSIToUnicode(token.c_str(), literalFormat, sizeof(literalFormat));
        format = literalFormat;
    }
    if (!format) {
        return source.fallback;
    }
    const wchar_t* arguments[4]{};
    wchar_t titleArguments[4][MAX_CHARS_PER_LINE]{};
    for (int index = 0; index < 4; ++index) {
        if (!source.argumentTitleKeys[index].empty()) {
            const client_textmessage_t* title = TextMessageGet(source.argumentTitleKeys[index].c_str());
            if (title && title->pMessage) {
                arguments[index] = vgui2::localize()->Find(title->pMessage);
                if (!arguments[index]) {
                    vgui2::localize()->ConvertANSIToUnicode(title->pMessage, titleArguments[index],
                        sizeof(titleArguments[index]));
                    arguments[index] = titleArguments[index];
                }
            }
        }
        if (!arguments[index] && !source.argumentTokens[index].empty()) {
            arguments[index] = vgui2::localize()->Find(source.argumentTokens[index].c_str());
        }
        if (!arguments[index]) {
            arguments[index] = source.arguments[index].c_str();
        }
    }
    wchar_t text[MAX_CHARS_PER_LINE]{};
    if (format == literalFormat) {
        std::wstring expanded;
        int sequential = 0;
        for (std::size_t index = 0; format[index] && expanded.size() < MAX_CHARS_PER_LINE - 1; ++index) {
            if (format[index] == L'%' && format[index + 1] == L'%') {
                expanded += L'%';
                ++index;
            } else if (format[index] == L'%' && format[index + 1] == L's') {
                index += 1;
                int argument = sequential++;
                if (format[index + 1] >= L'1' && format[index + 1] <= L'4') {
                    argument = format[++index] - L'1';
                }
                if (argument < 4) {
                    expanded += arguments[argument];
                }
            } else {
                expanded += format[index];
            }
        }
        wcsncpy(text, expanded.c_str(), MAX_CHARS_PER_LINE - 1);
    } else {
        vgui2::localize()->ConstructString(text, sizeof(text), const_cast<wchar_t*>(format), 4,
            arguments[0], arguments[1], arguments[2], arguments[3]);
    }
    text[MAX_CHARS_PER_LINE - 1] = 0;
    return text;
}

std::vector<std::wstring> WrapChatSource(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        int width = LINE_START;
        std::size_t end = start;
        std::size_t wordBreak = std::wstring::npos;
        while (end < text.size() && end - start < MAX_CHARS_PER_LINE - 2) {
            wchar_t character[2] = {text[end], 0};
            int characterWidth = 0;
            int height = 0;
            gHUD.m_VGUI2Print.GetStringSize(character, &characterWidth, &height);
            if (end > start && (width + characterWidth >= MAX_LINE_WIDTH || text[end] == L'\n')) {
                break;
            }
            width += characterWidth;
            if (text[end] == L' ' && end > start) {
                wordBreak = end;
            }
            ++end;
        }
        if (end < text.size() && wordBreak != std::wstring::npos) {
            end = wordBreak;
        }
        if (end == start) {
            ++end;
        }
        std::wstring line = text.substr(start, end - start);
        if (start > 0) {
            line.insert(line.begin(), L' ');
        }
        lines.push_back(line);
        start = end;
        while (start < text.size() && (text[start] == L' ' || text[start] == L'\n')) {
            ++start;
        }
    }
    return lines;
}

extern float* GetClientColor(int client);

class SayTextLine
{
public:
	SayTextLine();
	void SetText( wchar_t* text, int client );
	void Colorize();
	wchar_t* GetText() { return m_line; }
	void Clear();
	void Draw( int x, int y, float r, float g, float b );
	SayTextLine& operator=( const SayTextLine& other );
    const std::shared_ptr<ChatSource>& GetSource() const {
        return m_source;
    }
    int GetClientIndex() const {
        return m_clientIndex;
    }
    void SetSource(const std::shared_ptr<ChatSource>& source) {
        m_source = source;
        if (source) {
            m_clientIndex = source->client;
            m_teamColor = m_clientIndex > 0 ? GetClientColor(m_clientIndex) : nullptr;
        }
    }

private:
	wchar_t m_line[MAX_CHARS_PER_LINE];
	CUtlVector<TextRange> m_textRanges;
	int m_clientIndex = 0;
    float* m_teamColor = nullptr;
    std::shared_ptr<ChatSource> m_source;
};

extern float* GetClientColor( int client );
extern float g_LocationColor[3];

float* GetTextColor( TextColor color, int client )
{
	if ( color == COLOR_PLAYERNAME )
		return GetClientColor( client );
	return color == COLOR_LOCATION ? g_LocationColor : NULL;
}

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
SayTextLine::SayTextLine()
{
	m_line[0] = 0;
}

void SayTextLine::Clear()
{
	m_line[0] = 0;
    m_source.reset();
	m_textRanges.RemoveAll();
}

SayTextLine& SayTextLine::operator=( const SayTextLine& other )
{
	memcpy(m_line, other.m_line, sizeof(m_line));
    m_source = other.m_source;
    m_clientIndex = other.m_clientIndex;
    m_teamColor = other.m_teamColor;
	m_textRanges.RemoveAll();
	for ( int i = 0; i < other.m_textRanges.Count(); i++ )
		m_textRanges.AddToTail( other.m_textRanges[i] );
	return *this;
}

void SayTextLine::SetText( wchar_t* text, int client )
{
	Clear();
	if ( !text )
		return;
	wcsncpy( m_line, text, MAX_CHARS_PER_LINE );
	m_line[MAX_CHARS_PER_LINE - 1] = 0;
	m_clientIndex = client;
	m_teamColor = NULL;
	if ( client <= 0 )
	{
		TextRange range = { 0, (int)wcslen( m_line ), NULL };
		m_textRanges.AddToTail( range );
	}
	else
	{
		gEngfuncs.pfnGetPlayerInfo( client, &g_PlayerInfoList[client] );
		m_teamColor = GetClientColor( client );
		Colorize();
	}
}

void SayTextLine::Colorize()
{
	m_textRanges.RemoveAll();
	const int length = static_cast<int>(wcslen(m_line));
	if ( m_line[0] == COLOR_NORMAL || m_line[0] == COLOR_PLAYERNAME || m_line[0] == COLOR_LOCATION )
	{
		for ( int i = 0; m_line[i]; i++ )
		{
			if ( m_line[i] == COLOR_NORMAL || m_line[i] == COLOR_PLAYERNAME || m_line[i] == COLOR_LOCATION )
			{
				TextRange range = { i + 1, length, NULL };
				if ( m_line[i] == COLOR_PLAYERNAME )
					range.color = m_teamColor;
				else if ( m_line[i] == COLOR_LOCATION )
					range.color = g_LocationColor;
				if ( m_textRanges.Count() )
					m_textRanges[m_textRanges.Count() - 1].end = i;
				m_textRanges.AddToTail( range );
			}
		}
	}
	else if ( m_line[0] == COLOR_USEOLDCOLORS )
	{
		const char* name = g_PlayerInfoList[m_clientIndex].name;
		wchar_t unicodeName[128];
		vgui2::localize()->ConvertANSIToUnicode( name, unicodeName, sizeof( unicodeName ) );
		if ( name )
		{
			wchar_t* found = wcsstr( m_line, unicodeName );
			if ( found )
			{
				TextRange range = { 1, (int)( found - m_line + wcslen( unicodeName ) ), GetClientColor( m_clientIndex ) };
				m_textRanges.AddToTail( range );
				range.start = range.end;
				range.end = length;
				range.color = NULL;
				m_textRanges.AddToTail( range );
			}
		}
	}
	if ( !m_textRanges.Count() )
	{
		TextRange range = { 0, (int)wcslen( m_line ), NULL };
		m_textRanges.AddToTail( range );
	}
}

void SayTextLine::Draw( int x, int y, float r, float g, float b )
{
	if ( !m_textRanges.Count() )
	{
		gHUD.m_VGUI2Print.DrawVGUI2String( m_line, x, y, r, g, b );
		return;
	}
	for ( int i = 0; i < m_textRanges.Count(); i++ )
	{
		TextRange& range = m_textRanges[i];
		wchar_t saved = m_line[range.end];
		m_line[range.end] = 0;
		if ( range.color )
			x = gHUD.m_VGUI2Print.DrawVGUI2String( m_line + range.start, x, y, range.color[0], range.color[1], range.color[2] );
		else
			x = gHUD.m_VGUI2Print.DrawVGUI2String( m_line + range.start, x, y, r, g, b );
		m_line[range.end] = saved;
	}
}

static SayTextLine g_sayTextLine[MAX_LINES + 1];
static std::weak_ptr<ChatSource> g_lastChatSource;
static float flScrollTime;
static int Y_START;
static int line_height;

DECLARE_MESSAGE( m_SayText, SayText );
DECLARE_MESSAGE( m_SayText, SendAudio );

void HudSayTextToggle()
{
	if ( gHUD.m_SayText.m_HUD_saytext->value == 0 )
		gEngfuncs.Cvar_SetValue( "hud_saytext_internal", 1 );
	else
		gEngfuncs.Cvar_SetValue( "hud_saytext_internal", 0 );
}

int CHudSayText::Init()
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( SayText );
	HOOK_MESSAGE( SendAudio );
	InitHUDData();
	gEngfuncs.pfnAddCommand( "hud_saytext", HudSayTextToggle );
	m_HUD_saytext = CVAR_CREATE( "hud_saytext_internal", "1", FCVAR_ARCHIVE );
	m_HUD_saytext_time = CVAR_CREATE( "hud_saytext_time", "5", 0 );
	m_iFlags |= HUD_INTERMISSION; // is always drawn during an intermission
	return 1;
}

void CHudSayText::InitHUDData()
{
	for ( int i = 0; i <= MAX_LINES; i++ )
		g_sayTextLine[i].Clear();
}

int CHudSayText::VidInit()
{
	return 1;
}

int ScrollTextUp()
{
    if (g_sayTextLine[0].GetSource()) {
        ++g_sayTextLine[0].GetSource()->discardedLines;
    }
	if ( *g_sayTextLine[0].GetText() )
	{
		char buffer[512];
		vgui2::localize()->ConvertUnicodeToANSI( g_sayTextLine[0].GetText(), buffer, sizeof( buffer ) );
		strcat( buffer, "\n" );
		char* destination = buffer;
		for ( char* source = buffer; *source; source++ )
		{
			if ( (unsigned char)*source > COLOR_MAX )
				*destination++ = *source;
		}
		*destination = 0;
		ConsolePrint( buffer );
	}
	g_sayTextLine[MAX_LINES].Clear();
	for ( int i = 0; i < MAX_LINES - 1; i++ )
		g_sayTextLine[i] = g_sayTextLine[i + 1];
	g_sayTextLine[MAX_LINES - 1].Clear();
	if ( g_sayTextLine[0].GetText()[0] == ' ' )
	{
		g_sayTextLine[0].GetText()[0] = COLOR_USEOLDCOLORS;
		return 1 + ScrollTextUp();
	}
	return 1;
}

int CHudSayText::GetTextPrintY()
{
	int y = ScreenHeight;
	if ( g_iUser1 || gEngfuncs.IsSpectateOnly() )
		y -= gViewPortInterface->GetSpectatorBottomBarHeight() + 4;
	else
		y -= (int)( gHUD.m_iFontHeight * 2.75 );
	return y - ( MAX_LINES * line_height + (int)( line_height * 0.5 ) );
}

int CHudSayText::Draw( float time )
{
	if ( ( gViewPortInterface && !gViewPortInterface->AllowedToPrintText() ) || !m_HUD_saytext->value )
		return 1;
	float next = time + m_HUD_saytext_time->value;
	if ( flScrollTime > next )
		flScrollTime = next;
	if ( time >= flScrollTime )
	{
		if ( *g_sayTextLine[0].GetText() )
		{
			flScrollTime = next;
			// push the console up
			ScrollTextUp();
		}
		else
			m_iFlags &= ~HUD_ACTIVE;
	}
	DrawConsoleString( LINE_START, Y_START, "" );
	int r, g, b;
	if ( sscanf( CVAR_GET_STRING( "con_color" ), "%i %i %i", &r, &g, &b ) != 3 )
		r = g = b = 0;
	float red = r / 255.0;
	float green = g / 255.0;
	float blue = b / 255.0;
	int y = Y_START;
	for ( int i = 0; i < MAX_LINES; i++ )
	{
		if ( *g_sayTextLine[i].GetText() )
			g_sayTextLine[i].Draw( LINE_START, y, red, green, blue );
		y += line_height;
	}
	return 1;
}

void CHudSayText::FindAudMessage( const char* text, int, int pitch )
{
	char sentence[32] = "misc/talk.wav";
	char* found = (char*)strstr( text, "%!" );
	if ( !found )
	{
		gEngfuncs.pfnPlaySoundVoiceByName( (char*)text, 1, pitch );
		return;
	}
	const char* source = found + 1;
	int length = 0;
	while ( length < 31 && (unsigned char)( *source - 32 ) <= 'z' - 32 )
		sentence[length++] = *source++;
	sentence[length] = 0;
	strcpy( found, source );
	gEngfuncs.pfnPlaySoundByNameAtPitch( sentence, 1, pitch );
}

int CHudSayText::MsgFunc_SendAudio( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int client = READ_BYTE();
	char* text = READ_STRING();
	int pitch = READ_SHORT();
	FindAudMessage( text, size, pitch );
	g_PlayerExtraInfo[client].radarflashon = 1;
	g_PlayerExtraInfo[client].radarflash = gHUD.m_flTime;
	g_PlayerExtraInfo[client].radarflashes = 22;
	return 1;
}

void CHudSayText::EnsureTextFitsInOneLineAndWrapIfHaveTo( int line )
{
	SayTextLine& string = g_sayTextLine[line];
	int width = 0;
	gHUD.m_VGUI2Print.GetStringSize( string.GetText(), &width, &line_height );
	if ( width + 9 < MAX_LINE_WIDTH )
		return;
	wchar_t* lastBreak = NULL;
	int length = LINE_START;
	for ( wchar_t* scan = string.GetText(); *scan; scan++ )
	{
		if ( *scan == ' ' && scan != string.GetText() )
			lastBreak = scan;
		wchar_t character[2] = { *scan, 0 };
		int charWidth;
		gHUD.m_VGUI2Print.GetStringSize( character, &charWidth, &line_height );
		length += charWidth;
		if ( length < MAX_LINE_WIDTH + 1 )
			continue;
		if ( !lastBreak )
			lastBreak = scan - 1;
		string.Colorize();
		int next;
		do
		{
			for ( next = 0; next < MAX_LINES && *g_sayTextLine[next].GetText(); next++ )
				;
			if ( next == MAX_LINES )
				lastBreak -= 4 * ScrollTextUp();
		}
		while ( next == MAX_LINES );
		wchar_t* output = g_sayTextLine[next].GetText();
		if ( *lastBreak == ' ' )
		{
			if ( (int)( wcslen( output ) - wcslen( lastBreak ) ) <= MAX_CHARS_PER_LINE )
			{
				wcsncat( output, lastBreak, MAX_CHARS_PER_LINE );
				output[MAX_CHARS_PER_LINE - 1] = 0;
				g_sayTextLine[next].SetSource(string.GetSource());
                g_sayTextLine[next].Colorize();
			}
		}
		else if ( wcslen( output ) - 2 - wcslen( lastBreak ) <= MAX_CHARS_PER_LINE - 1 )
		{
			wcsncat( output, L" ", MAX_CHARS_PER_LINE );
			wcsncat( output, lastBreak, MAX_CHARS_PER_LINE );
			output[MAX_CHARS_PER_LINE - 1] = 0;
			g_sayTextLine[next].SetSource(string.GetSource());
                g_sayTextLine[next].Colorize();
		}
		*lastBreak = 0;
		EnsureTextFitsInOneLineAndWrapIfHaveTo( next );
		break;
	}
}

void CHudSayText::SayTextPrint( const char* text, int, int client, char* arg1, char* arg2, char* arg3, char* arg4 )
{
    g_lastChatSource.reset();
	if ( gViewPortInterface && !gViewPortInterface->AllowedToPrintText() )
		return;
	if ( (unsigned int)( client - 1 ) < MAX_PLAYERS )
	{
		gEngfuncs.pfnGetPlayerInfo( client, &g_PlayerInfoList[client] );
		if ( SteamFriends() )
		{
			EFriendRelationship relationship = SteamFriends()->GetFriendRelationship( CSteamID( g_PlayerInfoList[client].m_nSteamID ) );
			if ( relationship == k_EFriendRelationshipBlocked || relationship == k_EFriendRelationshipIgnored
				|| relationship == k_EFriendRelationshipIgnoredFriend )
				return;
		}
	}
	else
		SteamFriends();
	int line;
	for ( line = 0; line < MAX_LINES && *g_sayTextLine[line].GetText(); line++ )
		;
	if ( line == MAX_LINES )
	{
		ScrollTextUp();
		line = MAX_LINES - 1;
	}
	wchar_t finalBuffer[256] = { 0 };
	char buffer[512] = { 0 };
    size_t length = strlen(text);
    while (length > 0 && (text[length - 1] == '\r' || text[length - 1] == '\n')) {
        --length;
    }
    if (text[length] != 0) {
        const size_t copyLength = length < sizeof(buffer) ? length : sizeof(buffer) - 1;
        memcpy(buffer, text, copyLength);
        buffer[copyLength] = 0;
        text = buffer;
    }
	wchar_t* localized = vgui2::localize()->Find( text );
	wchar_t temp[256] = { 0 };
	wchar_t output[256] = { 0 };
	if ( !localized )
	{
		vgui2::localize()->ConvertANSIToUnicode( text, temp, sizeof( temp ) );
		wchar_t word[128] = { 0 };
		char token[128];
		bool inWord = false;
		wchar_t* wordStart = NULL;
        size_t count = 0;
        size_t remaining = wcslen(temp);
        constexpr size_t OutputCharacters = sizeof(output) / sizeof(output[0]);
        for (wchar_t* scan = temp; *scan && count < OutputCharacters; scan++, remaining--) {
			if ( inWord )
			{
				if ( remaining == 1 || iswspace( *scan ) )
				{
                    const size_t wordLength = static_cast<size_t>(scan - wordStart) + (remaining == 1);
                    constexpr size_t WordCharacters = sizeof(word) / sizeof(word[0]);
                    const size_t wordCopy = wordLength < WordCharacters ? wordLength : WordCharacters - 1;
                    wcsncpy(word, wordStart, wordCopy);
                    word[wordCopy] = 0;
					vgui2::localize()->ConvertUnicodeToANSI( word, token, sizeof( token ) );
					wchar_t* translated = vgui2::localize()->Find( token );
					if ( !translated )
						translated = word;
                    size_t copy = wcslen(translated);
                    if (copy > OutputCharacters - count) {
                        copy = OutputCharacters - count;
                    }
                    wcsncpy(output + count, translated, copy);
                    count += copy;
                    if (remaining > 1 && count < OutputCharacters) {
                        output[count++] = *scan;
                    }
					inWord = false;
				}
			}
			else if ( *scan == '#' )
			{
				wordStart = scan;
				inWord = true;
			}
			else
				output[count++] = *scan;
		}
        output[count < OutputCharacters ? count : OutputCharacters - 1] = 0;
		localized = output;
	}
	char* args[4] = { arg1, arg2, arg3, arg4 };
	static wchar_t wstr[4][256];
	wchar_t* unicodeArgs[4] = { NULL, NULL, NULL, NULL };
	for ( int i = 0; i < 4; i++ )
	{
		const bool playerChat = strncmp(text, "#Cstrike_Chat_", strlen("#Cstrike_Chat_")) == 0;
        if (i > 0 && !playerChat && *args[i] == '#') {
            unicodeArgs[i] = vgui2::localize()->Find(args[i]);
        }
		if ( !unicodeArgs[i] )
		{
			vgui2::localize()->ConvertANSIToUnicode( args[i], wstr[i], sizeof( wstr[i] ) );
			unicodeArgs[i] = wstr[i];
		}
	}
	wchar_t* format = wcsstr( localized, L"%s" );
	if ( format && format[2] >= '0' && format[2] <= '9' )
		vgui2::localize()->ConstructString( finalBuffer, sizeof( finalBuffer ), localized, 4,
			unicodeArgs[0], unicodeArgs[1], unicodeArgs[2], unicodeArgs[3] );
	else
	{
		_snwprintf( finalBuffer, ARRAYSIZE( finalBuffer ), localized,
			unicodeArgs[0], unicodeArgs[1], unicodeArgs[2], unicodeArgs[3] );
	}
	g_sayTextLine[line].SetText(finalBuffer, client);
    {
        auto source = std::make_shared<ChatSource>();
        if (text[0] == '#' && vgui2::localize()->Find(text)) {
            source->token = text;
        }
        source->client = client;
        source->fallback = finalBuffer;
        const bool playerChat = strncmp(text, "#Cstrike_Chat_", strlen("#Cstrike_Chat_")) == 0;
        for (int index = 0; index < 4; ++index) {
            source->arguments[index] = unicodeArgs[index];
            if (index > 0 && !playerChat && args[index][0] == '#' && vgui2::localize()->Find(args[index])) {
                source->argumentTokens[index] = args[index];
            }
        }
        g_sayTextLine[line].SetSource(source);
        g_lastChatSource = source;
    }
	EnsureTextFitsInOneLineAndWrapIfHaveTo( line );
	if ( !line )
		flScrollTime = m_HUD_saytext_time->value + gHUD.m_flTime;
	m_iFlags |= HUD_ACTIVE;
	Y_START = GetTextPrintY();
}

int CHudSayText::MsgFunc_SayText( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int client = READ_BYTE();
	static char szMsg[1024];
	strncpy( szMsg, READ_STRING(), sizeof( szMsg ) );
	szMsg[sizeof( szMsg ) - 1] = 0;
	int arguments = 0;
	for ( char* scan = szMsg; *scan; scan++ )
	{
		if ( *scan != '%' )
			continue;
		if ( scan[1] == '%' )
			scan++;
		else if ( arguments < 4 && scan[1] == 's' )
		{
			arguments++;
			scan++;
		}
		else
			*scan = ' ';
	}
	char* args[4];
	for ( int i = 0; i < 3; i++ )
	{
		const char* argument = READ_STRING();
		if ( !i && !*argument )
			argument = client > 0 ? g_PlayerInfoList[client].name : "";
		if ( !argument )
			argument = "";
		args[i] = new char[strlen( argument ) + 1];
		strcpy( args[i], argument );
	}
	args[3] = READ_STRING();
	SayTextPrint( szMsg, size - 1, client, args[0], args[1], args[2], args[3] );
	delete[] args[0];
	delete[] args[1];
	delete[] args[2];
	return 1;
}

void CHudSayText::RefreshLanguage() {
    struct Line {
        std::wstring text;
        std::shared_ptr<ChatSource> source;
        int client;
    };
    std::vector<Line> rendered;
    std::shared_ptr<ChatSource> previous;
    for (int index = 0; index < MAX_LINES; ++index) {
        SayTextLine& line = g_sayTextLine[index];
        const auto source = line.GetSource();
        if (source) {
            if (source != previous) {
                const auto lines = WrapChatSource(RenderChatSource(*source));
                for (std::size_t row = source->discardedLines; row < lines.size(); ++row) {
                    rendered.push_back({lines[row], source, source->client});
                }
            }
        } else if (*line.GetText()) {
            rendered.push_back({line.GetText(), nullptr, line.GetClientIndex()});
        }
        previous = source;
    }
    const std::size_t first = rendered.size() > MAX_LINES ? rendered.size() - MAX_LINES : 0;
    for (std::size_t index = 0; index < first; ++index) {
        if (rendered[index].source) {
            ++rendered[index].source->discardedLines;
        }
    }
    for (int index = 0; index <= MAX_LINES; ++index) {
        g_sayTextLine[index].Clear();
    }
    for (std::size_t index = first; index < rendered.size(); ++index) {
        SayTextLine& line = g_sayTextLine[index - first];
        line.SetText(const_cast<wchar_t*>(rendered[index].text.c_str()), rendered[index].client);
        line.SetSource(rendered[index].source);
    }
    // No packet/audio/console replay and no change to flScrollTime or active flags.
    Y_START = GetTextPrintY();
}

void CHudSayText::CaptureTextMessageSources(const char* text, const std::string (&arguments)[4]) {
    const auto source = g_lastChatSource.lock();
    if (!source) {
        return;
    }
    for (int index = 0; index < 5; ++index) {
        const char* raw = index == 0 ? text : arguments[index - 1].c_str();
        const bool playerChat = source->token.compare(0, strlen("#Cstrike_Chat_"), "#Cstrike_Chat_") == 0;
        if (index == 1 || (index > 0 && playerChat)) {
            wchar_t literal[MAX_CHARS_PER_LINE]{};
            vgui2::localize()->ConvertANSIToUnicode(raw ? raw : "", literal, sizeof(literal));
            source->arguments[index - 1] = literal;
            source->argumentTokens[index - 1].clear();
            continue;
        }
        const client_textmessage_t* title = raw && raw[0] == '#' ? TextMessageGet(raw + 1) : nullptr;
        if (title && CS_IsCatalogueMessage(title)) {
            std::string& target = index == 0 ? source->titleKey : source->argumentTitleKeys[index - 1];
            target = raw + 1;
        }
    }
    RefreshLanguage();
}
