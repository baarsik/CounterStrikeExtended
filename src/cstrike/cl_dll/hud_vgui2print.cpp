#include "hud.h"
#include "cl_util.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "../../engine/controls.h"
#include "../../vgui2/controls/controls.h"
#include <wchar.h>
#include <wctype.h>

int CHudVGUI2Print::Init()
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;
	m_hudfont = 0;
	return 1;
}

int CHudVGUI2Print::VidInit()
{
	m_flVGUI2StringTime = 0;
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( vgui2::scheme()->GetDefaultScheme() );
	m_hudfont = scheme->GetFont( "EngineFont", true );
	return 1;
}

void CHudVGUI2Print::RefreshLanguage() {
    vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme(vgui2::scheme()->GetDefaultScheme());
    m_hudfont = scheme->GetFont("EngineFont", true);
    for (int index = 0; index < 5; ++index) {
        if (m_centerSnapshots[index].Relocalize(TextMessageGet)) {
            const char* text = m_centerSnapshots[index].Get()->pMessage;
            std::string& target = index == 0 ? m_centerSource : m_centerArgs[index - 1];
            target = text ? text : "";
            while (!target.empty() && (target.back() == '\n' || target.back() == '\r')) {
                target.pop_back();
            }
        }
    }
    if (!m_centerSource.empty()) {
        FormatCenterMessage();
    }
}

