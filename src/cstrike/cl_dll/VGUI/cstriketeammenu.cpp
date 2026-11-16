//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "smoked_game_layout.h"
#include "hud.h" // for gEngfuncs
#include "buy_presets.h"
#include "cstriketeammenu.h"
#include "counterstrikeviewport.h"
#include "game_controls/iviewport.h"
#include "Button.h"
#include "cl_entity.h"
#include <string.h>

extern int g_iFreezeTimeOver;

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CCSTeamMenu::CCSTeamMenu( vgui2::Panel* parent ) : CTeamMenu( parent )
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSTeamMenu::~CCSTeamMenu()
{
}

void CCSTeamMenu::SetVisibleButton( const char* name, bool state )
{
	vgui2::Button* button = dynamic_cast<vgui2::Button*>( FindChildByName( name, false ) );
	if ( button )
	{
		button->SetVisible( state );
	}
}

void CCSTeamMenu::Update( const char* mapName, bool allowSpectators, const char**, int )
{
	CTeamMenu::Update( mapName, allowSpectators, NULL, 0 );

	if ( gViewPortInterface->GetAllowSpectators() )
	{
		cl_entity_t* player = gEngfuncs.GetLocalPlayer();
		if ( g_iTeamNumber == 0 || !g_iFreezeTimeOver || ( player && g_PlayerExtraInfo[player->index].dead == true ) )
			SetVisibleButton( "specbutton", true );
		else
			SetVisibleButton( "specbutton", false );
	}
	else
	{
		SetVisibleButton( "specbutton", false );
	}

	m_bVIPMap = IsASMapType();
	if ( m_bVIPMap && g_iTeamNumber == 2 )
		SetVisibleButton( "vipbutton", true );
	else
		SetVisibleButton( "vipbutton", false );

	if ( g_iTeamNumber )
		SetVisibleButton( "CancelButton", true );
	else
		SetVisibleButton( "CancelButton", false );
}

//-----------------------------------------------------------------------------
// Purpose: When a team button is pressed it triggers this function to 
//			cause the player to join a team
//-----------------------------------------------------------------------------
void CCSTeamMenu::OnCommand( const char* command )
{
	if ( stricmp( command, "vguicancel" ) )
	{
		gEngfuncs.pfnClientCmd( (char*)command );
	}
	if ( !stricmp( command, "jointeam 6" ) )
	{
		gViewPortInterface->HideBackGround();
		gViewPortInterface->ShowSpectatorGUI();
	}
	else if ( !stricmp( command, "vguicancel" ) )
	{
		gViewPortInterface->HideBackGround();
	}
	OnClose();
	vgui2::Frame::OnCommand( command );
}

void CCSTeamMenu::ApplySchemeSettings(vgui2::IScheme* scheme) {
    const bool restorePosition = cse::ui::ControlsStyleChanged(this) && !cse::ui::ControlsSmokedGlass();
    CTeamMenu::ApplySchemeSettings(scheme);
    auto* viewport = dynamic_cast<CounterStrikeViewport*>(gViewPortInterface);
    if (restorePosition && viewport && viewport->HasCenteredPersistentWindows()) {
        // CenterWindow adds the legacy viewport's letterbox offset only on first activation.
        // Resource restoration removes it, so restore the same offset on a live style switch.
        constexpr int LegacyViewportWidth = 640;
        constexpr int LegacyViewportHeight = 480;
        constexpr float VerticalOffsetScale = 0.67f;
        int screenWidth = 0;
        int screenHeight = 0;
        vgui2::surface()->GetScreenSize(screenWidth, screenHeight);
        int x = 0;
        int y = 0;
        GetPos(x, y);
        const int offsetX = (screenWidth - GetAlternateProportionalValueFromNormal(LegacyViewportWidth)) / 2;
        const int offsetY = (screenHeight - GetAlternateProportionalValueFromNormal(LegacyViewportHeight)) / 2;
        SetPos(x + offsetX, y + static_cast<int>(offsetY * VerticalOffsetScale));
    }
}

void CCSTeamMenu::PerformLayout() {
    CTeamMenu::PerformLayout();
    if (!cse::ui::ControlsSmokedGlass()) {
        return;
    }
    constexpr int MenuWidth = 920;
    constexpr int MenuHeight = 472;
    cse::gameui::Center(this, MenuWidth, MenuHeight);
    const int inset = vgui2::smoked::Pixels(cse::ui::WindowInset);
    const int contentTop = vgui2::smoked::Pixels(cse::ui::WindowHeaderHeight + cse::ui::WindowInset);
    const int gap = vgui2::smoked::Pixels(cse::gameui::ControlGap);
    const int columnWidth = (std::min)(vgui2::smoked::Pixels(cse::gameui::ActionColumnWidth),
        static_cast<int>(GetWide() * cse::gameui::MaximumActionWidthFraction));
    const int rowHeight = vgui2::smoked::Pixels(cse::ui::ControlHeight);
    cse::gameui::Title(FindChildByName("joinTeam", false), GetWide());
    int y = contentTop;
    constexpr const char* Actions[]{"terbutton", "ctbutton", "vipbutton", "autobutton", "specbutton",
        "CancelButton"};
    for (const char* name : Actions) {
        auto* button = FindChildByName(name, false);
        if (button && button->IsVisible()) {
            button->SetBounds(inset, y, columnWidth, rowHeight);
            y += rowHeight + gap;
        }
    }
    const int documentX = inset + columnWidth + gap;
    cse::gameui::Bounds(this, "MapInfo", documentX, contentTop, GetWide() - documentX - inset,
        GetTall() - contentTop - inset);
}

void CCSTeamMenu::PaintBackground() {
    if (cse::ui::ControlsSmokedGlass()) {
        vgui2::smoked::Surface(this);
        return;
    }
    CTeamMenu::PaintBackground();
}
