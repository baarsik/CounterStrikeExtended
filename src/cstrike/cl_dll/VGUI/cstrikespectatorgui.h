//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CSTRIKESPECTATORGUI_H
#define CSTRIKESPECTATORGUI_H

#include "game_controls/SpectatorGUI.h"
#include "cse_extended_stats_protocol.h"

//-----------------------------------------------------------------------------
// Purpose: Cstrike Spectator UI
//-----------------------------------------------------------------------------
class CCSSpectatorGUI : public CSpectatorGUI
{
public:
	CCSSpectatorGUI( vgui2::Panel* parent );
	~CCSSpectatorGUI() override;
    void GetPlayerLabelText(int player, wchar_t* text, int textLength) override;
    void RefreshLanguage();
	void UpdateSpectatorPlayerList() override;
	void UpdateTimer() override;
	void Update( const char* serverName, int player, int mode, bool spectateOnly,
		int spectatorCount, const char* mapName ) override;
	bool CanSpectateTeam( int team ) override;

private:
    void FormatStatsLabel(wchar_t* text, int textLength) const;
    static constexpr int StatsNameCharacters = 64;
    wchar_t m_statsName[StatsNameCharacters] = {};
    cse::extstats::Value m_statsHealth;
    cse::extstats::Value m_statsArmor;

	void LayoutAd();
	void StoreWidths();
	void SizeControls();
	bool ControlsOK();

	vgui2::Label* m_pCTLabel;
	vgui2::Label* m_pCTScore;
	vgui2::Label* m_pTerLabel;
	vgui2::Label* m_pTerScore;
	vgui2::Panel* m_pTimer;
	vgui2::Label* m_pTimerLabel;
	vgui2::Panel* m_pDivider;
	vgui2::Label* m_pExtraInfo;
	bool m_modifiedWidths;
	int m_scoreWidth;
	int m_extraInfoWidth;
	vgui2::Panel* m_pTopBar;
	vgui2::Panel* m_pBanner;
	vgui2::Panel* m_pIGA;
};

#endif
