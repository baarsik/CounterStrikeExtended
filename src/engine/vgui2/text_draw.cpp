#include "../vgui/IScheme.h"
#include "../vgui_intwrap2.h"
#include "../color.h"
#include "../controls.h"
#include "BaseUISurface.h"

#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <string>

static vgui2::HFont _consoleFont;
static vgui2::HFont _creditsFont;
static Color _col( 255, 255, 255, 255 );

void VGUI2_Draw_Init()
{
	vgui2::ISchemeManager* defaultManager = vgui2::scheme();
	vgui2::ISchemeManager* manager = vgui2::scheme();
	vgui2::IScheme* scheme = manager->GetIScheme( defaultManager->GetDefaultScheme() );
	if ( !_consoleFont )
		_consoleFont = scheme->GetFont( "EngineFont", true );
	if ( !_creditsFont )
	{
		if ( std::atoi( scheme->GetResourceString( "UseScaledCreditsFont" ) ) )
			_creditsFont = scheme->GetFont( "CreditsFont", true );
		else
			_creditsFont = scheme->GetFont( "Legacy_CreditsFont", true );
	}
}

void VGUI2_Draw_SetTextColor( int red, int green, int blue )
{
	_col.SetColor( red, green, blue, 255 );
}

void VGUI2_Draw_SetTextColorAlpha( int red, int green, int blue, int alpha )
{
	_col.SetColor( red, green, blue, alpha );
}

vgui2::HFont VGUI2_GetConsoleFont()
{
	return _consoleFont;
}

vgui2::HFont VGUI2_GetCreditsFont()
{
	return _creditsFont;
}

const wchar_t* VGUI2_Find_String( const char* text )
{
	constexpr int LOCALIZED_STRING_CHARACTERS = 1024;
	static wchar_t tmpString[LOCALIZED_STRING_CHARACTERS];
	if ( text && text[0] == '#' )
	{
		const wchar_t* localizedString = vgui2::localize()->Find( text );
		if ( localizedString )
			return localizedString;
	}
	vgui2::localize()->ConvertANSIToUnicode( text, tmpString, sizeof( tmpString ) );
	return tmpString;
}

static bool VGUI2_IsDrawableCharacter(int character) {
    // The process can use the C locale; Unicode visibility must not depend on it.
    constexpr int FirstPrintableCharacter = 0x20;
    constexpr int FirstControlCharacter = 0x7f;
    constexpr int LastControlCharacter = 0x9f;
    constexpr int FirstSurrogate = 0xd800;
    constexpr int LastSurrogate = 0xdfff;
    constexpr int NativeCharacterMaximum = sizeof(wchar_t) == 2 ? 0xffff : 0x10ffff;
    return character >= FirstPrintableCharacter && character <= NativeCharacterMaximum
        && (character < FirstControlCharacter || character > LastControlCharacter)
        && (character < FirstSurrogate || character > LastSurrogate);
}

int VGUI2_Draw_Character( int x, int y, int ch, unsigned int font ) {
    g_BaseUISurface.ResetViewPort();
    vgui2::surface()->DrawSetTextFont( font );
    vgui2::surface()->DrawSetTextPos( x, y );
    vgui2::surface()->DrawSetTextColor( _col );
    if (VGUI2_IsDrawableCharacter(ch)) {
        vgui2::surface()->DrawUnicodeChar( static_cast<wchar_t>( ch ) );
        vgui2::surface()->DrawFlushText();
    }
    int a = 0;
    int b = 0;
    int c = 0;
    vgui2::surface()->GetCharABCwide( font, ch, a, b, c );
    return a + b + c;
}

