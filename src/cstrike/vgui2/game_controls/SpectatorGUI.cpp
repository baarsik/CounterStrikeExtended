#include "../../../vgui2/controls/SmokedGlass.h"
//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "SpectatorGUI.h"
#include "../../../vgui2/controls/BackdropPanel.h"
#include "../../../public/vgui_style.h"
#include "iviewport.h"
#include "CreateCommandMenu.h"
#include "../../../engine/igameuifuncs.h"
#include "vgui_controls/ComboBox.h"
#include "Button.h"
#include "Label.h"
#include "ImagePanel.h"
#include "TextImage.h"
#include "Menu.h"
#include "MenuItem.h"
#include "vgui_controls/Controls.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/KeyValues.h>

//#include "pm_shared.h" // OBS_ defines
#include <stdio.h> // _snprintf define
#include <string.h>
#include <climits>
#include <map>
#include <iterator>
#include <string>
#include <wchar.h>
#if defined(_WIN32)
#include <malloc.h>
#endif

using namespace vgui2;

extern IGameUIFuncs* gameuifuncs; // for key binding details
extern float* GetClientColor( int clientIndex );
extern const char* GetSpectatorLabel( int mode );
extern void DuckMessage( const char* text );

// Owned spectator presentation inputs are captured only by normal gameplay updates.
// Language replay must remain safe after disconnect, without querying client entities.
namespace {
struct SpectatorPlayerText {
    std::wstring name;
    std::wstring detail;
    std::string detailToken;
    std::wstring fallback;
    bool withDetail = false;
    bool formatted = false;
    int teamNumber = 0;
};

struct SpectatorPresentation {
    bool hasUpdate = false;
    bool captureTarget = false;
    bool extraOverridden = false;
    bool spectateOnly = false;
    int spectatorCount = 0;
    std::string mapName;
    std::string modeToken;
    SpectatorPlayerText target;
    std::map<int, SpectatorPlayerText> rows;
};

std::map<const CSpectatorGUI*, SpectatorPresentation>& SpectatorPresentations() {
    static std::map<const CSpectatorGUI*, SpectatorPresentation> presentations;
    return presentations;
}

std::wstring FormatSpectatorPlayer(const SpectatorPlayerText& source) {
    if (!source.formatted) {
        return source.fallback;
    }
    wchar_t* format = localize()->Find(source.withDetail ? "#Spec_PlayerItem_Team" : "#Spec_PlayerItem");
    if (!format) {
        return source.fallback;
    }
    constexpr int PlayerTextCells = 256;
    wchar_t text[PlayerTextCells]{};
    const wchar_t* detail = source.detailToken.empty() ? nullptr : localize()->Find(source.detailToken.c_str());
    if (!detail) {
        detail = source.detail.c_str();
    }
    if (source.withDetail) {
        localize()->ConstructString(text, sizeof(text), format, 2, source.name.c_str(), detail);
    } else {
        localize()->ConstructString(text, sizeof(text), format, 1, source.name.c_str());
    }
    return text;
}

std::wstring FormatSpectatorExtra(const SpectatorPresentation& source) {
    constexpr int ExtraTextCells = 1024;
    constexpr int MapNameCells = 64;
    wchar_t text[ExtraTextCells]{};
    if (source.spectateOnly) {
        const std::wstring count = std::to_wstring(source.spectatorCount);
        wchar_t* format = localize()->Find("#Spectators");
        if (format) {
            localize()->ConstructString(text, sizeof(text), format, 1, count.c_str());
        }
        return std::wstring(text) + L" " + count;
    }
    wchar_t mapName[MapNameCells]{};
    localize()->ConvertANSIToUnicode(source.mapName.c_str(), mapName, sizeof(mapName));
    wchar_t* format = localize()->Find("#Spec_Map");
    if (format) {
        localize()->ConstructString(text, sizeof(text), format, 1, mapName);
    }
    return text;
}
} // namespace

class CSpectatorBar : public BackdropPanel {
public:
    CSpectatorBar(Panel* parent, const char* name, bool top)
        : BackdropPanel(parent, name, Style(top)) {}

protected:
    void PaintBackground() override {
        if (gViewPortInterface->GetClientDllInterface()->CvarGetFloat(cse::ui::VGuiStyleCvar) <= 0.0f) {
            Panel::PaintBackground();
            return;
        }
        BackdropPanel::PaintBackground();
    }

private:
    static cse::ui::BackdropStyle Style(bool top) {
        constexpr float EdgeOpacity = 1.0f;
        constexpr float EdgeDarkness = 0.60f;
        return {top ? EdgeOpacity : 0.0f, top ? 0.0f : EdgeOpacity, EdgeDarkness};
    }
};

class CLeftButton : public Button
{
public:
	CLeftButton( Panel* parent, const char* name, const char* text ) : Button( parent, name, text ) {}
    void Paint() override {
        if (cse::ui::ControlsSmokedGlass()) {
            smoked::Chevron(GetWide(), GetTall(), -1, smoked::ColorOf(cse::ui::Text));
            return;
        }
        Button::Paint();
    }

	void ApplySchemeSettings( IScheme* pScheme ) override
	{
		Button::ApplySchemeSettings( pScheme );
		SetFont( pScheme->GetFont( "Marlett", IsProportional() ) );
		SetText( "3" );
	}
};

