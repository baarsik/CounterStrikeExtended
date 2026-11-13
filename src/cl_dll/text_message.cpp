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
// text_message.cpp
//
// implementation of CHudTextMessage class
//
// this class routes messages through titles.txt for localisation
//

#include "hud.h"
#include "localization_client.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "../common/legacy_text_encoding.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "vgui_TeamFortressViewport.h"

DECLARE_MESSAGE( m_TextMessage, TextMsg );

int CHudTextMessage::Init(void)
{
	HOOK_MESSAGE( TextMsg );

	gHUD.AddHudElem( this );

	Reset();

	return 1;
};

// Searches through the string for any msg names (indicated by a '#')
// any found are looked up in titles.txt and the new message substituted
// the new value is pushed into dst_buffer
namespace {

std::size_t LocalizedPrefixBytes(const std::string& text, std::size_t capacity) {
    std::size_t offset = 0;
    std::size_t prefix = 0;
    while (offset < text.size()) {
        const auto* input = reinterpret_cast<const unsigned char*>(text.c_str() + offset);
        const auto decoded = cse::legacy_text::detail::DecodeUtf8(input);
        const auto encoded = cse::legacy_text::detail::EncodeUtf8(decoded.scalar);
        if (decoded.consumed != encoded.count || std::memcmp(input, encoded.bytes, encoded.count) != 0) {
            // Existing non-UTF8 titles retain their original byte semantics.
            return (std::min)(capacity, text.size());
        }
        offset += decoded.consumed;
        if (offset <= capacity) {
            prefix = offset;
        }
    }
    return prefix;
}

} // namespace

char* CHudTextMessage::LocaliseTextString(const char* msg, char* dst_buffer, int buffer_size) {
    if (!dst_buffer || buffer_size <= 0) {
        return dst_buffer;
    }
    if (!msg) {
        dst_buffer[0] = '\0';
        return dst_buffer;
    }

    std::string expanded;
    const char* source = msg;
    while (*source) {
        if (*source != '#') {
            expanded.push_back(*source++);
            continue;
        }
        const char* marker = source++;
        const char* nameStart = source;
        // Preserve the retained title-name alphabet, including underscores.
        while ((*source >= 'A' && *source <= 'z') || (*source >= '0' && *source <= '9')) {
            ++source;
        }
        const std::string name(nameStart, static_cast<std::size_t>(source - nameStart));
        const auto* message = TextMessageGet(name.c_str());
        if (message && message->pMessage) {
            expanded += message->pMessage;
        } else {
            expanded.push_back('#');
            source = marker + 1;
        }
    }

    const auto capacity = static_cast<std::size_t>(buffer_size - 1);
    const std::size_t copied = LocalizedPrefixBytes(expanded, capacity);
    std::memcpy(dst_buffer, expanded.c_str(), copied);
    dst_buffer[copied] = '\0';
    return dst_buffer;
}

// As above, but with a local static buffer
char *CHudTextMessage::BufferedLocaliseTextString( const char *msg )
{
	static char dst_buffer[1024];
	LocaliseTextString( msg, dst_buffer, 1024 );
	return dst_buffer;
}

// Simplified version of LocaliseTextString;  assumes string is only one word
char *CHudTextMessage::LookupString( const char *msg, int *msg_dest )
{
	if ( !msg )
		return "";

	// '#' character indicates this is a reference to a string in titles.txt, and not the string itself
	if ( msg[0] == '#' ) 
	{
		// this is a message name, so look up the real message
		client_textmessage_t *clmsg = TextMessageGet( msg+1 );

		if ( !clmsg || !(clmsg->pMessage) )
			return (char*)msg; // lookup failed, so return the original string
		
		if ( msg_dest )
		{
			// check to see if titles.txt info overrides msg destination
			// if clmsg->effect is less than 0, then clmsg->effect holds -1 * message_destination
			if ( clmsg->effect < 0 )  // 
				*msg_dest = -clmsg->effect;
		}

		return (char*)clmsg->pMessage;
	}
	else
	{  // nothing special about this message, so just return the same string
		return (char*)msg;
	}
}

