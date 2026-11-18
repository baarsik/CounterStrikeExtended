/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// death notice
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "death_message_extra.h"
#include "nextclient/hud/radar/integration/RadarHooks.h"
#include "../public/game_controls/iviewport.h"
#include "vgui/IScheme.h"
#include "../../vgui2/controls/controls.h"

#define MAX_DEATHNOTICES 4

struct DeathNoticeItem
{
	char szKiller[64];
	char szVictim[64];
    char szAssistant[64];
    char szRarity[64];
	int iId;	// the index number of the associated sprite
	int iSuicide;
	int iTeamKill;
	int iNonPlayerKill;
	float flDisplayTime;
	float* KillerColor;
	float* VictimColor;
    float* AssistantColor;
	int iHeadShotId;
};

static int DEATHNOTICE_DISPLAY_TIME = 6;
DeathNoticeItem rgDeathNoticeList[MAX_DEATHNOTICES + 1];
float g_ColorBlue[3] = { 0.6, 0.8, 1 };
float g_ColorRed[3] = { 1, 0.25, 0.25 };
float g_ColorGreen[3] = { 0.6, 1, 0.6 };
float g_ColorYellow[3] = { 1, 0.7, 0 };
float g_ColorGrey[3] = { 0.8, 0.8, 0.8 };

DECLARE_MESSAGE( m_DeathNotice, DeathMsg );

float* GetClientColor( int client )
{
	switch ( g_PlayerExtraInfo[client].teamnumber )
	{
	case 1: return g_ColorRed;
	case 2: return g_ColorBlue;
	case 3: return g_ColorYellow;
	case 4: return g_ColorGreen;
	default: return g_ColorGrey;
	}
}

int CHudDeathNotice::Init()
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( DeathMsg );
	CVAR_CREATE( "hud_deathnotice_time", "6", FCVAR_ARCHIVE );
	m_iDeathNoticeGap = vgui2::scheme()->GetProportionalScaledValue( 16 );
	m_iDeathNoticeGapRight = vgui2::scheme()->GetProportionalScaledValue( 12 );
	m_iDeathNoticeTop = vgui2::scheme()->GetProportionalScaledValue( 32 );
	m_iDeathTextGap = vgui2::scheme()->GetProportionalScaledValue( 5 );
	return 1;
}

int CHudDeathNotice::VidInit()
{
	m_HUD_d_skull = gHUD.GetSpriteIndex( "d_skull" );
	return 1;
}

void CHudDeathNotice::InitHUDData()
{
	memset( rgDeathNoticeList, 0, sizeof( rgDeathNoticeList ) );
}

int CHudDeathNotice::GetDeathNoticeY()
{
	if ( g_iUser1 || gEngfuncs.IsSpectateOnly() )
		return gViewPortInterface->GetSpectatorTopBarHeight();
	return m_iDeathNoticeTop;
}