class CRightButton : public Button
{
public:
	CRightButton( Panel* parent, const char* name, const char* text ) : Button( parent, name, text ) {}
    void Paint() override {
        if (cse::ui::ControlsSmokedGlass()) {
            smoked::Chevron(GetWide(), GetTall(), 1, smoked::ColorOf(cse::ui::Text));
            return;
        }
        Button::Paint();
    }

	void ApplySchemeSettings( IScheme* pScheme ) override
	{
		Button::ApplySchemeSettings( pScheme );
		SetFont( pScheme->GetFont( "Marlett", IsProportional() ) );
		SetText( "4" );
	}
};

class InsetBox : public Label
{
public:
	InsetBox( Panel* parent, const char* name, Panel* bottomPanel ) : Label( parent, name, (const char*)NULL )
	{
		m_pBottomPanel = bottomPanel;
	}
	void OnSetFocus() override
	{
		Panel::OnSetFocus();
		m_pBottomPanel->MoveToFront();
	}
	void Paint() override
	{
		Label::Paint();
		int wide, tall;
		GetSize( wide, tall );
		surface()->DrawSetTextColor( GetBgColor() );
		surface()->DrawLine( 0, 0, wide, 0 );
		surface()->DrawLine( 0, 0, 0, tall );
		surface()->DrawLine( 0, tall, wide, tall );
		surface()->DrawLine( wide, 0, wide, tall );
	}
	void ApplySchemeSettings( IScheme* pScheme ) override
	{
		Label::ApplySchemeSettings( pScheme );
		SetBgColor( GetSchemeColor( "Spectator/InsetBoxBorder", pScheme ) );
	}
	void OnMousePressed( MouseCode code ) override
	{
		if ( code == MOUSE_LEFT )
			gViewPortInterface->GetClientDllInterface()->CallEnghudClientCmd( "spec_toggleinset\n" );
		else
			Label::OnMousePressed( code );
	}

private:
	Panel* m_pBottomPanel;
};

// Spectator controls share one rail material; their individual fields remain transparent.
class CSpectatorComboBox : public ComboBox {
public:
    CSpectatorComboBox(Panel* parent, const char* name, int rows = 10, bool editable = false)
        : ComboBox(parent, name, rows, editable) {}

    void PaintBackground() override {
        if (!cse::ui::ControlsSmokedGlass()) {
            ComboBox::PaintBackground();
            return;
        }
        const Color background = GetBgColor();
        SetBgColor(Color(0, 0, 0, 0));
        ComboBox::PaintBackground();
        SetBgColor(background);
    }

    void PaintBorder() override {
        if (!cse::ui::ControlsSmokedGlass()) {
            ComboBox::PaintBorder();
        } else if (HasFocus()) {
            smoked::Outline(GetWide(), GetTall(), smoked::Radius(this, cse::ui::MaterialRole::Control),
                smoked::ColorOf(cse::ui::Edge));
        }
    }
};

class CBottomBar : public Frame
{
	DECLARE_CLASS_SIMPLE_NAMED( CBottomBar, Frame, "vgui::Frame" );
public:
	class CommandComboBox : public CSpectatorComboBox
	{
	public:
		CommandComboBox( Panel* parent, const char* name ) : CSpectatorComboBox( parent, name, 10, false ) {}
		void OnCommand( const char* command ) override;
		void OnShowMenu( Menu* menu ) override { SetMenuCvarChecks( menu ); }
		void SetMenuCvarChecks( Menu* menu );
	};

	CBottomBar( Panel* parent, const char* name );
    void PerformLayout() override;
    void PaintBackground() override;
    void SetTarget(const SpectatorPlayerText& target, const wchar_t* fallback);
	void ApplySchemeSettings( IScheme* pScheme ) override;
	void SetVisible( bool state ) override;
	void OnKeyCodePressed( KeyCode code ) override;
	void OnCommand( const char* command ) override;
	virtual InsetBox* GetInsetBox() { return m_pInsetBox; }
	int PlayerAddItem( int itemID, wchar_t* text, KeyValues* data );
	void SetPlayerText( const wchar_t* text );
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", data );

