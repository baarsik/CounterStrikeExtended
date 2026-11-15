#include "SmokedGlass.h"
//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "MenuItem.h"
#include "Menu.h"
#include "TextImage.h"
#include "controls.h"

#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

#include <cassert>
#include <cstdlib>

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\MenuItem.cpp" )
#else
#define ASSERT_AT( expression, line )	assert( expression )
#endif

//-----------------------------------------------------------------------------
// Purpose: Check box image
//-----------------------------------------------------------------------------
class MenuItemCheckImage : public vgui2::TextImage
{
public:
	MenuItemCheckImage( vgui2::MenuItem* menuItem ) : vgui2::TextImage( "g" ), _menuItem( menuItem )
	{
		SetSize( 20, 13 );
	}

	~MenuItemCheckImage() override;

	void Paint() override;

private:
	vgui2::MenuItem* _menuItem;
};

MenuItemCheckImage::~MenuItemCheckImage()
{
}

void MenuItemCheckImage::Paint()
{
	DrawSetTextFont( GetFont() );

	DrawSetTextColor( _menuItem->GetFgColor() );
	DrawPrintChar( 0, 0, L'g' );

	// draw check
	if ( !_menuItem->IsChecked() )
		return;

	if ( _menuItem->IsEnabled() )
	{
		DrawSetTextColor( _menuItem->GetButtonFgColor() );
		DrawPrintChar( 0, 2, L'a' );
	}
	else if ( !_menuItem->IsEnabled() )
	{
		// draw disabled version, with embossed look
		DrawSetTextColor( _menuItem->GetDisabledFgColor1() );
		DrawPrintChar( 1, 3, L'a' );

		DrawSetTextColor( _menuItem->GetDisabledFgColor2() );
		DrawPrintChar( 0, 2, L'a' );
	}
}

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input:	parent - the parent of this menu item, usually a menu
//			text - the name of the menu item as it appears in the menu
//			cascadeMenu - if this item triggers the opening of a cascading menu
//			provide a pointer to it.
//			MenuItems cannot be both checkable and trigger a cascade menu.
//-----------------------------------------------------------------------------
MenuItem::MenuItem( Menu* parent, const char* name, const char* text, Menu* cascadeMenu, bool checkable )
	: Button( parent, name, text )
{
	m_pCascadeMenu = cascadeMenu;
	m_bCheckable = checkable;
	SetButtonActivationType( ACTIVATE_ONRELEASED );
	m_pUserData = NULL;
#ifdef _WIN32
	ASSERT_AT( !(cascadeMenu && checkable), 87 );
#endif
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input:	parent - the parent of this menu item, usually a menu
//			text - the name of the menu item as it appears in the menu
//			cascadeMenu - if this item triggers the opening of a cascading menu
//			provide a pointer to it.
//			MenuItems cannot be both checkable and trigger a cascade menu.
//-----------------------------------------------------------------------------
MenuItem::MenuItem( Menu* parent, const char* name, const wchar_t* text, Menu* cascadeMenu, bool checkable )
	: Button( parent, name, text )
{
	m_pCascadeMenu = cascadeMenu;
	m_bCheckable = checkable;
	SetButtonActivationType( ACTIVATE_ONRELEASED );
	m_pUserData = NULL;
#ifdef _WIN32
	ASSERT_AT( !(cascadeMenu && checkable), 108 );
#endif
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Return a the menu that this menuItem contains
//          This is useful when the parent menu's commands must be
//          sent through all menus that are open as well (like hotkeys)
//-----------------------------------------------------------------------------
Menu* MenuItem::GetMenu()
{
	return m_pCascadeMenu;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Menu* MenuItem::GetParentMenu()
{
	return (Menu*)GetParent();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
KeyValues* MenuItem::GetUserData()
{
	if ( m_pCascadeMenu )
		return m_pCascadeMenu->GetItemUserData( m_pCascadeMenu->GetActiveItem() );

	return m_pUserData;
}

//-----------------------------------------------------------------------------
// Purpse: Return true if this item triggers a cascading menu
//-----------------------------------------------------------------------------
bool MenuItem::HasMenu()
{
	return m_pCascadeMenu != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Return if this menuitem is checkable or not
// This is used by menus to perform the layout properly.
//-----------------------------------------------------------------------------
bool MenuItem::IsCheckable()
{
	return m_bCheckable;
}

//-----------------------------------------------------------------------------
// Purpose: Close the cascading menu if we have one.
//-----------------------------------------------------------------------------
void MenuItem::CloseCascadeMenu()
{
	if ( !m_pCascadeMenu )
		return;

	if ( m_pCascadeMenu->IsVisible() )
		m_pCascadeMenu->SetVisible( false );

	SetArmed( false );
}

//-----------------------------------------------------------------------------
// Purpose: Return if this menuitem is checked or not
//-----------------------------------------------------------------------------
bool MenuItem::IsChecked()
{
	return m_bChecked;
}

//-----------------------------------------------------------------------------
// Purpose: Basic initializer
//-----------------------------------------------------------------------------
void MenuItem::Init()
{
	m_pCascadeArrow = NULL;
	m_pCheck = NULL;

	if ( m_pCascadeMenu )
	{
		m_pCascadeMenu->SetParent( this );
		m_pCascadeArrow = new TextImage( "4" ); // this makes a right pointing arrow.
		m_pCascadeMenu->AddActionSignalTarget( this );
	}
	else if ( m_bCheckable )
	{
		// move the text image over so we have room for the check
		SetTextImageIndex( 1 );
		m_pCheck = new ::MenuItemCheckImage( this );
		SetImageAtIndex( 0, m_pCheck, CHECK_INSET );
		SetChecked( false );
	}

	SetButtonBorderEnabled( false );
	SetContentAlignment( a_west );
	SetUseCaptureMouse( false );
	SetContentAlignment( a_west );
}

//-----------------------------------------------------------------------------
// Purpose: Return the size of the check portion of the label.
//-----------------------------------------------------------------------------
void MenuItem::GetCheckImageSize( int& width, int& height )
{
	width = 0;
	height = 0;

	if ( m_pCheck )
	{
		( (TextImage*)m_pCheck )->ResizeImageToContent();
		m_pCheck->GetSize( width, height );
		width += CHECK_INSET;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the menu to key mode if a child menu goes into keymode
//-----------------------------------------------------------------------------
void MenuItem::OnKeyModeSet()
{
	// send the message to this parent in case this is a cascading menu
	ivgui()->PostMessage( GetVParent(), new KeyValues( "KeyModeSet" ), GetVPanel(), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Handle mouse cursor entering a menuItem.
//-----------------------------------------------------------------------------
void MenuItem::OnCursorEntered()
{
    if (cse::ui::ControlsSmokedGlass() && smoked::MenuSeparator(this)) {
        SetArmed(false);
        return;
    }

	// post a message to the parent menu.
	// forward the message on to the parent of this menu.
	KeyValues* msg = new KeyValues( "CursorEnteredMenuItem" );
	msg->SetPtr( "VPanel", reinterpret_cast<void *>(GetVPanel()) );
	ivgui()->PostMessage( GetVParent(), msg, 0, 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Handle mouse cursor exiting a menuItem. 
//-----------------------------------------------------------------------------
void MenuItem::OnCursorExited()
{
	// post a message to the parent menu.
	// forward the message on to the parent of this menu.
	KeyValues* msg = new KeyValues( "CursorExitedMenuItem" );
	msg->SetPtr( "VPanel", reinterpret_cast<void *>(GetVPanel()) );
	ivgui()->PostMessage( GetVParent(), msg, 0, 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Handle cursor moving in a menuItem.
//-----------------------------------------------------------------------------
void MenuItem::OnCursorMoved( int x, int y )
{
	if ( GetParentMenu()->GetMenuMode() == Menu::KEYBOARD )
		OnCursorEntered();

	// chain up to parent
	CallParentFunction( new KeyValues( "OnCursorMoved", "x", x, "y", y ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool MenuItem::CanBeDefaultButton()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: fire the menu item as if it has been selected and 
//			Tell the owner that it is closing
//-----------------------------------------------------------------------------
void MenuItem::FireActionSignal()
{
    if (cse::ui::ControlsSmokedGlass() && smoked::MenuSeparator(this)) {
        SetArmed(false);
        return;
    }

	if ( m_pCascadeMenu )
	{
		if ( GetParentMenu()->GetMenuMode() == Menu::KEYBOARD )
			OpenCascadeMenu();
		return;
	}

	KeyValues* msg = new KeyValues( "MenuItemSelected" );
	msg->SetPtr( "panel", this );
	ivgui()->PostMessage( GetVParent(), msg, GetVPanel(), 0.0f );

	//	ivgui()->PostMessage(GetVParent(), new KeyValues("MenuItemSelected"), GetVPanel());		
	Button::FireActionSignal();

	// toggle the check next to the item if it is checkable
	if ( m_bCheckable )
		m_bChecked = !m_bChecked;
}

//-----------------------------------------------------------------------------
// Purpose: Pass kill focus events up to parent, This will tell all panels
//          in the hierarchy to hide themselves, and enables cascading menus to
//		    all disappear on selecting an item at the end of the tree.
//-----------------------------------------------------------------------------
void MenuItem::OnKillFocus()
{
	GetParentMenu()->OnKillFocus();
}

//-----------------------------------------------------------------------------
// Purpose: Layout the Textimage and the Arrow part of the menuItem
//-----------------------------------------------------------------------------
void MenuItem::Paint() {
    if (!cse::ui::ControlsSmokedGlass()) {
        Button::Paint();
        return;
    }
    const int inset = smoked::Pixels(16);
    if (smoked::MenuSeparator(this)) {
        smoked::Fill(inset, GetTall() / 2, (std::max)(0, GetWide() - inset - inset), 1, 0,
            smoked::ColorOf(cse::ui::Edge));
        return;
    }
    const Color foreground = IsEnabled() ? GetButtonFgColor() : GetDisabledFgColor2();
    if (IsArmed() && IsEnabled()) {
        smoked::Fill(0, smoked::Pixels(4), smoked::Pixels(3), GetTall() - smoked::Pixels(8),
            0, smoked::ColorOf(cse::ui::Accent));
    }
    int textX = smoked::MenuTextInset(GetParentMenu());
    TextImage* text = GetTextImage();
    for (int index = 0; index < GetImageCount(); ++index) {
        IImage* image = GetImageAtIndex(index);
        if (!image || image == text || image == m_pCascadeArrow || image == m_pCheck) {
            continue;
        }
        int wide = 0;
        int tall = 0;
        image->GetSize(wide, tall);
        image->Paint();
        textX += wide + smoked::Pixels(4);
    }
    int oldX = 0;
    int oldY = 0;
    int oldWide = 0;
    int oldTall = 0;
    text->GetPos(oldX, oldY);
    text->GetSize(oldWide, oldTall);
    const Color oldColor = text->GetColor();
    const int tall = surface()->GetFontTall(GetFont());
    text->SetPos(textX, (GetTall() - tall) / 2);
    text->SetSize((std::max)(0, GetWide() - textX - smoked::Pixels(m_pCascadeMenu ? 30 : 16)), tall);
    text->SetColor(foreground);
    text->Paint();
    text->SetColor(oldColor);
    text->SetPos(oldX, oldY);
    text->SetSize(oldWide, oldTall);
    if (m_pCascadeMenu) {
        const int gutter = smoked::Pixels(24);
        smoked::Chevron(gutter, GetTall(), 1, foreground, false, GetWide() - gutter);
    }
    if (m_bCheckable && m_bChecked) {
        const int x = smoked::Pixels(12);
        const int y = GetTall() / 2;
        surface()->DrawSetColor(IsEnabled() ? smoked::ColorOf(cse::ui::Accent) : foreground);
        surface()->DrawLine(x, y, x + smoked::Pixels(3), y + smoked::Pixels(3));
        surface()->DrawLine(x + smoked::Pixels(3), y + smoked::Pixels(3), x + smoked::Pixels(9),
            y - smoked::Pixels(4));
    }
}

void MenuItem::PaintBackground() {
    if (!cse::ui::ControlsSmokedGlass()) {
        Button::PaintBackground();
        return;
    }
    if (!smoked::MenuSeparator(this) && IsEnabled() && IsArmed()) {
        smoked::Fill(0, 0, GetWide(), GetTall(), smoked::Pixels(4), smoked::ColorOf(cse::ui::Selection));
    }
}

void MenuItem::PaintBorder() {
    if (!cse::ui::ControlsSmokedGlass()) {
        Button::PaintBorder();
    }
}

void MenuItem::PerformLayout()
{
	Button::PerformLayout();

	if ( m_pCascadeArrow )
		m_pCascadeArrow->SetColor( GetButtonFgColor() );
}

IBorder* MenuItem::GetBorder( bool, bool, bool, bool )
{
	return NULL;
}

void MenuItem::OnArmed()
{
}

void MenuItem::OnDisarmed()
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
MenuItem::~MenuItem()
{
	delete m_pCascadeMenu;
	delete m_pCascadeArrow;
	delete m_pCheck;

	if ( m_pUserData )
		m_pUserData->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Apply the resource scheme to the menu.
//-----------------------------------------------------------------------------
void MenuItem::ApplySchemeSettings( IScheme* pScheme )
{
	Button::ApplySchemeSettings( pScheme );
    smoked::MenuAvailability(this);

	Color bgColor = GetSchemeColor( "Menu/BgColor", GetBgColor(), pScheme );
	Color fgColor = GetSchemeColor( "Menu/FgColor", GetFgColor(), pScheme );
	SetDefaultColor( fgColor, bgColor );

	Color armedBgColor = GetSchemeColor( "Menu/ArmedBgColor", GetBgColor(), pScheme );
	Color armedFgColor = GetSchemeColor( "Menu/ArmedFgColor", GetFgColor(), pScheme );
	SetArmedColor( armedFgColor, armedBgColor );

	Color depressedBgColor = GetSchemeColor( "Menu/ArmedBgColor", GetBgColor(), pScheme );
	Color depressedFgColor = GetSchemeColor( "Menu/ArmedFgColor", GetFgColor(), pScheme );
	SetDepressedColor( depressedFgColor, depressedBgColor );

	SetTextInset( atoi( pScheme->GetResourceString( "Menu/TextInset" ) ), 0 );
    if (cse::ui::ControlsSmokedGlass()) {
        constexpr int TextInset = 8;
        SetTextInset(GetProportionalScaledValue(TextInset), 0);
    }

	// reload images since applyschemesettings in label wipes them out.
	if ( m_pCascadeArrow )
	{
		m_pCascadeArrow->SetFont( pScheme->GetFontEx( "Marlett", IsProportional(), IsHDProportional() ) );
		m_pCascadeArrow->ResizeImageToContent();
		AddImage( m_pCascadeArrow, 0 );
	}
	else if ( m_bCheckable )
	{
		( (TextImage*)m_pCheck )->SetFont( pScheme->GetFontEx( "Marlett", IsProportional(), IsHDProportional() ) );
		SetImageAtIndex( 0, m_pCheck, CHECK_INSET );
		( (TextImage*)m_pCheck )->ResizeImageToContent();
	}

	// Have the menu redo the layout
	// Get the parent to resize
	Menu* parent = GetParentMenu();
	if ( parent )
		parent->ForceCalculateWidth();
}

//-----------------------------------------------------------------------------
// Purpose: Highlight a menu item
//			Menu item buttons highlight if disabled, but won't activate.
//-----------------------------------------------------------------------------
void MenuItem::ArmItem()
{
    if (cse::ui::ControlsSmokedGlass() && smoked::MenuSeparator(this)) {
        SetArmed(false);
        return;
    }

	OnArmed();

	// close all other menus 
	GetParentMenu()->CloseOtherMenus( this );
	SetArmed( true );

	// When you have a submenu with no scroll bar the menu
	// border will not be drawn correctly. This fixes it.
	Menu* parent = GetParentMenu();
	if ( parent )
		parent->ForceCalculateWidth();

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Unhighlight a menu item
//-----------------------------------------------------------------------------
void MenuItem::DisarmItem()
{
	OnDisarmed();

	if ( !m_pCascadeMenu )
		Button::OnCursorExited();

	// When you have a submenu with no scroll bar the menu
	// border will not be drawn correctly. This fixes it.
	Menu* parent = GetParentMenu();
	if ( parent )
		parent->ForceCalculateWidth();

	Repaint();
}

void MenuItem::OnKeyCodeReleased( KeyCode code )
{
	Menu* parent = GetParentMenu();

	if ( code == KEY_ESCAPE || code == KEY_F4 )
	{
		parent->OnKeyCodeTyped( code );
	}
	else if ( parent->GetMenuMode() != Menu::KEYBOARD || !m_pCascadeMenu )
	{
		Button::OnKeyCodeReleased( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Opens the cascading menu.
//-----------------------------------------------------------------------------
void MenuItem::OpenCascadeMenu()
{
    if (cse::ui::ControlsSmokedGlass() && m_pCascadeMenu) {
        m_pCascadeMenu->SetVisible(true);
        m_pCascadeMenu->MakeReadyForUse();
        m_pCascadeMenu->PerformLayout();
        ArmItem();
        return;
    }

	if ( !m_pCascadeMenu )
		return;

	// perform layout on menu, this way it will open in the right spot 
	// if the window's been moved
	m_pCascadeMenu->PerformLayout();
	m_pCascadeMenu->SetVisible( true );
	ArmItem();
}

//-----------------------------------------------------------------------------
// Purpose: sets the user data
//-----------------------------------------------------------------------------
void MenuItem::SetUserData( const KeyValues* data )
{
	if ( m_pUserData )
		m_pUserData->deleteThis();

	if ( data )
		m_pUserData = data->MakeCopy();
}

bool MenuItem::IsItemArmed()
{
	return Button::IsArmed();
}

//-----------------------------------------------------------------------------
// Purpose: Return the size of the text portion of the label.
//			for normal menu items this is the same as the label size, but for
//			cascading menus it gives you the size of the text portion only, without
//			the arrow.
//-----------------------------------------------------------------------------
void MenuItem::GetTextImageSize( int& width, int& height )
{
	GetTextImage()->GetSize( width, height );
}

//-----------------------------------------------------------------------------
// Purpose: Set the size of the text portion of the label.
//			For normal menu items this is the same as the label size, but for
//			cascading menus it sizes textImage portion only, without
//			the arrow.
//-----------------------------------------------------------------------------
void MenuItem::SetTextImageSize( int width, int height )
{
	GetTextImage()->SetSize( width, height );
}

//-----------------------------------------------------------------------------
// Purpose: Return the size of the arrow portion of the label.
//			If the menuItem is not a cascading menu, 0 is returned.
//-----------------------------------------------------------------------------
void MenuItem::GetArrowImageSize( int& width, int& height )
{
	width = 0;
	height = 0;

	if ( m_pCascadeArrow )
		m_pCascadeArrow->GetSize( width, height );
}

//-----------------------------------------------------------------------------
// Purpose: Set the checked state of a checkable menuitem
//			Does nothing if item is not checkable
//-----------------------------------------------------------------------------
void MenuItem::SetChecked( bool state )
{
	if ( m_bCheckable )
		m_bChecked = state;
}

}