int CHudDeathNotice::Draw( float time )
{
	int top = GetDeathNoticeY();
	if ( gViewPortInterface->IsTutorTextWindowOpen() )
		return 1;
	for ( int i = 0; i < MAX_DEATHNOTICES; i++ )
	{
		DeathNoticeItem& notice = rgDeathNoticeList[i];
		if ( !notice.iId )
			break;
		if ( time > notice.flDisplayTime )
		{
			// remove the current item from the list
			memmove( &notice, &rgDeathNoticeList[i + 1], sizeof( notice ) * ( MAX_DEATHNOTICES - i ) );
			i--;  // continue on the next item;  stop the counter getting incremented
			continue;
		}
		float end = gHUD.m_flTime + DEATHNOTICE_DISPLAY_TIME;
		if ( notice.flDisplayTime > end )
			notice.flDisplayTime = end;
		if ( !gViewPortInterface || !gViewPortInterface->AllowedToPrintText() )
			continue;
		int y = top + i * m_iDeathNoticeGap;
		int id = notice.iId == -1 ? m_HUD_d_skull : notice.iId;
        int victimWidth = 0;
        int textHeight = 0;
        GetConsoleStringSize(notice.szVictim, &victimWidth, &textHeight);
        int x = ScreenWidth - victimWidth - gHUD.GetSpriteWidth(id)
            - m_iDeathNoticeGapRight - m_iDeathTextGap;
		if ( notice.iHeadShotId >= 0 )
			x -= gHUD.GetSpriteWidth( notice.iHeadShotId ) + m_iDeathTextGap;
        char visibleAssistant[sizeof(notice.szAssistant)];
        char visibleRarity[sizeof(notice.szRarity)];
        strcpy(visibleAssistant, notice.szAssistant);
        strcpy(visibleRarity, notice.szRarity);
        const int killerWidth = notice.iSuicide ? 0 : ConsoleStringLen(notice.szKiller) + m_iDeathTextGap;
        int optionalWidth = x - killerWidth;
        if (optionalWidth < 0) {
            optionalWidth = 0;
        }
        if (ConsoleStringLen(visibleAssistant) + ConsoleStringLen(visibleRarity) > optionalWidth) {
            const int rarityBudget = visibleAssistant[0] ? optionalWidth / 2 : optionalWidth;
            while (visibleRarity[0] && ConsoleStringLen(visibleRarity) > rarityBudget) {
                char* lastBadge = strrchr(visibleRarity, ' ');
                if (lastBadge) {
                    *lastBadge = 0;
                } else {
                    visibleRarity[0] = 0;
                }
            }
            const int assistantBudget = optionalWidth - ConsoleStringLen(visibleRarity);
            constexpr unsigned char kUtf8ContinuationMask = 0xc0;
            constexpr unsigned char kUtf8ContinuationValue = 0x80;
            while (visibleAssistant[0] && ConsoleStringLen(visibleAssistant) > assistantBudget) {
                size_t lastCharacter = strlen(visibleAssistant) - 1;
                while (lastCharacter > 0 &&
                       (static_cast<unsigned char>(visibleAssistant[lastCharacter]) & kUtf8ContinuationMask) ==
                           kUtf8ContinuationValue) {
                    --lastCharacter;
                }
                visibleAssistant[lastCharacter] = 0;
            }
            if (strlen(visibleAssistant) <= 3) {
                visibleAssistant[0] = 0;
            }
        }
        x -= ConsoleStringLen(visibleRarity);
		if ( !notice.iSuicide )
		{
			x -= ConsoleStringLen( notice.szKiller ) + m_iDeathTextGap;
            x -= ConsoleStringLen(visibleAssistant);
			if ( notice.KillerColor )
				gEngfuncs.pfnDrawSetTextColor( notice.KillerColor[0], notice.KillerColor[1], notice.KillerColor[2] );
            x = DrawConsoleString(x, y, notice.szKiller);
            if (visibleAssistant[0]) {
                if (notice.AssistantColor) {
                    gEngfuncs.pfnDrawSetTextColor(notice.AssistantColor[0], notice.AssistantColor[1],
                                                  notice.AssistantColor[2]);
                }
                x = DrawConsoleString(x, y, visibleAssistant);
            }
            x += m_iDeathTextGap;
		}
		int r = notice.iTeamKill ? 10 : 255;
		int g = notice.iTeamKill ? 240 : 80;
		int b = notice.iTeamKill ? 10 : 0;

		// Draw death weapon
		SPR_Set( gHUD.GetSprite( id ), r, g, b );
        // Larger console fonts need each sprite centered within the text line.
        constexpr int spriteCenterDivisor = 2;
        int weaponY = y;
        int weaponHeight = gHUD.GetSpriteHeight(id);
        if (textHeight > weaponHeight) {
            weaponY += (textHeight - weaponHeight) / spriteCenterDivisor;
        }
        SPR_DrawAdditive(0, x, weaponY, &gHUD.GetSpriteRect(id));
		x += gHUD.GetSpriteWidth( id ) + m_iDeathTextGap;
		if ( notice.iHeadShotId >= 0 )
		{
			SPR_Set( gHUD.GetSprite( notice.iHeadShotId ), r, g, b );
            int headshotY = y;
            int headshotHeight = gHUD.GetSpriteHeight(notice.iHeadShotId);
            if (textHeight > headshotHeight) {
                headshotY += (textHeight - headshotHeight) / spriteCenterDivisor;
            }
            SPR_DrawAdditive(0, x, headshotY, &gHUD.GetSpriteRect(notice.iHeadShotId));
			x += gHUD.GetSpriteWidth( notice.iHeadShotId ) + m_iDeathTextGap;
		}
        if (visibleRarity[0]) {
            gEngfuncs.pfnDrawSetTextColor(1.0f, 0.7f, 0.0f);
            x = DrawConsoleString(x, y, visibleRarity);
        }
		if ( !notice.iNonPlayerKill )
		{
			if ( notice.VictimColor )
				gEngfuncs.pfnDrawSetTextColor( notice.VictimColor[0], notice.VictimColor[1], notice.VictimColor[2] );
			DrawConsoleString( x, y, notice.szVictim );
		}
	}
	return 1;
}