	ComboBox*		m_pPlayerList;
	CommandComboBox*	m_pViewOptions;
	CommandComboBox*	m_pConfigSettings;
	InsetBox*		m_pInsetBox;
	CLeftButton*	m_pLeftButton;
	CRightButton*	m_pRightButton;
	bool			m_bChooseTeamCommand;
	KeyCode			m_iChooseTeamKey;
    Label* m_pHealthLabel = nullptr;
    cse::ui::BackdropRect m_originalControlBounds[5]{};
    bool m_layoutReady = false;
    int m_teamNumber = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBottomBar::CBottomBar( Panel* parent, const char* name ) : Frame( parent, name, true, false )
{
	m_iChooseTeamKey = KEY_NONE;
	m_bChooseTeamCommand = false;
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );
	SetTitleBarVisible( false ); // don't draw a title bar
	SetMoveable( false );
	SetSizeable( false );
	SetScheme( "ClientScheme" );
	m_pPlayerList = new CSpectatorComboBox(this, "playercombo", 10, false);
	HFont fallback = scheme()->GetIScheme( GetScheme() )->GetFont( "DefaultVerySmallFallBack", false );
	if ( fallback )
		m_pPlayerList->SetUseFallbackFont( true, fallback );
	m_pViewOptions = new CommandComboBox( this, "viewcombo" );
	m_pConfigSettings = new CommandComboBox( this, "settingscombo" );
	m_pLeftButton = new CLeftButton( this, "playerprev", "" );
	m_pRightButton = new CRightButton( this, "playernext", "" );
	m_pInsetBox = new InsetBox( parent, "bottombarinset", this );
	m_pInsetBox->SetPaintBackgroundEnabled( false );
	m_pInsetBox->SetVisible( false );
	m_pInsetBox->MakePopup( true, false );
	m_pInsetBox->SetMouseInputEnabled( true );
	m_pInsetBox->SetKeyBoardInputEnabled( false );
	m_pPlayerList->SetText( "" );
	m_pViewOptions->SetText( "#CAM_OPTIONS" );
	m_pConfigSettings->SetText( "#SPECT_OPTIONS" );
	m_pPlayerList->SetOpenDirection( ComboBox::UP );
	m_pViewOptions->SetOpenDirection( ComboBox::UP );
	m_pConfigSettings->SetOpenDirection( ComboBox::UP );
	int team = gViewPortInterface->GetClientDllInterface()->TeamNumber();
	CreateCommandCombo()->CreateCommandComboMenu( (char*)"spectcammenu.txt", m_pViewOptions, team );
	CreateCommandCombo()->CreateCommandComboMenu( (char*)"spectatormenu.txt", m_pConfigSettings, team );
	m_pConfigSettings->GetMenu()->AddActionSignalTarget( this );

	// set ourselves to be as wide as the screen
	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	SetSize( wide, GetTall() );
	LoadControlSettings( "Resource/UI/BottomSpectator.res", NULL );
    Panel* controls[]{m_pConfigSettings, m_pLeftButton, m_pPlayerList, m_pRightButton, m_pViewOptions};
    for (std::size_t index = 0; index < std::size(controls); ++index) {
        auto& bounds = m_originalControlBounds[index];
        controls[index]->GetBounds(bounds.x, bounds.y, bounds.width, bounds.height);
    }
    m_pHealthLabel = new Label(this, "playerhealth", "");
    m_pHealthLabel->SetContentAlignment(Label::a_center);
    m_pHealthLabel->SetVisible(false);
    m_layoutReady = true;
    InvalidateLayout(false, false);
}

void CBottomBar::PerformLayout() {
    Frame::PerformLayout();
    if (!m_layoutReady) {
        return;
    }
    Panel* controls[]{m_pConfigSettings, m_pLeftButton, m_pPlayerList, m_pRightButton, m_pViewOptions};
    if (!cse::ui::ControlsSmokedGlass()) {
        for (std::size_t index = 0; index < std::size(controls); ++index) {
            const auto& bounds = m_originalControlBounds[index];
            controls[index]->SetBounds(bounds.x, bounds.y, bounds.width, bounds.height);
        }
        m_pHealthLabel->SetVisible(false);
        return;
    }
    const HFont font = m_pHealthLabel->GetFont();
    const int fontTall = surface()->GetFontTall(font);
    const auto rail = cse::ui::SpectatorRail(GetWide(), GetTall(), fontTall);
    const cse::ui::BackdropRect bounds[]{rail.settings, rail.previous, rail.player, rail.next, rail.camera};
    for (std::size_t index = 0; index < std::size(controls); ++index) {
        controls[index]->SetBounds(bounds[index].x, bounds[index].y, bounds[index].width, bounds[index].height);
    }
    m_pHealthLabel->SetBounds(rail.health.x, rail.health.y, rail.health.width, rail.health.height);
    // Resource controls start shorter than the shared rail. Measure fonts after their final bounds are known.
    IScheme* currentScheme = scheme()->GetIScheme(GetScheme());
    ComboBox* combos[]{m_pConfigSettings, m_pPlayerList, m_pViewOptions};
    for (ComboBox* combo : combos) {
        combo->SetFont(smoked::TextFont(combo, currentScheme, combo->GetFont()));
    }
    m_pHealthLabel->SetFont(smoked::TextFont(m_pHealthLabel, currentScheme, m_pHealthLabel->GetFont()));
}

void CBottomBar::PaintBackground() {
    if (!m_layoutReady || !cse::ui::ControlsSmokedGlass()) {
        Frame::PaintBackground();
        return;
    }
    const HFont font = m_pHealthLabel->GetFont();
    const auto rail = cse::ui::SpectatorRail(GetWide(), GetTall(), surface()->GetFontTall(font));
    smoked::Surface(this, rail.surface);
    const int dividerTop = rail.settings.y + rail.settings.height / 4;
    const int dividerHeight = rail.settings.height / 2;
    const int inset = smoked::Pixels(22);
    const int dividers[]{rail.settings.x + rail.settings.width + inset,
        rail.health.x - smoked::Pixels(10), rail.camera.x - inset};
    for (const int divider : dividers) {
        smoked::Fill(divider, dividerTop, 1, dividerHeight, 0, smoked::ColorOf(cse::ui::Edge));
    }
    if (m_teamNumber > 0) {
        constexpr cse::ui::Rgba Terrorist{191, 67, 57, 255};
        constexpr cse::ui::Rgba CounterTerrorist{113, 166, 218, 255};
        const int side = (std::max)(4, rail.player.height / 4);
        smoked::Fill(rail.player.x - side - 4, rail.player.y + (rail.player.height - side) / 2,
            side, side, 0, smoked::ColorOf(m_teamNumber == 1 ? Terrorist : CounterTerrorist));
    }

}

