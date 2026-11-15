#include "SmokedGlass.h"
#include "VGUI_CheckButton.h"
#include "VGUI_Image.h"

namespace
{

using namespace vgui;

class FooCheckImage : public Image
{
public:
	FooCheckImage( CheckButton* checkButton )
	{
		_checkButton = checkButton;
		setSize( 20, 20 );
	}

	virtual void paint( Panel* panel ) override;

protected:
	CheckButton*	_checkButton;
};

void FooCheckImage::paint( Panel* )
{
    if (cse_vgui1::smoked::Enabled()) {
        drawSetTextFont(Scheme::sf_secondary1);
        const auto field = cse::ui::Field;
        drawSetTextColor(field.r, field.g, field.b, cse::ui::LegacyAlpha(field.a));
        drawPrintChar(0, 0, 'g');
        const auto edge = cse::ui::Edge;
        drawSetTextColor(edge.r, edge.g, edge.b, cse::ui::LegacyAlpha(edge.a));
        drawPrintChar(0, 0, 'c');
        if (_checkButton->isSelected()) {
            const auto accent = cse::ui::Accent;
            drawSetTextColor(accent.r, accent.g, accent.b, cse::ui::LegacyAlpha(accent.a));
            drawPrintChar(0, 0, 'a');
        }
        return;
    }
	drawSetTextFont( Scheme::sf_secondary1 );
	drawSetTextColor( Scheme::sc_white );
	drawPrintChar( 0, 0, 'g' );
	drawSetTextColor( Scheme::sc_secondary2 );
	drawPrintChar( 0, 0, 'c' );
	drawSetTextColor( Scheme::sc_white );
	drawPrintChar( 0, 0, 'd' );
	drawSetTextColor( Scheme::sc_black );
	drawPrintChar( 0, 0, 'e' );
	drawSetTextColor( Scheme::sc_secondary3 );
	drawPrintChar( 0, 0, 'f' );
	drawSetTextColor( Scheme::sc_black );

	if ( _checkButton->isSelected() )
	{
		drawSetTextColor( Scheme::sc_black );
		drawPrintChar( 0, 0, 'a' );
	}
}

}

namespace vgui
{

CheckButton::CheckButton( const char* text, int x, int y, int wide, int tall )
	: ToggleButton( text, x, y, wide, tall )
{
	setTextAlignment( Label::a_east );
	setImage( new FooCheckImage( this ) );
}

CheckButton::CheckButton( const char* text, int x, int y ) : ToggleButton( text, x, y )
{
	setTextAlignment( Label::a_east );
	setImage( new FooCheckImage( this ) );

	int wide;
	int tall;
	getContentSize( wide, tall );
	setSize( wide, tall );
}

void CheckButton::paintBackground()
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