int CHudDeathNotice::MsgFunc_DeathMsg( const char*, int size, void* buffer )
{
	m_iFlags |= HUD_ACTIVE;
	BEGIN_READ( buffer, size );
	int killer = READ_BYTE();
	int victim = READ_BYTE();
	char killedwith[32] = "d_";
	int headshot = READ_BYTE();
	strncat( killedwith, READ_STRING(), 30 );
	killedwith[31] = 0;
    const DeathMessageExtra extra = ParseDeathMessageExtra(buffer, size > 0 ? static_cast<std::size_t>(size) : 0);
    if (extra.has_position && victim > 0 && victim <= gEngfuncs.GetMaxClients()) {
        RadarHooks_OnPlayerDeath(victim, extra.position);
    }
	if ( gViewPortInterface )
		gViewPortInterface->DeathMsg( killer, victim );
	gHUD.m_Spectator.DeathMessage( victim );
	int index;
	for ( index = 0; index < MAX_DEATHNOTICES && rgDeathNoticeList[index].iId; index++ )
		;
	if ( index == MAX_DEATHNOTICES )
	{
		memmove( rgDeathNoticeList, rgDeathNoticeList + 1, sizeof( DeathNoticeItem ) * MAX_DEATHNOTICES );
		index = MAX_DEATHNOTICES - 1;
	}
	if ( gViewPortInterface )
		gViewPortInterface->GetAllPlayersInfo();
	DeathNoticeItem& notice = rgDeathNoticeList[index];
    // A slot compacted from an older row may still contain optional labels.
    notice.szAssistant[0] = 0;
    notice.szRarity[0] = 0;
    notice.AssistantColor = nullptr;
    if (extra.present && extra.assistant > 0 && extra.assistant <= gEngfuncs.GetMaxClients() &&
        extra.assistant <= MAX_PLAYERS) {
        const char* assistant_name = g_PlayerInfoList[extra.assistant].name;
        if (assistant_name && assistant_name[0]) {
            strcpy(notice.szAssistant, " + ");
            strncat(notice.szAssistant, assistant_name, sizeof(notice.szAssistant) - strlen(notice.szAssistant) - 1);
            notice.AssistantColor = GetClientColor(extra.assistant);
        }
    }
    if (extra.present) {
        FormatDeathRarity(extra.rarity, notice.szRarity, sizeof(notice.szRarity));
    }
	const char* name = g_PlayerInfoList[killer].name;
	if ( name )
	{
		notice.KillerColor = GetClientColor( killer );
		strncpy( notice.szKiller, name, 32 );
		notice.szKiller[31] = 0;
	}
	else
		notice.szKiller[0] = 0;
	if ( (unsigned char)victim == 255 )
	{
		notice.iNonPlayerKill = 1;
		strcpy( notice.szVictim, killedwith + 2 );
	}
	else
	{
		name = g_PlayerInfoList[victim].name;
		if ( name )
		{
			notice.VictimColor = GetClientColor( victim );
			strncpy( notice.szVictim, name, 32 );
			notice.szVictim[31] = 0;
		}
		else
			notice.szVictim[0] = 0;
		if ( killer == victim || !killer )
			notice.iSuicide = 1;
		if ( !strcmp( killedwith, "d_teammate" ) )
			notice.iTeamKill = 1;
	}
	notice.iId = gHUD.GetSpriteIndex( killedwith );
	notice.iHeadShotId = headshot ? gHUD.GetSpriteIndex( "d_headshot" ) : -1;
	DEATHNOTICE_DISPLAY_TIME = (int)CVAR_GET_FLOAT( "hud_deathnotice_time" );
	notice.flDisplayTime = DEATHNOTICE_DISPLAY_TIME + gHUD.m_flTime;
	if ( notice.iNonPlayerKill )
	{
		ConsolePrint( notice.szKiller );
		ConsolePrint( " killed a " );
		ConsolePrint( notice.szVictim );
		ConsolePrint( "\n" );
		return 1;
	}
	if ( notice.iSuicide )
	{
		ConsolePrint( notice.szVictim );
		ConsolePrint( !strcmp( killedwith, "d_world" ) ? " died" : " killed self" );
	}
	else if ( notice.iTeamKill )
	{
		ConsolePrint( notice.szKiller );
		ConsolePrint( notice.szAssistant );
		ConsolePrint( " killed his teammate " );
		ConsolePrint( notice.szVictim );
	}
	else
	{
		if ( headshot )
			ConsolePrint( "*** " );
		ConsolePrint( notice.szKiller );
		ConsolePrint( notice.szAssistant );
		ConsolePrint( " killed " );
		ConsolePrint( notice.szVictim );
	}
	if ( killedwith[0] > 13 && strcmp( killedwith, "d_world" ) && !notice.iTeamKill )
	{
		ConsolePrint( headshot ? " with a headshot from " : " with " );

		// replace the code names with the 'real' names
		if ( !strcmp( killedwith + 2, "egon" ) )
			strcpy( killedwith, "d_gluon gun" );
		if ( !strcmp( killedwith + 2, "gauss" ) )
			strcpy( killedwith, "d_tau cannon" );
		ConsolePrint( killedwith + 2 ); // skip over the "d_" part
	}
	if ( headshot )
		ConsolePrint( " ***" );
    if (notice.szRarity[0]) {
        ConsolePrint(notice.szRarity);
    }
	ConsolePrint( "\n" );
	return 1;
}