void CBottomBar::SetTarget(const SpectatorPlayerText& target, const wchar_t* fallback) {
    if (cse::ui::ControlsSmokedGlass() && target.formatted) {
        m_pPlayerList->TextEntry::SetText(target.name.c_str());
        m_teamNumber = target.teamNumber;
        const wchar_t* healthWord = localize()->Find("#Cstrike_TitlesTXT_Health");
        const std::wstring health = target.withDetail
            ? std::wstring(healthWord ? healthWord : L"Health") + L" " + target.detail : L"";
        m_pHealthLabel->SetText(health.c_str());
        m_pHealthLabel->SetVisible(target.withDetail);
    } else {
        m_teamNumber = 0;
        m_pPlayerList->TextEntry::SetText(fallback);
        m_pHealthLabel->SetVisible(false);
    }
}

void CBottomBar::CommandComboBox::OnCommand( const char* command )
{
	if ( !_stricmp( command, "ButtonClicked" ) )
		ComboBox::OnCommand( command );
	else
		gViewPortInterface->GetClientDllInterface()->CallEnghudClientCmd( command );
}

void CBottomBar::CommandComboBox::SetMenuCvarChecks( Menu* menu )
{
	if ( !menu )
		return;

	for ( int i = 0; i < menu->GetItemCount(); ++i )
	{
		MenuItem* item = menu->GetMenuItem( menu->GetMenuID( i ) );
		if ( item->IsCheckable() )
		{
			KeyValues* data = new KeyValues( "GetCommand" );
			item->RequestInfo( data );
			const char* command = data->GetString( "command", "" );
			if ( strstr( command, "spec_mode" ) )
				item->SetChecked( !gViewPortInterface->GetClientDllInterface()->PipInsetOff() );
			else if ( strstr( command, "togglescores" ) )
				item->SetChecked( gViewPortInterface->IsScoreBoardVisible() );
			else
			{
				char cvarName[64];
				_snprintf( cvarName, sizeof( cvarName ), "%s_internal", command );
				item->SetChecked( gViewPortInterface->GetClientDllInterface()->CvarGetFloat( cvarName ) > 0 );
			}
		}
		if ( item->HasMenu() )
			SetMenuCvarChecks( item->GetMenu() );
	}
}

void CBottomBar::OnTextChanged( KeyValues* data )
{
	ComboBox* panel = dynamic_cast<ComboBox*>( (Panel*)data->GetPtr( "panel", NULL ) );
	if ( panel == m_pConfigSettings )
		panel->SetText( "#SPECT_OPTIONS" );
	else if ( panel == m_pPlayerList )
	{
		KeyValues* selected = panel->GetActiveItemUserData();
		if ( selected )
			gViewPortInterface->GetClientDllInterface()->FindPlayer( selected->GetString( "player", "" ) );
	}
}

void CBottomBar::ApplySchemeSettings( IScheme* pScheme )
{
	Frame::ApplySchemeSettings( pScheme );
	// need to MakeReadyForUse() on the menus so we can set their bg color before they are displayed
	m_pConfigSettings->GetMenu()->MakeReadyForUse();
	m_pViewOptions->GetMenu()->MakeReadyForUse();
	m_pPlayerList->GetMenu()->MakeReadyForUse();
	m_pConfigSettings->GetMenu()->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pViewOptions->GetMenu()->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pPlayerList->GetMenu()->SetBgColor( Color( 0, 0, 0, 196 ) );
    if (m_pHealthLabel && cse::ui::ControlsSmokedGlass()) {
        // Normal scheme traversal skips this initially hidden label; layout still needs its font metrics.
        m_pHealthLabel->MakeReadyForUse();
        m_pHealthLabel->SetFgColor(smoked::ColorOf(cse::ui::SecondaryText));
    }

}

void CBottomBar::SetVisible( bool state )
{
	Panel::SetVisible( state );
	if ( m_iChooseTeamKey == KEY_NONE )
	{
		m_iChooseTeamKey = (KeyCode)gameuifuncs->GetVGUI2KeyCodeForBind( "chooseteam" );
		if ( m_iChooseTeamKey != KEY_NONE )
			m_bChooseTeamCommand = true;
		else
			m_iChooseTeamKey = (KeyCode)gameuifuncs->GetVGUI2KeyCodeForBind( "changeteam" );
	}
}

void CBottomBar::OnKeyCodePressed( KeyCode code )
{
	if ( m_iChooseTeamKey != KEY_NONE && m_iChooseTeamKey == code )
	{
		gViewPortInterface->HideSpectatorGUI();
		gViewPortInterface->GetClientDllInterface()->CallEnghudClientCmd(
			m_bChooseTeamCommand ? "chooseteam\n" : "changeteam\n" );
	}
	else
	{
		Panel::OnKeyCodePressed( code );
	}
}

void CBottomBar::OnCommand( const char* command )
{
	if ( !_stricmp( command, "playernext" ) )
		gViewPortInterface->GetClientDllInterface()->FindNextPlayer( true );
	else if ( !_stricmp( command, "playerprev" ) )
		gViewPortInterface->GetClientDllInterface()->FindNextPlayer( false );
}

