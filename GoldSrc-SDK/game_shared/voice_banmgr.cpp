//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <string.h>
#include <stdio.h>
#include "voice_banmgr.h"
#include "../common/voice_ban_file.h"


char const *g_pBanMgrFilename = "voice_ban.dt";



// Hash a player ID to a byte.
unsigned char HashPlayerID(char const playerID[16])
{
	unsigned char curHash = 0;

	for(int i=0; i < 16; i++)
		curHash += (unsigned char)playerID[i];

	return curHash;
}



CVoiceBanMgr::CVoiceBanMgr()
{
	Clear();
}


CVoiceBanMgr::~CVoiceBanMgr()
{
	Term();
}


bool CVoiceBanMgr::Init(char const* pGameDir) {
    static_assert(sizeof(m_PlayerHash[0].m_PlayerID) == cse_voice_bans::PlayerIdBytes,
        "Persistent player ID width");
    Term();
    constexpr std::size_t FilenameCapacity = 512;
    char filename[FilenameCapacity];
#if defined(_WIN32)
    _snprintf(filename, sizeof(filename), "%s/%s", pGameDir, g_pBanMgrFilename);
#else
    snprintf(filename, sizeof(filename), "%s/%s", pGameDir, g_pBanMgrFilename);
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
	for(int i=0; i < 256; i++)
	{
		BannedPlayer *pListHead = &m_PlayerHash[i];
		BannedPlayer *pNext;
		for(BannedPlayer *pCur=pListHead->m_pNext; pCur != pListHead; pCur=pNext)
		{
			pNext = pCur->m_pNext;
			delete pCur;
		}
	}

	Clear();
}


void CVoiceBanMgr::SaveState(char const *pGameDir)
{
	// Save the file out.
	char filename[512];
	_snprintf(filename, sizeof(filename), "%s/%s", pGameDir, g_pBanMgrFilename);
	filename[sizeof( filename ) - 1] = 0;

	FILE *fp = fopen(filename, "wb");
	if(fp)
	{
        cse_voice_bans::WriteVersion(fp);

		for(int i=0; i < 256; i++)
		{
			BannedPlayer *pListHead = &m_PlayerHash[i];
			for(BannedPlayer *pCur=pListHead->m_pNext; pCur != pListHead; pCur=pCur->m_pNext)
			{
				fwrite(pCur->m_PlayerID, 1, 16, fp);
			}
		}

		fclose(fp);
	}
}


bool CVoiceBanMgr::GetPlayerBan(char const playerID[16])
{
	return !!InternalFindPlayerSquelch(playerID);
}


void CVoiceBanMgr::SetPlayerBan(char const playerID[16], bool bSquelch)
{
	if(bSquelch)
	{
		// Is this guy already squelched?
		if(GetPlayerBan(playerID))
			return;
	
		AddBannedPlayer(playerID);
	}
	else
	{
		BannedPlayer *pPlayer = InternalFindPlayerSquelch(playerID);
		if(pPlayer)
		{
			pPlayer->m_pPrev->m_pNext = pPlayer->m_pNext;
			pPlayer->m_pNext->m_pPrev = pPlayer->m_pPrev;
			delete pPlayer;
		}
	}
}


void CVoiceBanMgr::ForEachBannedPlayer(void (*callback)(char id[16]))
{
	for(int i=0; i < 256; i++)
	{
		for(BannedPlayer *pCur=m_PlayerHash[i].m_pNext; pCur != &m_PlayerHash[i]; pCur=pCur->m_pNext)
		{
			callback(pCur->m_PlayerID);
		}
	}
}


void CVoiceBanMgr::Clear()
{
	// Tie off the hash table entries.
	for(int i=0; i < 256; i++)
		m_PlayerHash[i].m_pNext = m_PlayerHash[i].m_pPrev = &m_PlayerHash[i];
}


CVoiceBanMgr::BannedPlayer* CVoiceBanMgr::InternalFindPlayerSquelch(char const playerID[16])
{
	int index = HashPlayerID(playerID);
	BannedPlayer *pListHead = &m_PlayerHash[index];
	for(BannedPlayer *pCur=pListHead->m_pNext; pCur != pListHead; pCur=pCur->m_pNext)
	{
		if(memcmp(playerID, pCur->m_PlayerID, 16) == 0)
			return pCur;
	}

	return NULL;
}


CVoiceBanMgr::BannedPlayer* CVoiceBanMgr::AddBannedPlayer(char const playerID[16])
{
	BannedPlayer *pNew = new BannedPlayer;
	if(!pNew)
		return NULL;

	int index = HashPlayerID(playerID);
	memcpy(pNew->m_PlayerID, playerID, 16);
	pNew->m_pNext = &m_PlayerHash[index];
	pNew->m_pPrev = m_PlayerHash[index].m_pPrev;
	pNew->m_pPrev->m_pNext = pNew->m_pNext->m_pPrev = pNew;
	return pNew;
}