int VGUI2_Draw_CharacterAdd( int x, int y, int ch, int red, int green, int blue, unsigned int font ) {
    g_BaseUISurface.ResetViewPort();
    constexpr int TextOpaqueAlpha = 255;
    _col.SetColor(red, green, blue, TextOpaqueAlpha);
    vgui2::surface()->DrawSetTextFont(font);
    vgui2::surface()->DrawSetTextPos( x, y );
    vgui2::surface()->DrawSetTextColor( _col );
    if (VGUI2_IsDrawableCharacter(ch)) {
        vgui2::surface()->DrawUnicodeCharAdd(static_cast<wchar_t>(ch));
    }
    vgui2::surface()->GetFontTall( font );
    int a = 0;
    int b = 0;
    int c = 0;
    vgui2::surface()->GetCharABCwide( font, ch, a, b, c );
    return a + b + c;
}

int VGUI2_Draw_StringLenW( const wchar_t* text, unsigned int font )
{
    int width = 0;
    for ( std::size_t i = 0; i < std::wcslen( text ); ++i )
    {
        int a = 0;
        int b = 0;
        int c = 0;
        vgui2::surface()->GetCharABCwide( font, text[i], a, b, c );
        width += a + b + c;
    }
    return width;
}

int VGUI2_DrawString( int x, int y, const char* text, unsigned int font ) {
    g_BaseUISurface.ResetViewPort();
    vgui2::surface()->DrawSetTextFont( font );
    vgui2::surface()->DrawSetTextPos( x, y );
    vgui2::surface()->DrawSetTextColor( _col );
    std::wstring localizedString = VGUI2_Find_String(text);
    for (wchar_t& character : localizedString) {
        if (!VGUI2_IsDrawableCharacter(character)) {
            character = L' ';
        }
    }
    vgui2::surface()->DrawPrintText(localizedString.c_str(), static_cast<int>(localizedString.size()));
    vgui2::surface()->DrawFlushText();
    return VGUI2_Draw_StringLenW(localizedString.c_str(), font);
}

int VGUI2_DrawStringClient( int x, int y, const char* text, int red, int green, int blue )
{
	g_BaseUISurface.ResetViewPort();
	const wchar_t* localizedString = VGUI2_Find_String( text );
	int currentX = x;
	for ( std::size_t i = 0; i < std::wcslen( localizedString ); ++i )
		currentX += VGUI2_Draw_CharacterAdd( currentX, y, localizedString[i], red, green, blue, _creditsFont );
	return currentX - x;
}

int VGUI2_DrawStringReverseClient( int x, int y, const char* text, int red, int green, int blue )
{
	g_BaseUISurface.ResetViewPort();
	const wchar_t* localizedString = VGUI2_Find_String( text );
	std::size_t length = std::wcslen( localizedString );
	if ( length > INT_MAX )
		length = INT_MAX;
	if ( !length )
		return x;
	int a, b, c;
	vgui2::surface()->GetCharABCwide( _creditsFont, localizedString[length > 1 ? length - 2 : 0], a, b, c );
	int currentX = x - a - b - c;
	while ( length )
	{
		--length;
		currentX -= VGUI2_Draw_CharacterAdd( currentX, y, localizedString[length], red, green, blue, _creditsFont );
	}
	return x + c + b + a - currentX;
}

int VGUI2_Draw_StringLen( const char* text, unsigned int font )
{
	return VGUI2_Draw_StringLenW( VGUI2_Find_String( text ), font );
}

int VGUI2_GetFontTall( unsigned int font )
{
	return vgui2::surface()->GetFontTall( font );
}

int VGUI2_GetFontWide( wchar_t ch, unsigned int font )
{
	int a, b, c;
	vgui2::surface()->GetCharABCwide( font, ch, a, b, c );
	return a + b + c;
}

int VGUI2_MessageFontInfo( short* pWidth, unsigned int font )
{
	constexpr int MESSAGE_FONT_CHARACTERS = 256;
	vgui2::surface()->DrawSetTextFont( font );
	if ( pWidth )
	{
		for ( int i = 0; i < MESSAGE_FONT_CHARACTERS; ++i )
			pWidth[i] = static_cast<short>( VGUI2_GetFontWide( static_cast<wchar_t>( i ), font ) );
	}
	return vgui2::surface()->GetFontTall( font );
}

void VGUI2_ResetCurrentTexture()
{
	g_BaseUISurface.ResetCurrentTexture();
}