int CBottomBar::PlayerAddItem( int itemID, wchar_t* text, KeyValues* data )
{
	if ( m_pPlayerList->IsItemIDValid( itemID ) )
		m_pPlayerList->UpdateItem( itemID, text, data );
	else
		return m_pPlayerList->AddItem( text, data ) + 1;
	return itemID + 1;
}

void CBottomBar::SetPlayerText( const wchar_t* text )
{
    m_pHealthLabel->SetVisible(false);
    m_teamNumber = 0;
	int length = (int)wcslen( text ) + 1;
	CUtlVector<char> ansiText;
	ansiText.AddMultipleToTail( length );
	localize()->ConvertUnicodeToANSI( text, ansiText.Base(), length );
	m_pPlayerList->SetText( ansiText.Base() );
}

CSpectatorGUI::CSpectatorGUI( Panel* parent ) : Frame( parent, "SpectatorGUI", true, false )
{
	m_iDuckKey = KEY_NONE;
	m_bHelpShown = false;
	m_bInsetVisible = false;
	Panel::SetVisible( false );
	SetTitle( "SpectatorGUI", true );
	SetProportional( true );

	// load the new scheme early!!
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );

	// hide the system buttons
	SetTitleBarVisible( false );
    m_pTopBar = new CSpectatorBar(this, "topbar", true);
	m_pBottomBar = new CBottomBar( this, "bottombar" );
    m_pBottomBarBlank = new CSpectatorBar(this, "bottombarblank", false);
	m_pBannerImage = new ImagePanel( m_pTopBar, "banner" );
	m_pInsetBox = new InsetBox( this, "insetBox", this );

	// m_pBannerImage = new ImagePanel( m_pTopBar, NULL );
	m_pPlayerLabel = new Label( this, "playerlabel", "" );
	m_pPlayerLabel->SetVisible( false );
	m_pPlayerLabel->SetText( "" );
	TextImage* text = m_pPlayerLabel->GetTextImage();
	if ( text )
	{
		HFont fallback = scheme()->GetIScheme( GetScheme() )->GetFont( "DefaultVerySmallFallBack", false );
		if ( fallback )
			text->SetUseFallbackFont( true, fallback );
	}
	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	tall = GetProportionalScaledValue( 64 );
	SetSize( wide, tall );
	m_pTopBar->SetSize( wide, tall );
	m_pBottomBar->SetSize( wide, tall );
	m_pBottomBarBlank->SetSize( wide, tall );
	LoadControlSettings( "Resource/UI/Spectator.res", NULL );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	m_pInsetBox->SetPaintBackgroundEnabled( false );
	m_pInsetBox->SetVisible( false );
	m_pBottomBarBlank->SetVisible( true );
	m_pBannerImage->SetVisible( false );
	InvalidateLayout( false, false );
	m_pTimerLabel = dynamic_cast<Label*>( FindChildByName( "timerlabel", false ) );
	m_pExtraInfoLabel = dynamic_cast<Label*>( FindChildByName( "extrainfo", false ) );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSpectatorGUI::~CSpectatorGUI()
{
    SpectatorPresentations().erase(this);
}

//-----------------------------------------------------------------------------
// Purpose: shows the screen
//-----------------------------------------------------------------------------
void CSpectatorGUI::Activate()
{
	Frame::Activate();
	m_bHelpShown = false;
	SetVisible( true );
	m_pPlayerLabel->SetVisible( true ); // hide the bottom bar by default
	m_pPlayerLabel->MoveToFront();
	m_pPlayerLabel->SetText( "" );
	m_pBottomBar->SetVisible( false );
	m_pBannerImage->SetVisible( true );
	if ( !gViewPortInterface->GetClientDllInterface()->PipInsetOff() )
		EnableInsetView( !gViewPortInterface->GetClientDllInterface()->PipInsetOff() );
}

void CSpectatorGUI::HideGUI()
{
	DeactivateGUI();
	SetVisible( false );
	m_pPlayerLabel->SetVisible( true );
	m_pPlayerLabel->MoveToFront();
	m_pPlayerLabel->SetText( "" );
	m_pInsetBox->SetVisible( false );
	m_pBottomBar->SetVisible( false );
	m_pBottomBar->GetInsetBox()->SetVisible( false );
	SetImage( NULL );
}

bool CSpectatorGUI::IsGUIVisible()
{
	return IsVisible();
}

bool CSpectatorGUI::IsActiveGUIVisible()
{
	return m_pBottomBar->IsVisible();
}

