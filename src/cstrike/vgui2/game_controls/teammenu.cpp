//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "teammenu.h"
#include "../../../vgui2/controls/Button.h"
#include "../../../vgui2/controls/RichText.h"
#include "../../../vgui2/controls/Label.h"
#include "vgui_controls/Controls.h"
#include "iviewport.h"
#include "igameuifuncs.h"
#include "../../../public/FileSystem.h"
#include "vgui/KeyValues.h"
#include "vgui/IScheme.h"
#include <stdio.h>
#include <stdlib.h> // MAX_PATH define
#include <string.h>
#include <climits>
#include "../../../common/resource_text.h"

extern IGameUIFuncs* gameuifuncs; // for key binding details
void DuckMessage( const char* text );

const char* GetStringTeamColor( int team )
{
	switch ( team )
	{
	case 0: return "team0";
	case 1: return "team1";
	case 2: return "team2";
	case 3: return "team3";
	default: return "team4";
	}
}

CTeamMenu::CTeamMenu( vgui2::Panel* parent ) : vgui2::Frame( parent, "TeamMenu", true, false )
{
	m_iNumTeams = 0;
	m_iJumpKey = 0;
	m_iScoreBoardKey = 0;

	// initialize dialog
	SetTitle( "", true );

	// load the new scheme early!!
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );

	// hide the system buttons
	SetTitleBarVisible( false );
	SetProportional( true );
	m_pMapInfo = new vgui2::RichText( this, "MapInfo", true );
	m_pMapName = new vgui2::Label( this, "mapname", "" );
	for ( int i = 0; i < 6; ++i )
	{
		char name[20];
		_snprintf( name, sizeof( name ), "teambutton%i", i );
		vgui2::Button* button = new vgui2::Button( this, name, "" );
		button->SetVisible( false );
		m_pTeamButtons.AddToTail( button );
	}
	LoadControlSettings( "Resource/UI/TeamMenu.res", NULL );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTeamMenu::~CTeamMenu()
{
}


//-----------------------------------------------------------------------------
// Purpose: When a team button is pressed it triggers this function to cause the player to join a team
//-----------------------------------------------------------------------------
void CTeamMenu::OnTeamButton( int team )
{
	char command[64];
	if ( m_iNumTeams > team )
		_snprintf( command, sizeof( command ), "jointeam %i", team + 1 );
	else if ( m_iNumTeams == team )
		_snprintf( command, sizeof( command ), "jointeam 5" );
	else // next is spectate
	{
		DuckMessage( "#Spec_Duck" );
		gViewPortInterface->HideBackGround();
	}
	gViewPortInterface->GetClientDllInterface()->CallEnghudClientCmd( command );
	SetVisible( false );
	OnClose();
}

void CTeamMenu::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	m_pMapInfo->SetFgColor( scheme->GetColor( "MapDescriptionText", Color( 255, 255, 255, 0 ) ) );
}

void CTeamMenu::Activate() {
    if (m_bMapPageRequested) {
        InvalidateMapPage();
        LoadMapPage(m_mapPageName.c_str());
    }
    vgui2::Frame::Activate();
    if (!m_iJumpKey) {
        m_iJumpKey = gameuifuncs->GetVGUI2KeyCodeForBind("jump");
    }
    if (!m_iScoreBoardKey) {
        m_iScoreBoardKey = gameuifuncs->GetVGUI2KeyCodeForBind("showscores");
    }
}

void CTeamMenu::OnKeyCodePressed( vgui2::KeyCode key )
{
	if ( key == vgui2::KEY_PAD_ENTER || ( m_iJumpKey && m_iJumpKey == key ) )
		OnTeamButton( m_iNumTeams );
	else if ( m_iScoreBoardKey && m_iScoreBoardKey == key )
	{
		if ( !gViewPortInterface->IsScoreBoardVisible() )
		{
			gViewPortInterface->HideBackGround();
			gViewPortInterface->ShowScoreBoard();
			SetVisible( false );
		}
	}
	else
		vgui2::Frame::OnKeyCodePressed( key );
}