void StripEndNewlineFromString(char* text) {
    const size_t length = strlen(text);
    if (length == 0) {
        return;
    }
    const size_t last = length - 1;
    if (text[last] == '\n' || text[last] == '\r') {
        text[last] = 0;
    }
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
char* ConvertCRtoNL( char *str )
{
	for ( char *ch = str; *ch != 0; ch++ )
		if ( *ch == '\r' )
			*ch = '\n';
	return str;
}

// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message direction  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in titles.txt
// the next (optional) one to four strings are parameters for that string (which can also be message names if they begin with '#')
int CHudTextMessage::MsgFunc_TextMsg( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int msg_dest = READ_BYTE();

#define MSG_BUF_SIZE 128
	static char szBuf[6][MSG_BUF_SIZE];
	std::string sources[5];
    sources[0] = READ_STRING();
    char *msg_text = LookupString(sources[0].c_str(), &msg_dest);
	msg_text = safe_strcpy( szBuf[0], msg_text , MSG_BUF_SIZE);

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	sources[1] = READ_STRING();
    char *sstr1 = LookupString(sources[1].c_str());
	sstr1 = safe_strcpy( szBuf[1], sstr1 , MSG_BUF_SIZE);
	StripEndNewlineFromString( sstr1 );  // these strings are meant for subsitution into the main strings, so cull the automatic end newlines
	sources[2] = READ_STRING();
    char *sstr2 = LookupString(sources[2].c_str());
	sstr2 = safe_strcpy( szBuf[2], sstr2 , MSG_BUF_SIZE);
	StripEndNewlineFromString( sstr2 );
	sources[3] = READ_STRING();
    char *sstr3 = LookupString(sources[3].c_str());
	sstr3 = safe_strcpy( szBuf[3], sstr3 , MSG_BUF_SIZE);
	StripEndNewlineFromString( sstr3 );
	sources[4] = READ_STRING();
    char *sstr4 = LookupString(sources[4].c_str());
	sstr4 = safe_strcpy( szBuf[4], sstr4 , MSG_BUF_SIZE);
	StripEndNewlineFromString( sstr4 );
	char *psz = szBuf[5];
    const char* formatSources[]{szBuf[0], szBuf[1], szBuf[2], szBuf[3], szBuf[4]};

	if ( gViewPort && gViewPort->AllowedToPrintText() == FALSE )
		return 1;

	switch ( msg_dest )
	{
	case HUD_PRINTCENTER:
		safe_sprintf( psz, MSG_BUF_SIZE, msg_text, sstr1, sstr2, sstr3, sstr4 );
        FinishHudText(psz, formatSources);
        m_hasCenterSource = false;
        for (int index = 0; index < 5; ++index) {
            const bool token = sources[index].size() > 1 && sources[index][0] == '#'
                && IsCatalogueMessage(TextMessageGet(sources[index].c_str() + 1));
            m_centerTokens[index] = token;
            m_centerSources[index] = token ? sources[index].substr(1) : szBuf[index];
            m_hasCenterSource = m_hasCenterSource || token;
        }
        m_centerRendered = ConvertCRtoNL(psz);
        CenterPrint(psz);
		break;

	case HUD_PRINTNOTIFY:
		psz[0] = 1;  // mark this message to go into the notify buffer
		safe_sprintf( psz+1, MSG_BUF_SIZE - 1, msg_text, sstr1, sstr2, sstr3, sstr4 );
        FinishHudText(psz + 1, formatSources);
		ConsolePrint( ConvertCRtoNL( psz ) );
		break;

	case HUD_PRINTTALK:
		safe_sprintf( psz, MSG_BUF_SIZE, msg_text, sstr1, sstr2, sstr3, sstr4 );
        FinishHudText(psz, formatSources);
		        {
            auto source = std::make_shared<HudTextSource>();
            const char* resolved[]{szBuf[0], szBuf[1], szBuf[2], szBuf[3], szBuf[4]};
            if (!source->Capture(sources, resolved)) {
                source.reset();
            }
            gHUD.m_SayText.SayTextPrint(ConvertCRtoNL(psz), MSG_BUF_SIZE, -1, source);
        }
		break;

	case HUD_PRINTCONSOLE:
		safe_sprintf( psz, MSG_BUF_SIZE, msg_text, sstr1, sstr2, sstr3, sstr4 );
        FinishHudText(psz, formatSources);
		ConsolePrint( ConvertCRtoNL( psz ) );
		break;
	}

	return 1;
}

void CHudTextMessage::RefreshLanguage() {
    const auto* engine = GetLocalizationEngine();
    if (!engine || !m_hasCenterSource) {
        return;
    }
    std::string values[5];
    for (int index = 0; index < 5; ++index) {
        values[index] = m_centerSources[index];
        if (m_centerTokens[index]) {
            const auto* message = TextMessageGet(m_centerSources[index].c_str());
            if (!message || !message->pMessage) {
                return; // Keep the still-visible owned presentation on missing fallback.
            }
            values[index] = message->pMessage;
        }
        if (index > 0 && m_centerTokens[index] && !values[index].empty()
            && (values[index].back() == '\n' || values[index].back() == '\r')) {
            values[index].pop_back();
        }
    }
    constexpr int CenterMessageBytes = 128; // Existing TextMsg display capacity.
    char replacement[CenterMessageBytes];
    safe_sprintf(replacement, sizeof(replacement), values[0].c_str(), values[1].c_str(),
        values[2].c_str(), values[3].c_str(), values[4].c_str());
    const char* sources[]{values[0].c_str(), values[1].c_str(), values[2].c_str(), values[3].c_str(),
        values[4].c_str()};
    FinishHudText(replacement, sources);
    ConvertCRtoNL(replacement);
    if (engine->ReplaceCenterText(m_centerRendered.c_str(), replacement)) {
        m_centerRendered = replacement;
    } else {
        m_hasCenterSource = false;
    }
}
