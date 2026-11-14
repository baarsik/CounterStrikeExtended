//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <algorithm>
#include <cwchar>
#include "extended_stats.h"
#include "hud.h"
#include "cl_util.h"
#include "cstrikespectatorgui.h"
#include "icstrikeviewport.h"
#include "game_controls/iviewport.h"
#include "hud_iga.h"
#include "Label.h"
#include "Controls.h"
#include "ComboBox.h"
#include "vgui/ILocalize.h"
#include <stdio.h>
#include <wchar.h>

bool IsBombPlanted();

CCSSpectatorGUI::CCSSpectatorGUI( vgui2::Panel* parent ) : CSpectatorGUI( parent )
{
	m_pCTLabel = dynamic_cast<vgui2::Label*>( FindChildByName( "CTScoreLabel", false ) );
	m_pCTScore = dynamic_cast<vgui2::Label*>( FindChildByName( "CTScoreValue", false ) );
	m_pTerLabel = dynamic_cast<vgui2::Label*>( FindChildByName( "TerScoreLabel", false ) );
	m_pTerScore = dynamic_cast<vgui2::Label*>( FindChildByName( "TerScoreValue", false ) );
	m_pTimer = FindChildByName( "timerimage", false );
	m_pTimerLabel = dynamic_cast<vgui2::Label*>( FindChildByName( "timerlabel", false ) );
	m_pDivider = FindChildByName( "DividerBar", false );
	m_pExtraInfo = dynamic_cast<vgui2::Label*>( FindChildByName( "extrainfo", false ) );
	m_modifiedWidths = false;
	m_scoreWidth = 0;
	m_extraInfoWidth = 0;
	m_pIGA = NULL;
	m_pTopBar = FindChildByName( "topbar", false );
	m_pBanner = m_pTopBar->FindChildByName( "banner", false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSSpectatorGUI::~CCSSpectatorGUI()
{
}

bool CCSSpectatorGUI::CanSpectateTeam( int team )
{
	int forceCamera = gCSViewPortInterface->GetForceCamera();
	return g_iTeamNumber == 0 || gEngfuncs.IsSpectateOnly() || forceCamera == 0 || team == g_iTeamNumber;
}

bool CCSSpectatorGUI::ControlsOK()
{
	return m_pCTLabel && m_pCTScore && m_pTerLabel && m_pTerScore && m_pTimer &&
		m_pTimerLabel && m_pDivider && m_pExtraInfo;
}

void CCSSpectatorGUI::StoreWidths()
{
	if ( ControlsOK() && !m_modifiedWidths )
	{
		m_scoreWidth = m_pCTScore->GetWide();
		int terroristWidth = m_pTerScore->GetWide();
		m_extraInfoWidth = m_pExtraInfo->GetWide();
		if ( terroristWidth != m_scoreWidth )
			m_pTerScore = NULL;
	}
}

void CCSSpectatorGUI::LayoutAd()
{
	if ( !m_pIGA )
		return;

	bool shown = true;
	if ( m_pTopBar && g_pClientIGA->IsPanelReady( IGA_SPECTATOR, &shown ) )
	{
		if ( shown )
		{
			int wide, tall;
			m_pTopBar->GetSize( wide, tall );
			wide = (int)( wide * 0.6f );
			m_pIGA->SetBounds( 5, 5, ( tall - 10 ) * 4, tall - 10 );
		}
		else
		{
			m_pIGA->SetBounds( 0, 0, 1, 1 );
		}
		m_pIGA->SetVisible( true );
		m_pBanner->SetVisible( false );
	}
	else
	{
		if ( m_pIGA )
			m_pIGA->SetVisible( false );
		m_pBanner->SetVisible( true );
	}
}

void CCSSpectatorGUI::SizeControls()
{
	LayoutAd();
	StoreWidths();
	if ( !ControlsOK() )
		return;

	int x, y, width, height, ctWidth, ctHeight, terWidth, terHeight;
	m_pCTScore->GetBounds( x, y, width, height );
	m_pCTScore->GetContentSize( ctWidth, ctHeight );
	int terX, terY, terWide, terTall;
	m_pTerScore->GetBounds( terX, terY, terWide, terTall );
	m_pTerScore->GetContentSize( terWidth, terHeight );
	int scoreWidth = m_scoreWidth;
	if ( ctWidth >= scoreWidth )
		scoreWidth = ctWidth;
	if ( terWidth >= scoreWidth )
		scoreWidth = terWidth;
	int delta = scoreWidth - width;
	if ( delta )
	{
		m_pCTScore->GetBounds( x, y, width, height );
		m_pCTScore->SetBounds( x - delta, y, width + delta, height );
		m_pTerScore->GetBounds( x, y, width, height );
		m_pTerScore->SetBounds( x - delta, y, width + delta, height );
		m_pCTLabel->GetPos( x, y );
		m_pCTLabel->SetPos( x - delta, y );
		m_pTerLabel->GetPos( x, y );
		m_pTerLabel->SetPos( x - delta, y );
		m_modifiedWidths = true;
	}

	int extraWidth, extraHeight;
	m_pExtraInfo->GetBounds( x, y, width, height );
	m_pExtraInfo->GetContentSize( extraWidth, extraHeight );
	if ( extraWidth < m_extraInfoWidth )
		extraWidth = m_extraInfoWidth;
	delta = extraWidth - width;
	if ( delta )
	{
		m_pExtraInfo->GetBounds( x, y, width, height );
		m_pExtraInfo->SetBounds( x - delta, y, width + delta, height );
		m_pTimer->GetPos( x, y );
		m_pTimer->SetPos( x - delta, y );
		m_pTimerLabel->GetPos( x, y );
		m_pTimerLabel->SetPos( x - delta, y );
		m_pDivider->GetPos( x, y );
		m_pDivider->SetPos( x - delta, y );
		m_pCTScore->GetPos( x, y );
		m_pCTScore->SetPos( x - delta, y );
		m_pCTLabel->GetPos( x, y );
		m_pCTLabel->SetPos( x - delta, y );
		m_pTerScore->GetPos( x, y );
		m_pTerScore->SetPos( x - delta, y );
		m_pTerLabel->GetPos( x, y );
		m_pTerLabel->SetPos( x - delta, y );
		m_modifiedWidths = true;
	}
}

void CCSSpectatorGUI::UpdateTimer()
{
	if ( IsBombPlanted() )
	{
		SetLabelText( "timerlabel", "" );
	}
	else
	{
		int seconds = (int)( gHUD.m_roundTimer.m_flTimeEnd - gHUD.m_flTime );
		if ( seconds < 0 )
			seconds = 0;
		wchar_t text[64];
		_snwprintf( text, 63, L"%d:%02d", seconds / 60, seconds % 60 );
		text[62] = 0;
		SetLabelText( "timerlabel", text );
		LayoutAd();
	}
}



//-----------------------------------------------------------------------------
// Purpose: Resets the list of players
//-----------------------------------------------------------------------------
void CCSSpectatorGUI::UpdateSpectatorPlayerList()
{
	CSpectatorGUI::UpdateSpectatorPlayerList();
	bool foundCT = false;
	bool foundTerrorist = false;
	for ( int i = 0; i < 64; ++i )
	{
		VGuiLibraryTeamInfo_t team = gViewPortInterface->GetPlayerTeamInfo( i );
		if ( team.name )
		{
			wchar_t frags[10];
			if ( !stricmp( team.name, "TERRORIST" ) )
			{
				_snwprintf( frags, 10, L"%i", team.frags );
				SetLabelText( "TERScoreValue", frags );
				foundTerrorist = true;
			}
			else if ( !stricmp( team.name, "CT" ) )
			{
				_snwprintf( frags, 10, L"%i", team.frags );
				SetLabelText( "CTScoreValue", frags );
				foundCT = true;
			}
			if ( foundTerrorist == true && foundCT == true )
				break;
		}
	}
	if ( foundTerrorist && foundCT )
		SizeControls();
}

void CCSSpectatorGUI::Update( const char* serverName, int player, int mode,
	bool spectateOnly, int spectatorCount, const char* mapName )
{
	CSpectatorGUI::Update( serverName, player, mode, spectateOnly, spectatorCount, mapName );
	if ( g_iTeamNumber == 1 || g_iTeamNumber == 2 )
	{
		wchar_t text[64];
		_snwprintf( text, 63, L"$%i", gHUD.m_accountBalance.GetBalance() );
		text[62] = 0;
		SetLabelText( "extrainfo", text );
	}
	SizeControls();
}

void CCSSpectatorGUI::GetPlayerLabelText(int player, wchar_t* text, int textLength) {
    using namespace cse::extstats;
    m_statsArmor = {};
    if (!text || textLength < static_cast<int>(sizeof(wchar_t))) {
        return;
    }
    CSpectatorGUI::GetPlayerLabelText(player, text, textLength);
    const Value armor = ClientStats().Get(Stat::Armor, player);
    if (!armor.valid) {
        return;
    }
    const VGuiLibraryPlayer_t info = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo(player);
    if (!info.name) {
        return;
    }
    constexpr int SafeNameBytes = 256;
    char safeName[SafeNameBytes] = {};
    gViewPortInterface->MakeSafeName(info.name, safeName, sizeof(safeName));
    vgui2::localize()->ConvertANSIToUnicode(safeName, m_statsName, sizeof(m_statsName));
    m_statsName[StatsNameCharacters - 1] = 0;
    m_statsHealth = ClientStats().Get(Stat::Health, player);
    m_statsArmor = armor;
    FormatStatsLabel(text, textLength);
}

void CCSSpectatorGUI::FormatStatsLabel(wchar_t* text, int textLength) const {
    using namespace cse::extstats;
    const int capacity = textLength / static_cast<int>(sizeof(wchar_t));
    if (!text || capacity < 1) {
        return;
    }
    const Value health = m_statsHealth;
    const Value armor = m_statsArmor;
    constexpr int NameCharacters = StatsNameCharacters;
    constexpr int NumberCharacters = 16;
    constexpr int FormattedCharacters = 256;
    wchar_t name[NameCharacters] = {};
    wchar_t healthText[NumberCharacters] = {};
    wchar_t armorText[NumberCharacters] = {};
    wchar_t formatted[FormattedCharacters] = {};
    std::wcsncpy(name, m_statsName, StatsNameCharacters - 1);
    std::swprintf(healthText, NumberCharacters, L"%d", DisplayAmount(health.amount));
    std::swprintf(armorText, NumberCharacters, L"%d", DisplayAmount(armor.amount));
    const char* token = health.valid ? "#CSEStats_SpectatorHealthArmor" : "#CSEStats_SpectatorArmor";
    wchar_t fallbackHealthArmor[] = L"%s1 | HP %s2 | AP %s3";
    wchar_t fallbackArmor[] = L"%s1 | AP %s2";
    wchar_t* pattern = vgui2::localize()->Find(token);
    if (!pattern) {
        pattern = health.valid ? fallbackHealthArmor : fallbackArmor;
    }
    auto format = [&]() {
        if (health.valid) {
            vgui2::localize()->ConstructString(formatted, sizeof(formatted), pattern, 3,
                name, healthText, armorText);
        } else {
            vgui2::localize()->ConstructString(formatted, sizeof(formatted), pattern, 2, name, armorText);
        }
    };
    format();
    const int length = static_cast<int>(std::wcslen(formatted));
    if (length >= capacity) {
        const int nameLength = static_cast<int>(std::wcslen(name));
        const int shorten = (std::min)(nameLength, length - capacity + 1);
        name[nameLength - shorten] = 0;
        format();
    }
    std::wcsncpy(text, formatted, capacity - 1);
    text[capacity - 1] = 0;
}


void CCSSpectatorGUI::RefreshLanguage() {
    CSpectatorGUI::RefreshLanguage();
    if (!m_statsArmor.valid) {
        return;
    }
    // Replay only owned presentation inputs, including after the entity has disconnected.
    wchar_t text[80] = {};
    FormatStatsLabel(text, sizeof(text));
    SetLabelText("playerlabel", text);
    vgui2::ComboBox* combo = dynamic_cast<vgui2::ComboBox*>(FindChildByName("playercombo", true));
    if (combo) {
        combo->TextEntry::SetText(text);
    }
}