void CHudVGUI2Print::FormatCenterMessage() {
    wchar_t* localized = vgui2::localize()->Find(m_centerSource.c_str());
    if (!m_centerHasArgs) {
        if (localized) {
            wcsncpy(m_wCharBuf, localized, ARRAYSIZE(m_wCharBuf) - 1);
            m_wCharBuf[ARRAYSIZE(m_wCharBuf) - 1] = 0;
        }
        return;
    }
    wchar_t arguments[4][64]{};
    for (int index = 0; index < 4; ++index) {
        const char* source = m_centerArgs[index].c_str();
        if (m_centerSnapshots[index + 1].HasCatalogueSource()) {
            VGUI2LocalizeArg(const_cast<char*>(source), arguments[index], sizeof(arguments[index]));
        } else {
            vgui2::localize()->ConvertANSIToUnicode(source, arguments[index], sizeof(arguments[index]));
        }
    }
    if (localized) {
        vgui2::localize()->ConstructString(m_wCharBuf, sizeof(m_wCharBuf), localized, 4,
            arguments[0], arguments[1], arguments[2], arguments[3]);
    } else {
        wchar_t format[2048]{};
        vgui2::localize()->ConvertANSIToUnicode(m_centerSource.c_str(), format, sizeof(format));
        std::wstring expanded;
        int sequential = 0;
        for (std::size_t index = 0; format[index] && expanded.size() < ARRAYSIZE(m_wCharBuf) - 1; ++index) {
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
        wcsncpy(m_wCharBuf, expanded.c_str(), ARRAYSIZE(m_wCharBuf) - 1);
        m_wCharBuf[ARRAYSIZE(m_wCharBuf) - 1] = 0;
    }
}

int CHudVGUI2Print::Draw( float )
{
	if ( m_flVGUI2StringTime > gEngfuncs.GetClientTime() && m_hudfont )
		DrawVGUI2String( m_wCharBuf, m_iX, m_iY, m_fR, m_fG, m_fB );
	return 1;
}

int CHudVGUI2Print::DrawVGUI2String( char* text, int x, int y, float r, float g, float b )
{
	wchar_t* localized = vgui2::localize()->Find( text );
	wchar_t buffer[512];
	if ( !localized )
	{
		vgui2::localize()->ConvertANSIToUnicode( text, buffer, sizeof( buffer ) );
		localized = buffer;
	}
	return DrawVGUI2String( localized, x, y, r, g, b );
}

int CHudVGUI2Print::DrawVGUI2String( wchar_t* text, int x, int y, float r, float g, float b )
{
	if ( !m_hudfont )
		return 0;
	int lines = 1;
	for ( wchar_t* scan = text; *scan; scan++ )
	{
		if ( *scan == '\r' )
			lines++;
	}
	if ( y == -1 )
		y = ( ScreenHeight - vgui2::surface()->GetFontTall( m_hudfont ) ) / 2;
	int position = x;
	for ( int i = 0; i < lines; i++ )
	{
		wchar_t line[1024];
		int length = 0;
		while ( *text && *text != '\r' )
		{
			if ( *text != '\n' )
				line[length++] = *text;
			text++;
		}
		text++;
		line[length] = 0;
		int width, height;
		GetStringSize( line, &width, &height );
		position = x;
		if ( x == -1 )
		{
			float centered = ScreenWidth * 0.5 - width * 0.5;
			position = (int)centered;
		}
		int start = position;
		gEngfuncs.pfnDrawSetTextColor( 0, 0, 0 );
		int a, charWidth, c;
		for ( int j = 0; j < length; j++ )
		{
			gEngfuncs.pfnVGUI2DrawCharacter( (int)( position + 0.1 ), (int)( y + 0.1 ), line[j], m_hudfont );
			vgui2::surface()->GetCharABCwide( m_hudfont, line[j], a, charWidth, c );
			position += a + charWidth + c;
		}
		gEngfuncs.pfnDrawSetTextColor( r, g, b );
		position = start;
		for ( int j = 0; j < length; j++ )
		{
			gEngfuncs.pfnVGUI2DrawCharacter( position, y, line[j], m_hudfont );
			vgui2::surface()->GetCharABCwide( m_hudfont, line[j], a, charWidth, c );
			position += a + charWidth + c;
		}
		y += height;
	}
	return position;
}

int CHudVGUI2Print::DrawVGUI2StringReverse( char* text, int x, int y, float r, float g, float b )
{
	wchar_t* localized = vgui2::localize()->Find( text );
	wchar_t buffer[512];
	if ( !localized )
	{
		vgui2::localize()->ConvertANSIToUnicode( text, buffer, sizeof( buffer ) );
		localized = buffer;
	}
	return DrawVGUI2StringReverse( localized, x, y, r, g, b );
}

int CHudVGUI2Print::DrawVGUI2StringReverse( wchar_t* text, int x, int y, float r, float g, float b )
{
	if ( !m_hudfont )
		return 0;
	if ( y == -1 )
		y = ( ScreenHeight - vgui2::surface()->GetFontTall( m_hudfont ) ) / 2;
	gEngfuncs.pfnDrawSetTextColor( r, g, b );
	for (size_t i = wcslen(text); i > 0;) {
        --i;
        int a;
        int width;
        int c;
		vgui2::surface()->GetCharABCwide( m_hudfont, text[i], a, width, c );
		x -= a + width + c;
		gEngfuncs.pfnVGUI2DrawCharacter( x, y, text[i], m_hudfont );
	}
	return x;
}

void CHudVGUI2Print::VGUI2HudPrint( char* text, int x, int y, float r, float g, float b )
{
    for (auto& snapshot : m_centerSnapshots) {
        snapshot.Clear();
    }
    m_centerSource = text ? text : "";
    m_centerHasArgs = false;
	wchar_t* localized = vgui2::localize()->Find( text );
	if ( localized )
	{
		wcsncpy( m_wCharBuf, localized, ARRAYSIZE( m_wCharBuf ) - 1 );
		m_wCharBuf[ARRAYSIZE( m_wCharBuf ) - 1] = 0;
		m_iX = x;
		m_iY = y;
		m_fR = r;
		m_fG = g;
		m_fB = b;
		m_flVGUI2StringTime = gEngfuncs.GetClientTime() + 4;
	}
}

void CHudVGUI2Print::VGUI2LocalizeArg( char* text, wchar_t* buffer, int size )
{
	if ( !text || !buffer )
		return;
	if ( *text == '#' )
	{
		wchar_t* localized = vgui2::localize()->Find( text );
		if ( localized )
		{
			int length = size / sizeof( wchar_t ) - 1;
			wcsncpy( buffer, localized, length );
			buffer[length] = 0;
			return;
		}
	}
	vgui2::localize()->ConvertANSIToUnicode( text, buffer, size );
}

void CHudVGUI2Print::VGUI2HudPrintArgs( char* text, char* arg1, char* arg2, char* arg3, char* arg4,
	int x, int y, float r, float g, float b )
{
    for (auto& snapshot : m_centerSnapshots) {
        snapshot.Clear();
    }
    m_centerSource = text ? text : "";
    m_centerHasArgs = true;
    const char* sources[4] = {arg1, arg2, arg3, arg4};
    for (int index = 0; index < 4; ++index) {
        m_centerArgs[index] = sources[index] ? sources[index] : "";
    }
	wchar_t* localized = vgui2::localize()->Find( text );
	wchar_t first[64], second[64], third[64], fourth[64];
	VGUI2LocalizeArg( arg1, first, sizeof( first ) );
	VGUI2LocalizeArg( arg2, second, sizeof( second ) );
	VGUI2LocalizeArg( arg3, third, sizeof( third ) );
	VGUI2LocalizeArg( arg4, fourth, sizeof( fourth ) );
	m_iX = x;
	m_iY = y;
	m_fR = r;
	m_fG = g;
	m_fB = b;
	float centerTime = gEngfuncs.pfnGetCvarFloat( "scr_centertime" );
	if ( centerTime < 0 )
		centerTime = 2;
	m_flVGUI2StringTime = gEngfuncs.GetClientTime() + centerTime;
	if ( localized )
	{
		vgui2::localize()->ConstructString( m_wCharBuf, sizeof( m_wCharBuf ), localized, 4, first, second, third, fourth );
		return;
	}
#ifdef _WIN32
	wchar_t format[1024];
#else
	wchar_t format[2048];
#endif
	vgui2::localize()->ConvertANSIToUnicode( text, format, sizeof( format ) );
	_snwprintf( m_wCharBuf, ARRAYSIZE( m_wCharBuf ), format, first, second, third, fourth );
}

int CHudVGUI2Print::GetHudFontHeight()
{
	return m_hudfont ? vgui2::surface()->GetFontTall( m_hudfont ) : 0;
}

void CHudVGUI2Print::GetStringSize(const wchar_t* text, int* width, int* height) {
    if (width) {
        *width = 0;
    }
    if (height) {
        *height = 0;
    }
    if (!m_hudfont) {
        return;
    }
    const size_t length = wcslen(text);
    if (width) {
        for (size_t i = 0; i < length; i++) {
            int a;
            int b;
            int c;
            vgui2::surface()->GetCharABCwide(m_hudfont, text[i], a, b, c);
            *width += a + b + c;
        }
    }
    if (height) {
        *height = vgui2::surface()->GetFontTall(m_hudfont);
    }
}

void CHudVGUI2Print::CaptureTextMessageSources(const char* text, const std::string (&arguments)[4]) {
    const client_textmessage_t* format = text && text[0] == '#' ? TextMessageGet(text + 1) : nullptr;
    // This buy-error template takes an item title; other first arguments can be literal player names.
    const bool firstArgumentIsTitle = CS_IsCatalogueMessage(format) && format->pName &&
        !stricmp(format->pName, "Alias_Not_Avail");
    for (int index = 0; index < 5; ++index) {
        const char* source = index == 0 ? text : arguments[index - 1].c_str();
        client_textmessage_t* message = (index != 1 || firstArgumentIsTitle) && source && source[0] == '#'
            ? TextMessageGet(source + 1) : nullptr;
        m_centerSnapshots[index].Assign(message, CS_IsCatalogueMessage(message));
        if (index == 1 && !m_centerSnapshots[index].HasCatalogueSource()) {
            m_centerArgs[0] = arguments[0];
        }
    }
    FormatCenterMessage();
}