//-----------------------------------------------------------------------------
// Purpose: makes the user choose the auto assign option
//-----------------------------------------------------------------------------
void CTeamMenu::AutoAssign()
{
	OnTeamButton( m_iNumTeams );
}

void CTeamMenu::InvalidateMapPage() {
    m_bMapPageLoaded = false;
}

void CTeamMenu::LoadMapPage(const char* map) {
    const std::string requestedMap = map ? map : "";
    if (m_bMapPageLoaded && m_mapPageName == requestedMap) {
        return;
    }
    m_bMapPageLoaded = false;
    m_bMapPageRequested = true;
    m_mapPageName = requestedMap;

    constexpr std::size_t MapPagePathBytes = 260;
    char filename[MapPagePathBytes];
    _snprintf(filename, sizeof(filename), "maps/%s.txt", requestedMap.c_str());
    filename[sizeof(filename) - 1] = 0;
    if (!vgui2::filesystem()->FileExists(filename)) {
        if (!vgui2::filesystem()->FileExists("maps/default.txt")) {
            return;
        }
        _snprintf(filename, sizeof(filename), "maps/default.txt");
    }

    FileHandle_t file = vgui2::filesystem()->Open(filename, "rb", NULL);
    if (!file) {
        return;
    }
    unsigned int size = vgui2::filesystem()->Size(file);
    if (size > static_cast<unsigned int>(INT_MAX)) {
        vgui2::filesystem()->Close(file);
        return;
    }
    std::vector<unsigned char> bytes(size);
    int bytesRead = size == 0 ? 0 : vgui2::filesystem()->Read(bytes.data(), static_cast<int>(size), file);
    vgui2::filesystem()->Close(file);
    if (bytesRead != static_cast<int>(size)) {
        return;
    }

    std::string text;
    if (!cse::resource_text::Decode(bytes.data(), bytes.size(), text)) {
        return;
    }
    m_pMapInfo->SetText(text.c_str());
    m_pMapInfo->GotoTextStart();
    m_bMapPageLoaded = true;
    InvalidateLayout(false, false);
    Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: sets the text on and displays the team buttons
//-----------------------------------------------------------------------------
void CTeamMenu::MakeTeamButtons( const char** names, int numTeams )
{
	m_iNumTeams = numTeams;
	for ( int i = 0; i < m_pTeamButtons.Count(); ++i )
		m_pTeamButtons[i]->SetVisible( false );
	for ( int i = 0; i <= numTeams + 1; ++i )
	{
		char text[32];
		_snprintf( text, sizeof( text ), "&%i %s", i + 1, names[i] );
		text[sizeof( text ) - 1] = 0;
		vgui2::Button* button = m_pTeamButtons[i];
		button->SetText( text );
		button->SetCommand( new KeyValues( "TeamButton", "team", i ) );
		vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
		Color background = scheme->GetColor( "SelectionBG", Color( 255, 255, 255, 0 ) );
		Color foreground = scheme->GetColor( GetStringTeamColor( i ), Color( 255, 255, 255, 255 ) );
		button->SetArmedColor( foreground, background );
		background = scheme->GetColor( "ButtonArmedBgColor", Color( 255, 255, 255, 0 ) );
		foreground = scheme->GetColor( GetStringTeamColor( i ), Color( 255, 255, 255, 255 ) );
		button->SetDepressedColor( foreground, background );
		background = scheme->GetColor( "ButtonDepressedBgColor", Color( 255, 255, 255, 0 ) );
		foreground = scheme->GetColor( GetStringTeamColor( i ), Color( 255, 255, 255, 255 ) );
		button->SetDefaultColor( foreground, background );
		button->SetVisible( true );
	}
}

void CTeamMenu::Update( const char* map, bool, const char** names, int numTeams )
{
	m_pMapName->SetText( map );
	LoadMapPage( map );
	if ( names && *names )
		MakeTeamButtons( names, numTeams );
}

void CTeamMenu::SetLabelText( const char* name, const char* text )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( name, false ) );
	if ( label )
		label->SetText( text );
}