bool CSpectatorGUI::CanSpectateTeam( int )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: makes the GUI fill the screen
//-----------------------------------------------------------------------------
void CSpectatorGUI::PerformLayout()
{
	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	SetBounds( 0, 0, wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the colour of the top and bottom bars
//-----------------------------------------------------------------------------
void CSpectatorGUI::ApplySchemeSettings( IScheme* pScheme )
{
	Frame::ApplySchemeSettings( pScheme );
	SetBgColor( Color( 0, 0, 0, 0 ) ); // make the background transparent
	m_pTopBar->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pBottomBarBlank->SetBgColor( Color( 0, 0, 0, 196 ) );
	m_pBottomBar->SetBgColor( Color( 0, 0, 0, 0 ) );
	SetPaintBorderEnabled( false );
	SetBorder( NULL );
}

void CSpectatorGUI::UpdateTimer()
{
	wchar_t text[64];
	_snwprintf( text, 63, L"%d:%02d\n", 0, 0 );
	text[63] = 0;
	m_pTimerLabel->SetText( text );
}

void CSpectatorGUI::EnableInsetView( int enabled )
{
	int screenwide, screentall;
	surface()->GetScreenSize( screenwide, screentall );
	if ( enabled )
	{
		int x, y, wide, tall;
		gViewPortInterface->GetClientDllInterface()->InsetValues( x, y, wide, tall );
		m_pInsetBox->SetBounds( x, y, wide, tall );
		m_pBottomBar->GetInsetBox()->SetBounds( x, y, wide, tall );
		m_pTopBar->SetPos( wide + x + 2, 0 );
		m_pInsetBox->SetVisible( true );
		if ( m_pBottomBar->IsVisible() )
		{
			m_pBottomBar->GetInsetBox()->SetVisible( true );
			m_bInsetVisible = true;
		}
	}
	else
	{
		m_pInsetBox->SetVisible( false );
		if ( m_pBottomBar->IsVisible() )
		{
			m_pBottomBar->GetInsetBox()->SetVisible( false );
			m_bInsetVisible = false;
		}
		m_pTopBar->SetPos( 0, 0 );
	}
}

void CSpectatorGUI::SetImage( const char* image )
{
	if ( image && m_pBannerImage )
	{
		CUtlVector<char> imageName;
        const size_t imageBytes = strlen(image);
        if (imageBytes >= INT_MAX) {
            return;
        }
        imageName.AddMultipleToTail(static_cast<int>(imageBytes) + 1);
		strcpy( imageName.Base(), image );
		m_pBannerImage->SetImage( scheme()->GetImage( imageName.Base(), false ) );
		m_pBannerImage->InvalidateLayout( false, false );
		m_pBannerImage->SetPos( 0, 0 );
		m_pBannerImage->SetSize( 256, GetProportionalScaledValue( 64 ) );
		m_pBannerImage->SetVisible( true );
	}
	else
	{
		m_pBannerImage->SetVisible( false );
	}
}

void CSpectatorGUI::OnCommand( const char* command )
{
	if ( !_stricmp( command, "okay" ) )
	{
		m_pPlayerLabel->SetVisible( true );
		m_pPlayerLabel->MoveToFront();
		m_pBottomBar->SetVisible( false );
	}
	Frame::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: when duck is pressed it hides the active part of the GUI
//-----------------------------------------------------------------------------
void CSpectatorGUI::OnKeyCodePressed( KeyCode code )
{
	m_iDuckKey = (KeyCode)gameuifuncs->GetVGUI2KeyCodeForBind( "duck" );
	if ( m_iDuckKey != KEY_NONE && m_iDuckKey == code )
	{
		SetKeyBoardInputEnabled( false );
		m_pBottomBar->SetVisible( false );
		if ( m_bInsetVisible )
		{
			m_pBottomBar->GetInsetBox()->SetVisible( false );
			m_pInsetBox->SetVisible( true );
		}
		m_pPlayerLabel->SetVisible( true );
		m_pPlayerLabel->MoveToFront();
	}
	else
	{
		Frame::OnKeyCodePressed( code );
	}
}

void CSpectatorGUI::OnThink()
{
	static int lastWide = 0;
	static int lastTall = 0;
	if ( !IsVisible() )
		return;

	int wide, tall;
	surface()->GetScreenSize( wide, tall );
	if ( lastWide != wide || lastTall != tall )
	{
		lastWide = wide;
		lastTall = tall;
		int barHeight = GetProportionalScaledValue( 64 );
		SetSize( wide, barHeight );
		m_pTopBar->SetSize( wide, barHeight );
		m_pBottomBar->SetSize( wide, barHeight );
		m_pBottomBar->SetPos( 0, tall - barHeight );
		m_pBottomBarBlank->SetSize( wide, barHeight );
		m_pBottomBarBlank->SetPos( 0, tall - barHeight );
		InvalidateLayout( false, false );
	}

    // Activation clears the caption, and player info may arrive after the observer target.
    // Refresh only the caption so an open player menu keeps its rows and selection.
    const auto* client = gViewPortInterface->GetClientDllInterface();
    const int mode = client->SpectatorMode();
    const int player = client->SpectatorTarget();
    constexpr int RoamingMode = 3;
    if (mode > 0 && mode != RoamingMode && player > 0 && player <= client->GetMaxPlayers()) {
        UpdatePlayerLabel(player);
    }
}

void CSpectatorGUI::DeactivateGUI()
{
	m_pPlayerLabel->SetVisible( true );
	m_pBottomBar->SetPlayerText( L"" );
	m_pInsetBox->SetVisible( false );
	SetKeyBoardInputEnabled( false );
	m_pBottomBar->SetVisible( false );
}

void CSpectatorGUI::ShowBottomBar()
{
	SetKeyBoardInputEnabled( true );
	m_pBottomBar->SetVisible( true );
	UpdateSpectatorPlayerList();
	m_pPlayerLabel->SetVisible( false );
	m_pPlayerLabel->SetText( "" );
	m_pBottomBar->SetPlayerText( L"" );
	m_pBottomBar->MoveToFront();
}

int CSpectatorGUI::GetTopBarHeight()
{
	return m_pTopBar->GetTall();
}

int CSpectatorGUI::GetBottomBarHeight()
{
	return m_pBottomBarBlank->GetTall();
}

void CSpectatorGUI::SetLabelText( const char* name, const char* text )
{
	Label* label = dynamic_cast<Label*>( FindChildByName( name, false ) );
    if (label) {
        label->SetText(text);
        if (label == m_pExtraInfoLabel) {
            SpectatorPresentations()[this].extraOverridden = true;
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text of a control by name
//-----------------------------------------------------------------------------
void CSpectatorGUI::SetLabelText( const char* name, wchar_t* text )
{
	Label* label = dynamic_cast<Label*>( FindChildByName( name, false ) );
    if (label) {
        label->SetText(text);
        if (label == m_pExtraInfoLabel) {
            SpectatorPresentations()[this].extraOverridden = true;
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text of a control by name
//-----------------------------------------------------------------------------
void CSpectatorGUI::MoveLabelToFront( const char* name )
{
	Label* label = dynamic_cast<Label*>( FindChildByName( name, false ) );
	if ( label )
		label->MoveToFront();
}

void CSpectatorGUI::GetPlayerLabelText( int player, wchar_t* text, int textLength )
{
	wcscpy( text, L"Unable to find #Spec_PlayerItem*" );
	wchar_t playerName[64] = L"";
	VGuiLibraryPlayer_t info = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( player );
	if ( info.name )
	{
		int nameLength = 2 * (int)strlen( info.name ) + 1;
		CUtlVector<char> safeName;
		safeName.AddMultipleToTail( nameLength );
		gViewPortInterface->MakeSafeName( info.name, safeName.Base(), nameLength );
		localize()->ConvertANSIToUnicode( safeName.Base(), playerName, sizeof( playerName ) );
	}
	else
	{
		localize()->ConvertANSIToUnicode( NULL, playerName, sizeof( playerName ) );
	}
	if ( info.health > 0 )
	{
		wchar_t health[10];
		_snwprintf( health, 10, L"%d", info.health );
		localize()->ConstructString( text, textLength, localize()->Find( "#Spec_PlayerItem_Team" ),
			2, playerName, health );
	}
	else
	{
		localize()->ConstructString( text, textLength, localize()->Find( "#Spec_PlayerItem" ), 1, playerName );
	}
    SpectatorPresentation& presentation = SpectatorPresentations()[this];
    if (presentation.captureTarget) {
        presentation.target.name = playerName;
        presentation.target.teamNumber = info.teamnumber;
        presentation.target.withDetail = info.health > 0;
        presentation.target.detail = std::to_wstring(info.health);
        presentation.target.fallback = text;
        presentation.target.formatted = true;
    }
}

void CSpectatorGUI::GetPlayerLabelColor( int player, Color& color )
{
	if ( player )
	{
		float* clientColor = GetClientColor( player );
		color = Color( (int)( clientColor[0] * 255 ), (int)( clientColor[1] * 255 ),
			(int)( clientColor[2] * 255 ), 255 );
	}
	else
	{
		color = scheme()->GetIScheme( GetScheme() )->GetColor( "BaseText", Color( 255, 255, 255, 0 ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Resets the list of players
//-----------------------------------------------------------------------------
void CSpectatorGUI::UpdateSpectatorPlayerList()
{
	if ( !m_pBottomBar->IsVisible() )
		return;

    SpectatorPresentation& presentation = SpectatorPresentations()[this];
    presentation.rows.clear();
	m_pBottomBar->m_pPlayerList->DeleteAllItems();
	int itemID = 0;
	for ( int player = 1; player < gViewPortInterface->GetClientDllInterface()->GetMaxPlayers(); ++player )
	{
		VGuiLibraryPlayer_t info = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( player );
		if ( !info.name || info.thisplayer || info.dead || !CanSpectateTeam( info.teamnumber ) )
			continue;

		wchar_t playerText[80], playerName[64], teamText[64];
		localize()->ConvertANSIToUnicode( info.name, playerName, sizeof( playerName ) );
		if ( info.teamname )
		{
			char teamToken[64];
			_snprintf( teamToken, sizeof( teamToken ), "#%s", info.teamname );
			wchar_t* teamName = localize()->Find( teamToken );
			if ( !teamName )
			{
				localize()->ConvertANSIToUnicode( info.teamname, teamText, sizeof( teamText ) );
				teamName = teamText;
			}
			localize()->ConstructString( playerText, sizeof( playerText ),
				localize()->Find( "#Spec_PlayerItem_Team" ), 2, playerName, teamName );
		}
		else
		{
			localize()->ConstructString( playerText, sizeof( playerText ),
				localize()->Find( "#Spec_PlayerItem" ), 1, playerName );
		}
        SpectatorPlayerText row;
        row.name = playerName;
        row.withDetail = info.teamname != nullptr;
        if (info.teamname) {
            localize()->ConvertANSIToUnicode(info.teamname, teamText, sizeof(teamText));
            row.detail = teamText;
            // Only client-owned team labels are catalogue keys; arbitrary identities remain literal.
            row.detailToken = info.teamnumber == 1 ? "#TERRORIST" : info.teamnumber == 2 ? "#CT" : "";
        }
        row.fallback = playerText;
        row.formatted = true;
		KeyValues* data = new KeyValues( "UserData", "player", info.name );
		if ( m_pBottomBar->m_pPlayerList->IsItemIDValid( itemID ) )
			m_pBottomBar->m_pPlayerList->UpdateItem( itemID++, playerText, data );
		else
			itemID = m_pBottomBar->m_pPlayerList->AddItem( playerText, data ) + 1;
        presentation.rows[itemID - 1] = row;
	}
}

void CSpectatorGUI::UpdatePlayerLabel(int player) {
    SpectatorPresentation& presentation = SpectatorPresentations()[this];
    presentation.target = SpectatorPlayerText{};
    presentation.captureTarget = true;
    Color color(0, 0, 0, 0);
    GetPlayerLabelColor(player, color);
    m_pBottomBar->m_pPlayerList->SetFgColor(color);
    m_pPlayerLabel->SetFgColor(color);
    constexpr int PlayerTextCharacters = 80;
    wchar_t playerText[PlayerTextCharacters] = {};
    GetPlayerLabelText(player, playerText, sizeof(playerText));
    presentation.captureTarget = false;
    // An overriding formatter may own another presentation; retain that result literally.
    if (presentation.target.fallback != playerText) {
        presentation.target.formatted = false;
    }
    presentation.target.fallback = playerText;
    m_pBottomBar->SetTarget(presentation.target, playerText);
    m_pPlayerLabel->SetText(playerText);
}

void CSpectatorGUI::Update( const char*, int player, int mode, bool spectateOnly,
	int spectatorCount, const char* mapName )
{
    SpectatorPresentation& presentation = SpectatorPresentations()[this];
    presentation.hasUpdate = true;
    presentation.extraOverridden = false;
    presentation.spectateOnly = spectateOnly;
    presentation.spectatorCount = spectatorCount;
    presentation.mapName = mapName ? mapName : "";
    presentation.modeToken = GetSpectatorLabel(mode);
	m_pBottomBar->m_pViewOptions->SetText( GetSpectatorLabel( mode ) );
	if ( !m_bHelpShown && !m_pBottomBar->IsVisible() && mode )
	{
		m_bHelpShown = true;
		DuckMessage( "#Spec_Duck" );
	}
    UpdatePlayerLabel(player);
	wchar_t text[1024], wideName[64];
	if ( spectateOnly )
	{
		char count[6];
		_snprintf( count, sizeof( count ), "%i", spectatorCount );
		localize()->ConvertANSIToUnicode( count, wideName, 6 * sizeof( wchar_t ) );
		localize()->ConstructString( text, sizeof( text ), localize()->Find( "#Spectators" ), 1, wideName );
		wcscat( text, L" " );
		wcscat( text, wideName );
	}
	else
	{
		localize()->ConvertANSIToUnicode( mapName, wideName, sizeof( wideName ) );
		localize()->ConstructString( text, sizeof( text ), localize()->Find( "#Spec_Map" ), 1, wideName );
	}
	if ( m_pExtraInfoLabel )
		m_pExtraInfoLabel->SetText( text );
}

void CSpectatorGUI::RefreshLanguage() {
    // Generic menu bindings translate existing items without rebuilding commands.
    // Consume revisions before restoring the headings and authoritative current mode.
    m_pBottomBar->m_pViewOptions->RefreshLanguageText();
    m_pBottomBar->m_pConfigSettings->RefreshLanguageText();
    const wchar_t* settings = localize()->Find("#SPECT_OPTIONS");
    if (settings) {
        m_pBottomBar->m_pConfigSettings->TextEntry::SetText(settings);
    }
    const auto found = SpectatorPresentations().find(this);
    if (found == SpectatorPresentations().end() || !found->second.hasUpdate) {
        const wchar_t* camera = localize()->Find("#CAM_OPTIONS");
        if (camera) {
            m_pBottomBar->m_pViewOptions->TextEntry::SetText(camera);
        }
    }
    if (found == SpectatorPresentations().end()) {
        return;
    }
    const SpectatorPresentation& presentation = found->second;
    if (presentation.hasUpdate) {
        m_pBottomBar->m_pPlayerList->RefreshLanguageText();
        const wchar_t* mode = localize()->Find(presentation.modeToken.c_str());
        if (mode) {
            m_pBottomBar->m_pViewOptions->TextEntry::SetText(mode);
        }
        const std::wstring playerText = FormatSpectatorPlayer(presentation.target);
        m_pBottomBar->SetTarget(presentation.target, playerText.c_str());
        m_pPlayerLabel->SetText(playerText.c_str());
        if (m_pExtraInfoLabel && !presentation.extraOverridden) {
            const std::wstring extra = FormatSpectatorExtra(presentation);
            m_pExtraInfoLabel->SetText(extra.c_str());
        }
    }
    for (const auto& row : presentation.rows) {
        if (m_pBottomBar->m_pPlayerList->IsItemIDValid(row.first)) {
            const std::wstring text = FormatSpectatorPlayer(row.second);
            m_pBottomBar->m_pPlayerList->UpdateItem(row.first, text.c_str(), nullptr);
        }
    }
    InvalidateLayout(false, false);
}
