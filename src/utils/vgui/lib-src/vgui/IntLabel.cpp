#include "SmokedGlass.h"
#include "VGUI_IntLabel.h"

#include <stdio.h>
#include <string.h>

namespace vgui
{

IntLabel::IntLabel( int value, int x, int y, int wide, int tall ) : Label( NULL, x, y, wide, tall )
{
	_value = 0;
	setValue( value );
}

void IntLabel::setValue( int value )
{
	if ( _value != value )
	{
		_value = value;
		repaint();
	}
}

void IntLabel::intChanged( int value, Panel* )
{
	setValue( value );
}

void IntLabel::paintBackground()
{
	char buf[50];

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4996 )
#endif
	sprintf( buf, "%d", _value );
#if defined( _MSC_VER )
#pragma warning( pop )
#endif
	Panel::paintBackground();
	drawSetTextFont( Scheme::sf_primary1 );
    if (cse_vgui1::smoked::Enabled()) {
        const auto color = cse::ui::Text;
        drawSetTextColor(color.r, color.g, color.b, cse::ui::LegacyAlpha(color.a));
    } else {
        drawSetTextColor(Scheme::sc_black);
    }
	drawPrintText( 0, 0, buf, (int)strlen( buf ) );
}

}
