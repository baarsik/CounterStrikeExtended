//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "voice_banmgr.h"
#include "../../common/voice_ban_file.h"

#include <stdio.h>
#include <string.h>


const char* g_pBanMgrFilename = "voice_ban.dt";

unsigned char HashPlayerID( const char playerID[16] )
{
	unsigned char hash = 0;
	for ( int i = 0; i < 16; ++i )
		hash += (unsigned char)playerID[i];
	return hash;
}

CVoiceBanMgr::CVoiceBanMgr()
{
	Clear();
}

CVoiceBanMgr::~CVoiceBanMgr()
{
	Term();
}

bool CVoiceBanMgr::Init(char const* gameDirectory) {
    static_assert(sizeof(m_PlayerHash[0].m_PlayerID) == cse_voice_bans::PlayerIdBytes,
        "Persistent player ID width");
    Term();
    constexpr std::size_t FilenameCapacity = 512;
    char filename[FilenameCapacity];
#if defined(_WIN32)
    _snprintf(filename, sizeof(filename), "%s/%s", gameDirectory, g_pBanMgrFilename);
#else
    snprintf(filename, sizeof(filename), "%s/%s", gameDirectory, g_pBanMgrFilename);
#endif
    filename[sizeof(filename) - 1] = 0;
    FILE* file = fopen(filename, "rb");
    if (file) {
        cse_voice_bans::ReadLegacy(file, [this](const char* playerId) {
            AddBannedPlayer(playerId);
        });
        fclose(file);
    }
    return true;
}


void CVoiceBanMgr::Term()
{
	// Free all the player structures.
	for ( int i = 0; i < 256; ++i )
	{
		BannedPlayer* head = &m_PlayerHash[i];
		BannedPlayer* player = head->m_pNext;
		while ( player != head )
		{
			BannedPlayer* next = player->m_pNext;
			delete player;
			player = next;
		}
	}
	Clear();
}

void CVoiceBanMgr::SaveState( const char* gameDirectory )
{
	char filename[512];
#ifdef _WIN32
	_snprintf( filename, sizeof( filename ), "%s/%s", gameDirectory, g_pBanMgrFilename );
#else
	snprintf( filename, sizeof( filename ), "%s/%s", gameDirectory, g_pBanMgrFilename );
#endif
	filename[sizeof( filename ) - 1] = 0;
	FILE* file = fopen( filename, "wb" );
	if ( file )
	{
        cse_voice_bans::WriteVersion(file);
		for ( int i = 0; i < 256; ++i )
		{
			BannedPlayer* head = &m_PlayerHash[i];
			for ( BannedPlayer* player = head->m_pNext; player != head; player = player->m_pNext )
				fwrite( player->m_PlayerID, 1, sizeof( player->m_PlayerID ), file );
		}
		fclose( file );
	}
}

bool CVoiceBanMgr::GetPlayerBan( const char playerID[16] )
{
	return InternalFindPlayerSquelch( playerID ) != NULL;
}

void CVoiceBanMgr::SetPlayerBan( const char playerID[16], bool squelch )
{
	if ( squelch )
	{
		if ( !GetPlayerBan( playerID ) )
			AddBannedPlayer( playerID );
	}
	else
	{
		BannedPlayer* player = InternalFindPlayerSquelch( playerID );
		if ( player )
		{
			player->m_pPrev->m_pNext = player->m_pNext;
			player->m_pNext->m_pPrev = player->m_pPrev;
			delete player;
		}
	}
}

void CVoiceBanMgr::ForEachBannedPlayer( void ( *callback )( char playerID[16] ) )
{
	for ( int i = 0; i < 256; ++i )
	{
		BannedPlayer* head = &m_PlayerHash[i];
		for ( BannedPlayer* player = head->m_pNext; player != head; player = player->m_pNext )
			callback( player->m_PlayerID );
	}
}

void CVoiceBanMgr::Clear()
{
	// Tie off the hash table entries.
	for ( int i = 0; i < 256; ++i )
		m_PlayerHash[i].m_pNext = m_PlayerHash[i].m_pPrev = &m_PlayerHash[i];
}

CVoiceBanMgr::BannedPlayer* CVoiceBanMgr::InternalFindPlayerSquelch( const char playerID[16] )
{
	BannedPlayer* head = &m_PlayerHash[HashPlayerID( playerID )];
	for ( BannedPlayer* player = head->m_pNext; player != head; player = player->m_pNext )
	{
		if ( memcmp( playerID, player->m_PlayerID, 16 ) == 0 )
			return player;
	}
	return NULL;
}

CVoiceBanMgr::BannedPlayer* CVoiceBanMgr::AddBannedPlayer( const char playerID[16] )
{
	BannedPlayer* player = new BannedPlayer;
	if ( !player )
		return NULL;
	BannedPlayer* head = &m_PlayerHash[HashPlayerID( playerID )];
	memcpy( player->m_PlayerID, playerID, 16 );
	player->m_pNext = head;
	player->m_pPrev = head->m_pPrev;
	player->m_pPrev->m_pNext = head->m_pPrev = player;
	return player;
}
