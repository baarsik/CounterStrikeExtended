#pragma once

class CBasePlayer;
class CBasePlayerItem;

namespace cse::weapons::server {

void Initialize();
void StageConfiguredCatalog();
void Activate();
void Deactivate();
void RoundCleanup();
void LinkMessages();

bool AcceptClient(edict_t* client, char* rejectReason);
void Connect(CBasePlayer& player);
void Disconnect(CBasePlayer& player);
bool ClientCommand(CBasePlayer& player, const char* command);
void UpdateClient(CBasePlayer& player);
void ClearAmmo(CBasePlayer& player);

bool IsCustom(const CBasePlayerItem* item);
int Weight(CBasePlayerItem* item);
bool BlocksStockPickup(const CBasePlayer& player, int slot);
bool BlocksStockPurchase(const CBasePlayer& player, bool primary);
bool Drop(CBasePlayer& player, CBasePlayerItem* item, bool death, bool packAmmo);
const char* DamageWeapon(entvars_t* attacker, entvars_t* inflictor);
bool DeathNotice(CBasePlayer& victim, entvars_t* killer, entvars_t* inflictor, int killerIndex, bool headshot, const char*& logWeapon);

} // namespace cse::weapons::server
