#include "SmokedGlass.h"
#include "VGUI_RadioButton.h"
#include "VGUI_Image.h"

namespace
{

using namespace vgui;

class FooRadioImage : public Image
{
public:
	FooRadioImage( RadioButton* radioButton )
	{
		_radioButton = radioButton;
		setSize( 20, 20 );
	}

	virtual void paint( Panel* panel ) override;

protected:
	RadioButton*	_radioButton;
};

void FooRadioImage::paint( Panel* )
{
    if (cse_vgui1::smoked::Enabled()) {
        drawSetTextFont(Scheme::sf_secondary1);
        const auto field = cse::ui::Field;
        drawSetTextColor(field.r, field.g, field.b, cse::ui::LegacyAlpha(field.a));
        drawPrintChar(0, 0, 'n');
        const auto edge = _radioButton->isSelected() ? cse::ui::Accent : cse::ui::Edge;
        drawSetTextColor(edge.r, edge.g, edge.b, cse::ui::LegacyAlpha(edge.a));
        drawPrintChar(0, 0, 'j');
        if (_radioButton->isSelected()) {
            const auto accent = cse::ui::Accent;
            drawSetTextColor(accent.r, accent.g, accent.b, cse::ui::LegacyAlpha(accent.a));
            drawPrintChar(0, 0, 'h');
        }
        return;
    }
	drawSetTextFont( Scheme::sf_secondary1 );
	drawSetTextColor( Scheme::sc_white );
	drawPrintChar( 0, 0, 'n' );
	drawSetTextColor( Scheme::sc_secondary2 );
	drawPrintChar( 0, 0, 'j' );
	drawSetTextColor( Scheme::sc_white );
	drawPrintChar( 0, 0, 'k' );
	drawSetTextColor( Scheme::sc_black );
	drawPrintChar( 0, 0, 'l' );
	drawSetTextColor( Scheme::sc_secondary3 );
	drawPrintChar( 0, 0, 'm' );

	if ( _radioButton->isSelected() )
	{
		drawSetTextColor( Scheme::sc_black );
		drawPrintChar( 0, 0, 'h' );
	}
}

}

namespace vgui
{

RadioButton::RadioButton( const char* text, int x, int y, int wide, int tall )
	: ToggleButton( text, x, y, wide, tall )
{
	setTextAlignment( Label::a_east );
	setImage( new FooRadioImage( this ) );
}

RadioButton::RadioButton( const char* text, int x, int y ) : ToggleButton( text, x, y )
{
	setTextAlignment( Label::a_east );
	setImage( new FooRadioImage( this ) );

	int wide;
	int tall;
	getContentSize( wide, tall );
	setSize( wide, tall );
}

void RadioButton::paintBackground()
{
    if (cse_vgui1::smoked::Enabled()) {
        Panel::paintBackground();
        return;
    }
	int wide;
	int tall;

	getPaintSize( wide, tall );
	drawSetColor( Scheme::sc_secondary3 );
	drawFilledRect( 0, 0, wide, tall );
}

}
