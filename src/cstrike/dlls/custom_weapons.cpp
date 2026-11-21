#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "studio.h"
#include "engine_studio.h"
#include "cse_custom_weapons.h"
#include "custom_weapons.h"
#include "pm_shared.h"

extern int gmsgDeathMsg;

namespace cse::weapons::server {
namespace {

constexpr int PrimarySlot = 1;
constexpr int SecondarySlot = 2;
constexpr int DefaultFov = 90;
constexpr float ProjectileLifetime = 6.0f;
constexpr float ProjectileImpactOffset = 1.0f;
constexpr size_t WeaponPrefixLength = sizeof("weapon_") - 1;
constexpr float DropLifetime = 300.0f;
constexpr float DropOwnerDelay = 1.0f;
constexpr float CatalogRetryDelay = 1.0f;
constexpr float CatalogPacketInterval = 0.05f;
constexpr unsigned MaxCatalogAttempts = 4;
constexpr size_t MaxNewModelResources = 24;
constexpr size_t MaxNewSoundResources = 64;
constexpr float ThinSnapshotDelay = 0.05f;
constexpr size_t MaxEventQueue = 1024;
constexpr size_t MaxPurchaseTransactions = 1024;
constexpr int MaxDefinitionBytes = 16 * 1024;
constexpr char FactoryName[] = "cse_custom_weapon";
constexpr char ProjectileClassname[] = "cse_custom_projectile";

char catalogName[] = "cse_weapon_catalog";
char noCatalog[] = "";
char requireClientName[] = "cse_weapon_require_client";
char sharedSlotsName[] = "cse_weapon_shared_slots";
char disabled[] = "0";

cvar_t catalogPath = {catalogName, noCatalog, FCVAR_SERVER, 0.0f, nullptr};
cvar_t requireClient = {requireClientName, disabled, FCVAR_SERVER, 0.0f, nullptr};
cvar_t sharedSlots = {sharedSlotsName, disabled, FCVAR_SERVER, 0.0f, nullptr};

struct AmmoPool {
    std::string key;
    uint16_t capacity;
    int stockIndex = -1;
    size_t privateIndex = 0;
};

struct Entry {
    Definition definition{};
    std::string source;
    Result status = Result::Pending;
    const AmmoPool* primary = nullptr;
    const AmmoPool* secondary = nullptr;
    int viewSequences = 0;
};

struct Purchase {
    uint32_t transaction;
    uint32_t instance;
    uint16_t definition;
    SlotPolicy policy;
};

struct Client {
    EHANDLE entity{};
    Readiness readiness = Readiness::Unsupported;
    std::array<int, MaxAmmoTypes> ammo{};
    State lastState{};
    uint32_t sequence = 0;
    uint16_t pickup = 0;
    float nextCatalog = 0;
    float nextResync = 0;
    float nextState = 0;
    size_t catalogCursor = 0;
    unsigned catalogAttempts = 0;
    bool catalogStarted = false;
    bool catalogSent = false;
    bool resyncRequested = false;
    bool legacyCustom = false;
    std::vector<Purchase> purchases;
};

struct AttackIdentity {
    entvars_t* attacker = nullptr;
    entvars_t* inflictor = nullptr;
    uint16_t definition = 0;
    uint32_t instance = 0;
    uint16_t clip = 0;
    uint16_t reserve = 0;
    uint16_t secondary = 0;
};

class AttackContext {
public:
    explicit AttackContext(const AttackIdentity& attack);
    ~AttackContext();

private:
    AttackIdentity previous;
};

std::array<Client, MaxPlayers + 1> clients;
std::deque<Entry> catalog;
std::deque<AmmoPool> pools;
std::vector<std::string> acceptedModels;
std::vector<std::string> acceptedSounds;
std::vector<EHANDLE> worldItems;
std::vector<EHANDLE> projectiles;
std::deque<Event> events;
AttackIdentity currentAttack;
bool catalogOpen = true;
bool mapActive = false;
uint32_t generation = 0;
uint32_t digest = InitialDigest;
uint32_t nextInstance = 0;
uint32_t killSequence = 0;
uint16_t committedDefinitions = 0;
int messageId = 0;

AttackContext::AttackContext(const AttackIdentity& attack) : previous(currentAttack) {
    currentAttack = attack;
}

AttackContext::~AttackContext() {
    currentAttack = previous;
}

float Seconds(uint16_t milliseconds) {
    constexpr float MillisecondsToSeconds = 0.001f;
    return milliseconds * MillisecondsToSeconds;
}

CBasePlayer* Player(int index) {
    if (index < 1 || index > gpGlobals->maxClients || index > MaxPlayers) {
        return nullptr;
    }
    return dynamic_cast<CBasePlayer*>(UTIL_PlayerByIndex(index));
}

Client* Session(CBasePlayer& player) {
    const int index = player.entindex();
    if (index < 1 || index > MaxPlayers) {
        return nullptr;
    }
    Client& client = clients[static_cast<size_t>(index)];
    return client.entity.Get() == player.edict() ? &client : nullptr;
}

bool Supports(edict_t* client) {
    char* userinfo = g_engfuncs.pfnGetInfoKeyBuffer(client);
    const char* value = userinfo ? g_engfuncs.pfnInfoKeyValue(userinfo, const_cast<char*>(CapabilityKey)) : nullptr;
    return value && !std::strcmp(value, CapabilityVersion);
}

bool IsReady(CBasePlayer& player) {
    Client* client = Session(player);
    return client && client->readiness == Readiness::Ready;
}

Entry* FindEntry(const char* key) {
    if (!key) {
        return nullptr;
    }
    for (Entry& entry : catalog) {
        if (!std::strcmp(entry.definition.presentation.key, key)) {
            return &entry;
        }
    }
    return nullptr;
}

const Entry* FindEntry(uint16_t id) {
    for (const Entry& entry : catalog) {
        if (entry.status == Result::Ok && entry.definition.presentation.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

void Send(CBasePlayer& player, const Packet& packet) {
    if (!messageId || packet.size <= 0 || packet.size > MaxPacketBytes) {
        return;
    }
    MESSAGE_BEGIN(MSG_ONE, messageId, nullptr, player.edict());
    for (int i = 0; i < packet.size; ++i) {
        WRITE_BYTE(packet.bytes[i]);
    }
    MESSAGE_END();
}

int& Reserve(CBasePlayer& owner, const AmmoPool& pool) {
    if (pool.stockIndex >= 0) {
        return owner.m_rgAmmo[pool.stockIndex];
    }
    return Session(owner)->ammo[pool.privateIndex];
}

uint16_t ReserveAmount(CBasePlayer* owner, const AmmoPool* pool) {
    if (!owner || !pool || !Session(*owner)) {
        return 0;
    }
    return static_cast<uint16_t>((std::clamp)(Reserve(*owner, *pool), 0, static_cast<int>(pool->capacity)));
}

void AddReserve(CBasePlayer& owner, const AmmoPool* pool, uint16_t count) {
    if (!pool || !count) {
        return;
    }
    int& reserve = Reserve(owner, *pool);
    reserve = (std::min)(reserve + count, static_cast<int>(pool->capacity));
    if (pool->stockIndex >= 0) {
        owner.TabulateAmmo();
    }
}

void Emit(const AttackIdentity& attack, EventKind kind, int target = 0) {
    if (events.size() == MaxEventQueue) {
        events.pop_front();
    }
    const int player = attack.attacker ? ENTINDEX(ENT(attack.attacker)) : 0;
    events.push_back(Event{generation, attack.instance, attack.definition, attack.clip, attack.reserve,
        attack.secondary, static_cast<uint8_t>(player), static_cast<uint8_t>(target), kind});
}

bool AssetExists(const std::string& path) {
    return g_engfuncs.pfnGetFileSize(const_cast<char*>(path.c_str())) > 0;
}

bool ReadFile(const char* path, int limit, std::string& contents) {
    if (!path || !AssetExists(path)) {
        return false;
    }
    const int fileSize = g_engfuncs.pfnGetFileSize(const_cast<char*>(path));
    if (fileSize <= 0 || fileSize > limit) {
        return false;
    }
    int length = 0;
    byte* data = LOAD_FILE_FOR_ME(const_cast<char*>(path), &length);
    if (!data) {
        return false;
    }
    if (length > 0 && length <= limit) {
        contents.assign(reinterpret_cast<const char*>(data), static_cast<size_t>(length));
    }
    FREE_FILE(data);
    return !contents.empty();
}

bool IsConfigPath(std::string_view path, std::string_view extension) {
    constexpr size_t MaxConfigPath = 255;
    if (path.empty() || path.size() > MaxConfigPath) {
        return false;
    }
    const size_t last = path.find_last_of('/');
    if (last != std::string_view::npos) {
        for (size_t start = 0; start <= last;) {
            const size_t end = path.find('/', start);
            const auto name = path.substr(start, end - start);
            if (name.empty() || !std::all_of(name.begin(), name.end(), [](char ch) {
                    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
                })) {
                return false;
            }
            start = end + 1;
        }
    }
    return IsAssetPath(path.substr(last == std::string_view::npos ? 0 : last + 1), "", extension);
}

bool StockName(const char* key) {
    for (int i = 1; i <= WEAPON_P90; ++i) {
        const WeaponInfoStruct* info = GetWeaponInfo(i);
        if (info && info->entityName && !std::strcmp(info->entityName, key)) {
            return true;
        }
    }
    // These stock entities have no weapon purchase metadata.
    constexpr const char* StockEquipment[] = {
        "weapon_c4", "weapon_flashbang", "weapon_hegrenade", "weapon_knife", "weapon_shield", "weapon_smokegrenade",
    };
    for (const char* name : StockEquipment) {
        if (!std::strcmp(key, name)) {
            return true;
        }
    }
    WeaponIdType alias = WEAPON_NONE;
    return AliasToWeaponID(key + WeaponPrefixLength) != WEAPON_NONE ||
        BuyAliasToWeaponID(key + WeaponPrefixLength, &alias) != nullptr;
}

Result RegisterFile(const char* path) {
    if (!catalogOpen) {
        return Result::WrongPhase;
    }
    if (!path || !IsConfigPath(path, ".cfg")) {
        ALERT(at_console, "CSE weapons: invalid definition path\n");
        return Result::InvalidDefinition;
    }
    std::string contents;
    if (!ReadFile(path, MaxDefinitionBytes, contents)) {
        ALERT(at_console, "CSE weapons: cannot read %.127s\n", path);
        return Result::MissingAsset;
    }
    Entry entry;
    unsigned line = 0;
    const ParseResult parsed = ParseDefinition(contents, entry.definition, &line);
    if (parsed != ParseResult::Ok) {
        ALERT(at_console, "CSE weapons: %s:%u rejected (parse result %d)\n", path, line, static_cast<int>(parsed));
        return Result::InvalidDefinition;
    }
    const char* key = entry.definition.presentation.key;
    if (const Entry* previous = FindEntry(key)) {
        ALERT(at_console, "CSE weapons: %s duplicates %s in %s\n", path, key, previous->source.c_str());
        return Result::Duplicate;
    }
    if (StockName(key)) {
        ALERT(at_console, "CSE weapons: %s reserves stock classname %s\n", path, key);
        return Result::Duplicate;
    }
    if (catalog.size() >= MaxDefinitions) {
        return Result::Capacity;
    }
    entry.source = path;
    catalog.push_back(std::move(entry));
    return Result::Ok;
}

const AmmoPool* BindAmmo(const Ammo& ammo) {
    if (!ammo.key[0]) {
        return nullptr;
    }
    const int index = ammo.stock ? CBasePlayer::GetAmmoIndex(ammo.key) : -1;
    const int capacity = ammo.stock && index >= 0 ?
        MaxAmmoCarry(ALLOC_STRING(CBasePlayerItem::AmmoInfoArray[index].pszName)) : ammo.capacity;
    if ((ammo.stock && index < 0) || capacity <= 0 || (ammo.capacity && ammo.capacity != capacity) ||
        ammo.initialReserve > capacity) {
        return nullptr;
    }
    for (const AmmoPool& pool : pools) {
        if (pool.stockIndex == index && (index >= 0 || pool.key == ammo.key)) {
            return pool.capacity == capacity ? &pool : nullptr;
        }
    }
    if (pools.size() == MaxAmmoTypes) {
        return nullptr;
    }
    pools.push_back(AmmoPool{ammo.key, static_cast<uint16_t>(capacity), index, pools.size()});
    return &pools.back();
}

int ModelSequences(const char* path) {
    constexpr int StudioMagic = 'I' | ('D' << 8) | ('S' << 16) | ('T' << 24);
    if (!AssetExists(path)) {
        return 0;
    }
    int length = 0;
    byte* data = LOAD_FILE_FOR_ME(const_cast<char*>(path), &length);
    if (!data) {
        return 0;
    }
    if (length < static_cast<int>(sizeof(studiohdr_t))) {
        FREE_FILE(data);
        return 0;
    }
    const auto field = [data](size_t offset) {
        int value = 0;
        std::memcpy(&value, data + offset, sizeof(value));
        return value;
    };
    const int sequenceCount = field(offsetof(studiohdr_t, numseq));
    const int lengthOnDisk = field(offsetof(studiohdr_t, length));
    const bool valid = field(offsetof(studiohdr_t, id)) == StudioMagic &&
        field(offsetof(studiohdr_t, version)) == STUDIO_VERSION && lengthOnDisk <= length &&
        lengthOnDisk >= static_cast<int>(sizeof(studiohdr_t));
    FREE_FILE(data);
    return valid ? sequenceCount : 0;
}

bool OptionalSound(Entry& entry, char (&sound)[64]) {
    if (!sound[0]) {
        return false;
    }
    if (!AssetExists(std::string("sound/") + sound)) {
        ALERT(at_console, "CSE weapons: %s optional sound %s is missing\n", entry.definition.presentation.key, sound);
        sound[0] = '\0';
        return false;
    }
    return true;
}

void AddResource(std::vector<std::string>& current, const char* path) {
    if (path[0] && !std::any_of(current.begin(), current.end(), [path](const std::string& resource) {
            return !stricmp(resource.c_str(), path);
        })) {
        current.emplace_back(path);
    }
}

Result PrecacheEntry(Entry& entry) {
    Definition& definition = entry.definition;
    if (definition.presentation.slot > 1 || !g_pGameRules || !g_pGameRules->IsMultiplayer()) {
        return Result::InvalidDefinition;
    }
    entry.viewSequences = ModelSequences(definition.viewModel);
    if (entry.viewSequences <= 0 || ModelSequences(definition.playerModel) <= 0 ||
        ModelSequences(definition.worldModel) <= 0 ||
        (definition.projectileModel[0] && ModelSequences(definition.projectileModel) <= 0)) {
        return Result::MissingAsset;
    }

    const auto priorModels = acceptedModels.size();
    const auto priorSounds = acceptedSounds.size();
    for (const char* path : {definition.viewModel, definition.playerModel, definition.worldModel,
            definition.projectileModel}) {
        AddResource(acceptedModels, path);
    }
    for (auto* sound : {&definition.fireSound, &definition.secondarySound, &definition.impactSound}) {
        if (OptionalSound(entry, *sound)) {
            AddResource(acceptedSounds, *sound);
        }
    }
    for (auto& sound : definition.animationSounds) {
        if (OptionalSound(entry, sound)) {
            AddResource(acceptedSounds, sound);
        }
    }
    if (acceptedModels.size() > MaxNewModelResources || acceptedSounds.size() > MaxNewSoundResources) {
        acceptedModels.resize(priorModels);
        acceptedSounds.resize(priorSounds);
        return Result::Capacity;
    }

    const size_t before = pools.size();
    entry.primary = BindAmmo(definition.primary);
    entry.secondary = BindAmmo(definition.secondary);
    if (!entry.primary || (definition.secondary.key[0] && !entry.secondary)) {
        acceptedModels.resize(priorModels);
        acceptedSounds.resize(priorSounds);
        while (pools.size() > before) {
            pools.pop_back();
        }
        entry.primary = nullptr;
        entry.secondary = nullptr;
        return Result::InvalidDefinition;
    }
    definition.primary.capacity = entry.primary->capacity;
    for (size_t i = priorModels; i < acceptedModels.size(); ++i) {
        // Allocate in the engine string table because the engine retains the resource pointer.
        const std::string optional = "!" + acceptedModels[i];
        PRECACHE_MODEL(const_cast<char*>(STRING(ALLOC_STRING(optional.c_str()))));
    }
    for (size_t i = priorSounds; i < acceptedSounds.size(); ++i) {
        PRECACHE_SOUND(const_cast<char*>(STRING(ALLOC_STRING(acceptedSounds[i].c_str()))));
    }
    return Result::Ok;
}

class CustomProjectile : public CBaseEntity {
public:
    void Launch(CBasePlayer& owner, const Definition& definition, const Vector& origin, const Vector& direction,
        const AttackIdentity& identity);
    void Touch(CBaseEntity* other) override;
    void Think() override;
    int ObjectCaps() override { return FCAP_DONT_SAVE; }

private:
    EHANDLE owner_;
    int userId_;
    const Definition* definition_;
    AttackIdentity identity_;
};

void CustomProjectile::Launch(CBasePlayer& owner, const Definition& definition, const Vector& origin,
    const Vector& direction, const AttackIdentity& identity) {
    owner_ = &owner;
    userId_ = GETPLAYERUSERID(owner.edict());
    definition_ = &definition;
    identity_ = identity;
    identity_.inflictor = pev;
    pev->classname = ALLOC_STRING(ProjectileClassname);
    pev->owner = owner.edict();
    pev->movetype = MOVETYPE_FLY;
    pev->solid = SOLID_BBOX;
    pev->velocity = direction * static_cast<float>(definition.projectileSpeed);
    pev->angles = UTIL_VecToAngles(direction);
    pev->nextthink = gpGlobals->time + ProjectileLifetime;
    SET_MODEL(edict(), definition.projectileModel);
    UTIL_SetSize(pev, g_vecZero, g_vecZero);
    UTIL_SetOrigin(pev, origin);
}

void CustomProjectile::Touch(CBaseEntity* other) {
    if ((pev->flags & FL_KILLME) || !other || other->edict() == owner_.Get() || other->pev->solid == SOLID_TRIGGER) {
        return;
    }
    edict_t* owner = owner_.Get();
    if (!owner || owner->free || GETPLAYERUSERID(owner) != userId_) {
        UTIL_Remove(this);
        return;
    }
    const Vector blastOrigin = pev->origin - pev->velocity.Normalize() * ProjectileImpactOffset;
    pev->solid = SOLID_NOT;
    pev->velocity = g_vecZero;
    if (definition_->impactSound[0]) {
        EMIT_SOUND(edict(), CHAN_WEAPON, definition_->impactSound, VOL_NORM, ATTN_NORM);
    }
    {
        AttackContext attack(identity_);
        constexpr int DamageType = DMG_EXPLOSION | DMG_NEVERGIB;
        const float radius = definition_->secondaryRadius;
        const bool inWater = UTIL_PointContents(blastOrigin) == CONTENTS_WATER;
        CBaseEntity* target = nullptr;
        while ((target = UTIL_FindEntityInSphere(target, blastOrigin, radius)) != nullptr) {
            if (target->pev->takedamage == DAMAGE_NO || (inWater && !target->pev->waterlevel) ||
                (!inWater && target->pev->waterlevel == 3)) {
                continue;
            }
            const Vector hitPoint = target->BodyTarget(blastOrigin);
            TraceResult trace;
            UTIL_TraceLine(blastOrigin, hitPoint, dont_ignore_monsters, edict(), &trace);
            if (trace.flFraction != 1.0f && trace.pHit != target->edict()) {
                continue;
            }
            const float distance = (blastOrigin - hitPoint).Length();
            const float scale = (std::clamp)(1.0f - distance / radius, 0.0f, 1.0f);
            if (scale > 0) {
                target->TakeDamage(pev, VARS(owner), definition_->secondaryDamage * scale, DamageType);
            }
        }
    }
    UTIL_Remove(this);
}

void CustomProjectile::Think() {
    UTIL_Remove(this);
}

class CustomWeapon : public CBasePlayerWeapon {
public:
    void Spawn() override;
    void Activate() override;
    void KeyValue(KeyValueData* data) override;
    void Think() override;
    void Touch(CBaseEntity* other) override;
    int ObjectCaps() override { return FCAP_DONT_SAVE; }

    int AddToPlayer(CBasePlayer* player) override;
    int AddDuplicate(CBasePlayerItem*) override { return FALSE; }
    int GetItemInfo(ItemInfo* info) override;
    int iItemSlot() override;
    BOOL CanDeploy() override;
    BOOL CanDrop() override;
    BOOL Deploy() override;
    void Holster(int skipLocal) override;
    BOOL IsUseable() override;
    float GetMaxSpeed() override;
    void ItemPostFrame() override;
    int UpdateClientData(CBasePlayer* player) override;
    void Drop() override;
    void Kill() override;

    bool Create(const Entry& entry);
    void SetLoose(const Vector& origin, const Vector& velocity, CBasePlayer* previous, bool expired);
    void ResetTemplate();
    void PreserveReserve(bool packAmmo);
    AttackIdentity Identity() const;
    uint32_t Serial() const { return serial_; }
    uint16_t Id() const { return entry_->definition.presentation.id; }
    const Definition& Specs() const { return entry_->definition; }
    const AmmoPool* Primary() const { return entry_->primary; }
    const AmmoPool* Secondary() const { return entry_->secondary; }
    bool IsSecondaryMode() const { return secondaryMode_; }
    bool IsPickupTemplate() const { return mapTemplate_; }
    uint32_t AnimationSerial() const { return animationSerial_; }
    uint8_t Animation() const { return animation_; }

private:
    void Animate(uint8_t sequence);
    void Fire(bool alternate);
    void StartReload();

    const Entry* entry_;
    string_t key_;
    uint32_t serial_;
    uint32_t animationSerial_;
    uint16_t initialPrimary_;
    uint16_t initialSecondary_;
    uint16_t packedPrimary_;
    uint16_t packedSecondary_;
    uint8_t animation_;
    bool secondaryMode_;
    bool mapTemplate_;
    Vector templateOrigin_;
    float nextAttack_;
    float nextSwitch_;
    float nextIdle_;
    float pickupAfter_;
    EHANDLE previousOwner_;
};

LINK_ENTITY_TO_CLASS(cse_custom_weapon, CustomWeapon)

template <class Entity>
void Track(std::vector<EHANDLE>& entities, Entity& entity) {
    entities.erase(std::remove_if(entities.begin(), entities.end(), [](const EHANDLE& handle) {
        edict_t* edict = handle.Get();
        return !edict || edict->free || (edict->v.flags & FL_KILLME);
    }), entities.end());
    EHANDLE handle{};
    handle = &entity;
    entities.push_back(handle);
}

void CustomWeapon::KeyValue(KeyValueData* data) {
    if (!std::strcmp(data->szKeyName, "weapon_key")) {
        key_ = ALLOC_STRING(data->szValue);
        data->fHandled = TRUE;
        return;
    }
    CBasePlayerWeapon::KeyValue(data);
}

void CustomWeapon::Spawn() {
    // A BSP entity can spawn before plugins finish registering its definition.
    pev->solid = SOLID_NOT;
    pev->effects |= EF_NODRAW;
}

void CustomWeapon::Activate() {
    if (entry_) {
        return;
    }
    const Entry* entry = key_ ? FindEntry(STRING(key_)) : nullptr;
    if (!entry || entry->status != Result::Ok || !Create(*entry)) {
        ALERT(at_console, "CSE weapons: unbound %s at (%.0f %.0f %.0f)\n", FactoryName,
            pev->origin.x, pev->origin.y, pev->origin.z);
        UTIL_Remove(this);
        return;
    }
    mapTemplate_ = true;
    templateOrigin_ = pev->origin;
    ResetTemplate();
}

bool CustomWeapon::Create(const Entry& entry) {
    if (nextInstance == (std::numeric_limits<uint32_t>::max)()) {
        return false;
    }
    entry_ = &entry;
    serial_ = ++nextInstance;
    m_iId = WEAPON_NONE;
    m_iClip = Specs().initialClip;
    m_iPrimaryAmmoType = entry.primary->stockIndex;
    m_iSecondaryAmmoType = entry.secondary ? entry.secondary->stockIndex : -1;
    initialPrimary_ = Specs().primary.initialReserve;
    initialSecondary_ = Specs().secondary.initialReserve;
    const char* oldClassname = STRING(pev->classname);
    if (*oldClassname) {
        RemoveEntityHashValue(pev, oldClassname, CLASSNAME);
    }
    pev->classname = ALLOC_STRING(Specs().presentation.key);
    AddEntityHashValue(pev, STRING(pev->classname), CLASSNAME);
    Track(worldItems, *this);
    Emit(Identity(), EventKind::Spawn);
    return true;
}

void CustomWeapon::SetLoose(const Vector& origin, const Vector& velocity, CBasePlayer* previous, bool expired) {
    m_pPlayer = nullptr;
    m_pNext = nullptr;
    previousOwner_ = previous;
    pickupAfter_ = previous ? gpGlobals->time + DropOwnerDelay : 0.0f;
    pev->owner = nullptr;
    pev->aiment = nullptr;
    pev->movetype = MOVETYPE_TOSS;
    pev->effects &= ~EF_NODRAW;
    pev->solid = SOLID_TRIGGER;
    pev->flags &= ~FL_ONGROUND;
    pev->angles.x = 0;
    pev->angles.z = 0;
    pev->velocity = velocity;
    pev->nextthink = expired ? gpGlobals->time + DropLifetime : 0.0f;
    SET_MODEL(edict(), Specs().worldModel);
    UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));
    UTIL_SetOrigin(pev, origin);
}

void CustomWeapon::ResetTemplate() {
    SetLoose(templateOrigin_, g_vecZero, nullptr, false);
}

void CustomWeapon::Think() {
    if (m_pPlayer) {
        return;
    }
    if (mapTemplate_) {
        ResetTemplate();
    } else {
        Kill();
    }
}

bool Eligible(CBasePlayer& player, const Definition& definition) {
    return player.IsAlive() && !player.IsBot() && !player.m_bIsVIP && !player.HasShield() &&
        (player.m_iTeam == CT || player.m_iTeam == TERRORIST) &&
        (!definition.team || definition.team == player.m_iTeam);
}

int CustomCount(const CBasePlayer& player) {
    int count = 0;
    for (const CBasePlayerItem* slot : player.m_rgpPlayerItems) {
        for (const CBasePlayerItem* item = slot; item; item = item->m_pNext) {
            if (dynamic_cast<const CustomWeapon*>(item)) {
                ++count;
            }
        }
    }
    return count;
}

bool SharedSlots() {
    return sharedSlots.value != 0;
}

bool CanPickUp(CBasePlayer& player, CustomWeapon& weapon) {
    return IsReady(player) && Eligible(player, weapon.Specs()) && CustomCount(player) < MaxOwned &&
        (SharedSlots() || !player.m_rgpPlayerItems[weapon.iItemSlot()]) && !player.HasPlayerItem(&weapon);
}

CustomWeapon* SpawnWeapon(const Entry& entry, const Vector& origin) {
    edict_t* entity = CREATE_ENTITY();
    if (!entity) {
        return nullptr;
    }
    CustomWeapon* weapon = GetClassPtr(reinterpret_cast<CustomWeapon*>(VARS(entity)));
    if (!weapon->Create(entry)) {
        UTIL_Remove(weapon);
        return nullptr;
    }
    weapon->SetLoose(origin, g_vecZero, nullptr, true);
    return weapon;
}

void CustomWeapon::Touch(CBaseEntity* other) {
    if (!entry_ || m_pPlayer || (pev->flags & FL_KILLME) || !(pev->flags & FL_ONGROUND) || !other ||
        !other->IsPlayer() || (other->edict() == previousOwner_.Get() && gpGlobals->time < pickupAfter_)) {
        return;
    }
    auto& player = static_cast<CBasePlayer&>(*other);
    if (!CanPickUp(player, *this) || !player.AddPlayerItem(this)) {
        return;
    }
    AttachToPlayer(&player);
    if (mapTemplate_ && Specs().respawn) {
        CustomWeapon* replacement = SpawnWeapon(*entry_, templateOrigin_);
        if (replacement) {
            replacement->mapTemplate_ = true;
            replacement->templateOrigin_ = templateOrigin_;
            replacement->pev->nextthink = g_pGameRules->FlWeaponRespawnTime(this);
            replacement->pev->movetype = MOVETYPE_NONE;
            replacement->pev->solid = SOLID_NOT;
            replacement->pev->effects |= EF_NODRAW;
        }
    }
    mapTemplate_ = false;
}

int CustomWeapon::AddToPlayer(CBasePlayer* player) {
    if (!player || !CanPickUp(*player, *this)) {
        return FALSE;
    }
    m_pPlayer = player;
    AddReserve(*player, Primary(), initialPrimary_);
    AddReserve(*player, Secondary(), initialSecondary_);
    AddReserve(*player, Primary(), packedPrimary_);
    AddReserve(*player, Secondary(), packedSecondary_);
    initialPrimary_ = 0;
    initialSecondary_ = 0;
    packedPrimary_ = 0;
    packedSecondary_ = 0;
    Session(*player)->pickup = Id();
    Emit(Identity(), EventKind::Pickup);
    EMIT_SOUND(player->edict(), CHAN_ITEM, "items/gunpickup2.wav", VOL_NORM, ATTN_NORM);
    return TRUE;
}

int CustomWeapon::GetItemInfo(ItemInfo* info) {
    if (!entry_ || !info) {
        return FALSE;
    }
    const Definition& definition = Specs();
    *info = ItemInfo{definition.presentation.slot, definition.presentation.order, definition.primary.key,
        Primary()->capacity, Secondary() ? definition.secondary.key : nullptr, Secondary() ? Secondary()->capacity : 0,
        definition.presentation.key, definition.clip, WEAPON_NONE, 0, definition.weight};
    return TRUE;
}

int CustomWeapon::iItemSlot() {
    return Specs().presentation.slot == 0 ? PrimarySlot : SecondarySlot;
}

BOOL CustomWeapon::CanDeploy() {
    return m_pPlayer && IsReady(*m_pPlayer) && Eligible(*m_pPlayer, Specs());
}

BOOL CustomWeapon::CanDrop() {
    return Specs().droppable != 0;
}

BOOL CustomWeapon::Deploy() {
    if (!CanDeploy()) {
        return FALSE;
    }
    const Definition& definition = Specs();
    m_pPlayer->pev->viewmodel = ALLOC_STRING(definition.viewModel);
    m_pPlayer->pev->weaponmodel = ALLOC_STRING(definition.playerModel);
    model_name = m_pPlayer->pev->viewmodel;
    std::strcpy(m_pPlayer->m_szAnimExtention, iItemSlot() == PrimarySlot ? "rifle" : "onehanded");
    m_pPlayer->m_iFOV = DefaultFov;
    m_pPlayer->pev->fov = DefaultFov;
    m_pPlayer->m_iLastZoom = DefaultFov;
    m_pPlayer->m_bResumeZoom = false;
    m_pPlayer->m_flEjectBrass = 0;
    m_pPlayer->m_flNextAttack = Seconds(definition.deployMs);
    nextIdle_ = gpGlobals->time + Seconds(definition.deployMs);
    Animate(secondaryMode_ ? definition.secondaryDrawAnim : definition.drawAnim);
    Emit(Identity(), EventKind::Deploy);
    return TRUE;
}

void CustomWeapon::Holster(int) {
    if (!m_pPlayer) {
        return;
    }
    m_fInReload = FALSE;
    m_pPlayer->m_bResumeZoom = false;
    m_pPlayer->m_iFOV = DefaultFov;
    m_pPlayer->pev->fov = DefaultFov;
    m_pPlayer->m_iLastZoom = DefaultFov;
    m_pPlayer->m_flNextAttack = 0;
    CBasePlayerItem::Holster();
    Emit(Identity(), EventKind::Holster);
}

BOOL CustomWeapon::IsUseable() {
    return m_iClip > 0 || ReserveAmount(m_pPlayer, Primary()) > 0 || ReserveAmount(m_pPlayer, Secondary()) > 0;
}

float CustomWeapon::GetMaxSpeed() {
    return Specs().speed;
}

void CustomWeapon::Animate(uint8_t sequence) {
    animation_ = sequence < entry_->viewSequences ? sequence : 0;
    if (++animationSerial_ == 0) {
        ++animationSerial_;
    }
    SendWeaponAnim(animation_, 0);
}

AttackIdentity CustomWeapon::Identity() const {
    entvars_t* owner = m_pPlayer ? m_pPlayer->pev : nullptr;
    return AttackIdentity{owner, owner, Id(), serial_, static_cast<uint16_t>(m_iClip), ReserveAmount(m_pPlayer, Primary()),
        ReserveAmount(m_pPlayer, Secondary())};
}

void CustomWeapon::Fire(bool alternate) {
    const Definition& definition = Specs();
    CBasePlayer& owner = *m_pPlayer;
    const float now = gpGlobals->time;
    nextAttack_ = now + Seconds(alternate ? definition.secondaryIntervalMs : definition.fireIntervalMs);
    const bool noClip = definition.clip == 0;
    const bool emptyPrimary = noClip ? ReserveAmount(&owner, Primary()) == 0 : m_iClip == 0;
    if (alternate ? ReserveAmount(&owner, Secondary()) == 0 : emptyPrimary) {
        ResetEmptySound();
        PlayEmptySound();
        if (alternate) {
            Animate(definition.secondaryEmptyAnim);
        }
        return;
    }
    UTIL_MakeVectors(owner.pev->v_angle + owner.pev->punchangle);
    const Vector source = owner.GetGunPosition();
    if (alternate) {
        edict_t* entity = CREATE_ENTITY();
        if (!entity) {
            return;
        }
        CustomProjectile* projectile = GetClassPtr(reinterpret_cast<CustomProjectile*>(VARS(entity)));
        --Reserve(owner, *Secondary());
        if (Secondary()->stockIndex >= 0) {
            owner.TabulateAmmo();
        }
        const AttackIdentity identity = Identity();
        Emit(identity, EventKind::Shot);
        projectile->Launch(owner, definition, source, gpGlobals->v_forward, identity);
        Track(projectiles, *projectile);
    } else {
        if (noClip) {
            --Reserve(owner, *Primary());
            if (Primary()->stockIndex >= 0) {
                owner.TabulateAmmo();
            }
        } else {
            --m_iClip;
        }
        const AttackIdentity identity = Identity();
        Emit(identity, EventKind::Shot);
        const AttackContext attack(identity);
        // The ballistic trace is shared with stock firearms. The damage context supplies the custom identity.
        constexpr int TracePasses = 1;
        constexpr float NoRangeFalloff = 1.0f;
        owner.FireBullets3(source, gpGlobals->v_forward, Seconds(definition.spreadThousandths), definition.range,
            TracePasses, BULLET_PLAYER_762MM, definition.damage, NoRangeFalloff, owner.pev,
            iItemSlot() == SecondarySlot, owner.random_seed);
    }
    owner.pev->effects |= EF_MUZZLEFLASH;
    owner.SetAnimation(PLAYER_ATTACK1);
    owner.m_iWeaponVolume = NORMAL_GUN_VOLUME;
    owner.m_iWeaponFlash = BRIGHT_GUN_FLASH;
    owner.pev->punchangle.x -= Seconds(definition.recoilThousandths);
    Animate(alternate ? definition.secondaryFireAnim : definition.fireAnim);
    const char* sound = alternate ? definition.secondarySound : definition.fireSound;
    if (sound[0]) {
        EMIT_SOUND(owner.edict(), CHAN_WEAPON, sound, VOL_NORM, ATTN_NORM);
    }
    nextIdle_ = nextAttack_;
}

void CustomWeapon::StartReload() {
    if (secondaryMode_ || !Specs().clip || m_iClip >= Specs().clip || !ReserveAmount(m_pPlayer, Primary())) {
        return;
    }
    m_fInReload = TRUE;
    m_pPlayer->m_flNextAttack = Seconds(Specs().reloadMs);
    Animate(Specs().reloadAnim);
    m_pPlayer->SetAnimation(PLAYER_RELOAD);
    Emit(Identity(), EventKind::ReloadBegin);
}

void CustomWeapon::ItemPostFrame() {
    const float now = gpGlobals->time;
    CBasePlayer& owner = *m_pPlayer;
    const Definition& definition = Specs();
    if (m_fInReload) {
        int& reserve = Reserve(owner, *Primary());
        const int count = (std::min)(static_cast<int>(definition.clip) - m_iClip, reserve);
        m_iClip += count;
        reserve -= count;
        if (Primary()->stockIndex >= 0) {
            owner.TabulateAmmo();
        }
        m_fInReload = FALSE;
        Emit(Identity(), EventKind::ReloadComplete);
    }
    if (owner.m_afButtonPressed & IN_ATTACK2 && now >= nextSwitch_) {
        if (Secondary()) {
            secondaryMode_ = !secondaryMode_;
            Animate(secondaryMode_ ? definition.switchToSecondaryAnim : definition.switchToPrimaryAnim);
            nextSwitch_ = now + Seconds(definition.switchIntervalMs);
            nextAttack_ = (std::max)(nextAttack_, nextSwitch_);
            nextIdle_ = nextSwitch_;
        } else if (definition.zoom) {
            owner.m_iFOV = owner.m_iFOV == DefaultFov ? definition.zoom : DefaultFov;
            owner.pev->fov = static_cast<float>(owner.m_iFOV);
            nextSwitch_ = now + Seconds(definition.switchIntervalMs);
            owner.ResetMaxSpeed();
        }
        return;
    }
    if (now < nextAttack_) {
        return;
    }
    const bool firing = (owner.pev->button & IN_ATTACK) != 0;
    const bool pressed = (owner.m_afButtonPressed & IN_ATTACK) != 0;
    if (firing && (pressed || (definition.automatic && !secondaryMode_)) && owner.m_bCanShoot &&
        !g_pGameRules->IsFreezePeriod() && !owner.m_bIsDefusing) {
        Fire(secondaryMode_);
        return;
    }
    if (!firing && ((owner.pev->button & IN_RELOAD) || m_iClip == 0)) {
        StartReload();
        if (m_fInReload) {
            return;
        }
    }
    if (now >= nextIdle_) {
        constexpr float IdleInterval = 5.0f;
        Animate(secondaryMode_ ? definition.secondaryIdleAnim : definition.idleAnim);
        nextIdle_ = now + IdleInterval;
    }
}

int CustomWeapon::UpdateClientData(CBasePlayer* player) {
    // Continue the stock linked-list traversal without adding a legacy weapon row.
    if (m_pNext) {
        m_pNext->UpdateClientData(player);
    }
    return TRUE;
}

void CustomWeapon::PreserveReserve(bool packAmmo) {
    if (!packAmmo || !m_pPlayer) {
        return;
    }
    packedPrimary_ = ReserveAmount(m_pPlayer, Primary());
    Reserve(*m_pPlayer, *Primary()) = 0;
    if (Secondary()) {
        packedSecondary_ = ReserveAmount(m_pPlayer, Secondary());
        Reserve(*m_pPlayer, *Secondary()) = 0;
    }
    m_pPlayer->TabulateAmmo();
}

void CustomWeapon::Drop() {
    Kill();
}

void CustomWeapon::Kill() {
    if (!(pev->flags & FL_KILLME)) {
        if (entry_) {
            Emit(Identity(), EventKind::Remove);
        }
        UTIL_Remove(this);
    }
}

CustomWeapon* OwnedBy(CBasePlayer& player, uint32_t serial) {
    if (!serial) {
        return nullptr;
    }
    for (CBasePlayerItem* slot : player.m_rgpPlayerItems) {
        for (CBasePlayerItem* item = slot; item; item = item->m_pNext) {
            auto* custom = dynamic_cast<CustomWeapon*>(item);
            if (custom && custom->Serial() == serial && !(custom->pev->flags & FL_KILLME)) {
                return custom;
            }
        }
    }
    return nullptr;
}

void Unlink(CBasePlayer& player, CustomWeapon& weapon) {
    if (player.m_pActiveItem == &weapon && !g_pGameRules->GetNextBestWeapon(&player, &weapon)) {
        static_cast<CBasePlayerItem&>(weapon).Holster();
    }
    player.RemovePlayerItem(&weapon);
    player.m_bHasPrimary = player.HasShield() || player.m_rgpPlayerItems[PrimarySlot] != nullptr;
    player.ResetMaxSpeed();
}

bool MakeDrop(CBasePlayer& player, CustomWeapon& weapon, bool death, bool packAmmo) {
    if (weapon.m_pPlayer != &player || !weapon.CanDrop() || (!death && !weapon.CanHolster())) {
        return false;
    }
    const AttackIdentity identity = weapon.Identity();
    weapon.PreserveReserve(packAmmo);
    Unlink(player, weapon);
    Vector origin = player.pev->origin;
    constexpr float DeathVelocityScale = 0.75f;
    Vector velocity = player.pev->velocity * DeathVelocityScale;
    if (!death) {
        constexpr float DropOffset = 10.0f;
        constexpr float DropSpeed = 400.0f;
        UTIL_MakeVectors(player.pev->angles);
        origin = origin + gpGlobals->v_forward * DropOffset;
        velocity = gpGlobals->v_forward * DropSpeed;
    }
    weapon.SetLoose(origin, velocity, &player, true);
    Emit(identity, EventKind::Drop);
    return true;
}

State Snapshot(CBasePlayer& viewer, bool observing) {
    State state{};
    state.generation = generation;
    CBasePlayer* subject = &viewer;
    if (observing && viewer.pev->iuser1 == OBS_IN_EYE && viewer.m_hObserverTarget) {
        auto* target = dynamic_cast<CBasePlayer*>(CBaseEntity::Instance(viewer.m_hObserverTarget.Get()));
        if (target && target->IsAlive()) {
            subject = target;
            state.flags |= Observing;
        }
    }
    state.subject = static_cast<uint8_t>(subject->entindex());
    auto* active = dynamic_cast<CustomWeapon*>(subject->m_pActiveItem);
    if (active) {
        const AttackIdentity identity = active->Identity();
        state.active = active->Serial();
        state.animationSerial = active->AnimationSerial();
        state.animation = active->Animation();
        state.clip = identity.clip;
        state.reserve = identity.reserve;
        state.secondary = identity.secondary;
        state.zoom = static_cast<uint8_t>((std::clamp)(subject->m_iFOV, 0, DefaultFov));
        if (!active->Specs().clip) {
            state.flags |= NoClip;
        }
        if (active->IsSecondaryMode()) {
            state.flags |= SecondaryMode;
        }
        if (active->m_fInReload) {
            state.flags |= Reloading;
        }
    }
    if (state.flags & Observing) {
        if (active) {
            state.items[state.count++] = Owned{active->Serial(), active->Id()};
        }
        return state;
    }
    for (CBasePlayerItem* slot : subject->m_rgpPlayerItems) {
        for (CBasePlayerItem* item = slot; item && state.count < MaxOwned; item = item->m_pNext) {
            if (auto* weapon = dynamic_cast<CustomWeapon*>(item)) {
                state.items[state.count++] = Owned{weapon->Serial(), weapon->Id()};
                if (subject->m_pLastItem == item) {
                    state.last = weapon->Serial();
                }
            }
        }
    }
    return state;
}

bool Changed(const State& lhs, const State& rhs) {
    if (lhs.generation != rhs.generation || lhs.active != rhs.active || lhs.last != rhs.last ||
        lhs.animationSerial != rhs.animationSerial || lhs.animation != rhs.animation ||
        lhs.clip != rhs.clip || lhs.reserve != rhs.reserve || lhs.secondary != rhs.secondary ||
        lhs.subject != rhs.subject || lhs.flags != rhs.flags || lhs.zoom != rhs.zoom || lhs.count != rhs.count) {
        return true;
    }
    for (unsigned i = 0; i < lhs.count; ++i) {
        if (lhs.items[i].instance != rhs.items[i].instance || lhs.items[i].definition != rhs.items[i].definition) {
            return true;
        }
    }
    return false;
}

uint16_t Find(const char* key) {
    const Entry* entry = FindEntry(key);
    return entry && entry->status == Result::Ok ? entry->definition.presentation.id : 0;
}

Result RegistrationStatus(const char* key) {
    const Entry* entry = FindEntry(key);
    return entry ? entry->status : Result::InvalidDefinition;
}

int32_t PlayerReadiness(int32_t index) {
    CBasePlayer* player = Player(index);
    Client* client = player ? Session(*player) : nullptr;
    return static_cast<int32_t>(client ? client->readiness : Readiness::Unsupported);
}

Result DefinitionInfo(uint16_t id, Definition* out) {
    const Entry* entry = FindEntry(id);
    if (!entry || !out) {
        return Result::InvalidDefinition;
    }
    *out = entry->definition;
    return Result::Ok;
}

Result Query(int32_t index, State* state) {
    CBasePlayer* player = Player(index);
    Client* client = player ? Session(*player) : nullptr;
    if (!client || !state) {
        return Result::InvalidPlayer;
    }
    *state = Snapshot(*player, false);
    state->sequence = (std::max)(client->sequence, 1u);
    return Result::Ok;
}

bool PollEvent(Event* event) {
    if (!event || events.empty()) {
        return false;
    }
    *event = events.front();
    events.pop_front();
    return true;
}

uint32_t Generation() {
    return generation;
}

bool ValidLocation(const Request& request) {
    constexpr float MapCoordinateLimit = 32768.0f;
    for (float value : request.origin) {
        if (!std::isfinite(value) || std::abs(value) >= MapCoordinateLimit) {
            return false;
        }
    }
    return true;
}

bool ValidPolicy(SlotPolicy policy) {
    return policy == SlotPolicy::Reject || policy == SlotPolicy::Replace || policy == SlotPolicy::Drop;
}

Result Give(CBasePlayer& player, const Entry& entry, Operation operation, const Request& request, uint32_t& serial) {
    Client& client = *Session(player);
    const Definition& definition = entry.definition;
    const bool buy = operation == Operation::Buy;
    const bool transaction = buy && request.transaction;
    if (transaction) {
        for (const auto& purchase : client.purchases) {
            if (purchase.transaction != request.transaction) {
                continue;
            }
            if (purchase.definition != request.definition || purchase.policy != request.policy) {
                return Result::Blocked;
            }
            serial = purchase.instance;
            return Result::Ok;
        }
        if (client.purchases.size() >= MaxPurchaseTransactions) {
            return Result::Capacity;
        }
    }
    if (!Eligible(player, definition) || !ValidPolicy(request.policy)) {
        return Result::Blocked;
    }
    if (buy && (!definition.price || !player.CanPlayerBuy(false))) {
        return Result::Blocked;
    }
    if (buy && player.m_iAccount < definition.price) {
        return Result::InsufficientFunds;
    }
    const int slot = definition.presentation.slot == 0 ? PrimarySlot : SecondarySlot;
    CBasePlayerItem* occupied = player.m_rgpPlayerItems[slot];
    for (CBasePlayerItem* item = occupied; item; item = item->m_pNext) {
        if (FStrEq(STRING(item->pev->classname), definition.presentation.key)) {
            return Result::Occupied;
        }
    }
    if (SharedSlots() && request.policy == SlotPolicy::Reject) {
        occupied = nullptr;
    }
    if (occupied && request.policy == SlotPolicy::Reject) {
        return Result::Occupied;
    }
    if (occupied && (!occupied->CanDrop() || !occupied->CanHolster() || occupied->m_pNext)) {
        return Result::Blocked;
    }
    if (CustomCount(player) >= MaxOwned && !dynamic_cast<CustomWeapon*>(occupied)) {
        return Result::Capacity;
    }
    CustomWeapon* weapon = SpawnWeapon(entry, player.pev->origin);
    if (!weapon) {
        return Result::Capacity;
    }
    if (occupied) {
        if (request.policy == SlotPolicy::Drop) {
            player.DropPlayerItem(STRING(occupied->pev->classname));
        } else if (auto* custom = dynamic_cast<CustomWeapon*>(occupied)) {
            Unlink(player, *custom);
            custom->Kill();
        } else {
            if (player.m_pActiveItem == occupied) {
                g_pGameRules->GetNextBestWeapon(&player, occupied);
            }
            player.RemovePlayerItem(occupied);
            player.pev->weapons &= ~(1 << occupied->m_iId);
            occupied->Kill();
        }
    }
    if (!player.AddPlayerItem(weapon)) {
        weapon->Kill();
        return Result::Blocked;
    }
    weapon->AttachToPlayer(&player);
    serial = weapon->Serial();
    if (buy) {
        player.AddAccount(-static_cast<int>(definition.price), true);
        if (transaction) {
            client.purchases.push_back(Purchase{request.transaction, serial, request.definition, request.policy});
        }
    }
    return Result::Ok;
}

Result Operate(Operation operation, const Request* request, uint32_t* instance) {
    if (!request || !mapActive || request->generation != generation) {
        return Result::WrongPhase;
    }
    if (instance) {
        *instance = 0;
    }
    if (operation == Operation::Spawn || operation == Operation::Give || operation == Operation::Buy) {
        const Entry* entry = FindEntry(request->definition);
        if (!entry) {
            return Result::InvalidDefinition;
        }
        if (operation == Operation::Spawn) {
            if (!ValidLocation(*request)) {
                return Result::Blocked;
            }
            CustomWeapon* weapon = SpawnWeapon(*entry, Vector(request->origin[0], request->origin[1], request->origin[2]));
            if (!weapon) {
                return Result::Capacity;
            }
            if (instance) {
                *instance = weapon->Serial();
            }
            return Result::Ok;
        }
        CBasePlayer* player = Player(request->player);
        if (!player) {
            return Result::InvalidPlayer;
        }
        if (!IsReady(*player)) {
            return Result::NotReady;
        }
        uint32_t serial = 0;
        const Result result = Give(*player, *entry, operation, *request, serial);
        if (instance) {
            *instance = serial;
        }
        return result;
    }
    CBasePlayer* player = Player(request->player);
    if (!player) {
        return Result::InvalidPlayer;
    }
    if (!IsReady(*player)) {
        return Result::NotReady;
    }
    CustomWeapon* weapon = OwnedBy(*player, request->instance);
    if (!weapon) {
        return Result::NotOwned;
    }
    switch (operation) {
    case Operation::Equip:
        if (!weapon->CanDeploy() || (player->m_pActiveItem && !player->m_pActiveItem->CanHolster())) {
            return Result::Blocked;
        }
        player->SelectItem(weapon->Specs().presentation.key);
        break;
    case Operation::Drop:
        if (!MakeDrop(*player, *weapon, false, false)) {
            return Result::Blocked;
        }
        break;
    case Operation::Remove:
        Unlink(*player, *weapon);
        weapon->Kill();
        break;
    case Operation::GrantAmmo: {
        const AmmoPool* pool = request->secondary ? weapon->Secondary() : weapon->Primary();
        if (!pool) {
            return Result::InvalidDefinition;
        }
        AddReserve(*player, pool, request->amount);
        break;
    }
    default:
        return Result::Blocked;
    }
    if (instance) {
        *instance = request->instance;
    }
    return Result::Ok;
}

const Api api{ApiVersion, sizeof(Api), RegisterFile, Find, RegistrationStatus, PlayerReadiness,
    Operate, Query, PollEvent, Generation, DefinitionInfo};

bool Number(const char* text, uint32_t& output) {
    const char* end = text + std::strlen(text);
    const auto result = std::from_chars(text, end, output);
    return result.ec == std::errc{} && result.ptr == end;
}

bool Coordinate(const char* text, float& output) {
    char* end = nullptr;
    output = std::strtof(text, &end);
    return end != text && *end == '\0';
}

void ReportConsoleResult(Result result, uint32_t serial) {
    ALERT(at_console, "CSE weapons: result %d, instance %u\n", static_cast<int>(result), serial);
}

void ConsoleList() {
    ALERT(at_console, "CSE weapons: catalog generation %u, %d active definitions\n", generation, committedDefinitions);
    for (const Entry& entry : catalog) {
        ALERT(at_console, "  %s (%d) status %d: %s\n", entry.definition.presentation.key,
            entry.definition.presentation.id, static_cast<int>(entry.status), entry.source.c_str());
    }
}

void ConsoleGive() {
    if (CMD_ARGC() != 3 && CMD_ARGC() != 4) {
        ALERT(at_console, "Usage: cse_weapon_give <player-index> <weapon_key> [reject|replace|drop]\n");
        return;
    }
    uint32_t index = 0;
    if (!Number(CMD_ARGV(1), index) || index > MaxPlayers) {
        ReportConsoleResult(Result::InvalidPlayer, 0);
        return;
    }
    Request request{};
    request.generation = generation;
    request.definition = Find(CMD_ARGV(2));
    request.player = static_cast<int32_t>(index);
    if (CMD_ARGC() == 4) {
        const char* policy = CMD_ARGV(3);
        if (!std::strcmp(policy, "replace")) {
            request.policy = SlotPolicy::Replace;
        } else if (!std::strcmp(policy, "drop")) {
            request.policy = SlotPolicy::Drop;
        } else if (std::strcmp(policy, "reject")) {
            ReportConsoleResult(Result::Blocked, 0);
            return;
        }
    }
    uint32_t serial = 0;
    const Result result = Operate(Operation::Give, &request, &serial);
    ReportConsoleResult(result, serial);
}

void ConsoleSpawn() {
    if (CMD_ARGC() != 5) {
        ALERT(at_console, "Usage: cse_weapon_spawn <weapon_key> <x> <y> <z>\n");
        return;
    }
    Request request{};
    request.generation = generation;
    request.definition = Find(CMD_ARGV(1));
    for (int i = 0; i < 3; ++i) {
        if (!Coordinate(CMD_ARGV(i + 2), request.origin[i])) {
            ReportConsoleResult(Result::Blocked, 0);
            return;
        }
    }
    uint32_t serial = 0;
    const Result result = Operate(Operation::Spawn, &request, &serial);
    ReportConsoleResult(result, serial);
}

} // namespace

void Initialize() {
    CVAR_REGISTER(&catalogPath);
    char* startupCatalog = nullptr;
    if (g_engfuncs.pfnCheckParm && g_engfuncs.pfnCheckParm("-cseweaponcatalog", &startupCatalog) && startupCatalog &&
        *startupCatalog && *startupCatalog != '-' && *startupCatalog != '+') {
        CVAR_SET_STRING(catalogPath.name, startupCatalog);
    }
    CVAR_REGISTER(&requireClient);
    CVAR_REGISTER(&sharedSlots);
    g_engfuncs.pfnAddServerCommand("cse_weapon_list", ConsoleList);
    g_engfuncs.pfnAddServerCommand("cse_weapon_give", ConsoleGive);
    g_engfuncs.pfnAddServerCommand("cse_weapon_spawn", ConsoleSpawn);
}

void StageConfiguredCatalog() {
    const std::string path = catalogPath.string;
    if (path.empty()) {
        return;
    }
    if (IsConfigPath(path, ".cfg")) {
        RegisterFile(path.c_str());
        return;
    }
    std::string text;
    if (!IsConfigPath(path, ".lst") || !ReadFile(path.c_str(), MaxDefinitionBytes, text)) {
        ALERT(at_console, "CSE weapons: invalid or missing catalog file %.127s\n", path.c_str());
        return;
    }
    std::istringstream file(text);
    std::string line;
    while (std::getline(file, line)) {
        const size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }
        const size_t last = line.find_last_not_of(" \t\r");
        RegisterFile(line.substr(first, last - first + 1).c_str());
    }
}

void Activate() {
    catalogOpen = false;
    mapActive = true;
    if (++generation == 0) {
        ++generation;
    }
    committedDefinitions = 0;
    digest = InitialDigest;
    for (Entry& entry : catalog) {
        entry.status = PrecacheEntry(entry);
        if (entry.status != Result::Ok) {
            ALERT(at_console, "CSE weapons: %s in %s rejected (resource/binding result %d)\n",
                entry.definition.presentation.key, entry.source.c_str(), static_cast<int>(entry.status));
            continue;
        }
        entry.definition.presentation.id = ++committedDefinitions;
        digest = UpdateDigest(digest, CatalogItem{generation, entry.definition.presentation});
    }
}

void Deactivate() {
    catalogOpen = true;
    mapActive = false;
    clients = {};
    worldItems.clear();
    projectiles.clear();
    events.clear();
    catalog.clear();
    pools.clear();
    acceptedModels.clear();
    acceptedSounds.clear();
    nextInstance = 0;
    killSequence = 0;
    committedDefinitions = 0;
}

void RoundCleanup() {
    for (const EHANDLE& handle : projectiles) {
        if (edict_t* entity = handle.Get(); entity && !entity->free) {
            UTIL_Remove(CBaseEntity::Instance(entity));
        }
    }
    projectiles.clear();
    for (const EHANDLE& handle : worldItems) {
        if (edict_t* entity = handle.Get(); entity && !entity->free) {
            auto* weapon = dynamic_cast<CustomWeapon*>(CBaseEntity::Instance(entity));
            if (weapon && !weapon->m_pPlayer) {
                if (weapon->IsPickupTemplate()) {
                    weapon->ResetTemplate();
                } else {
                    weapon->Kill();
                }
            }
        }
    }
}

void LinkMessages() {
    if (!messageId) {
        messageId = REG_USER_MSG(UserMessage, -1);
    }
}

bool AcceptClient(edict_t* client, char* rejectReason) {
    if (!committedDefinitions || !requireClient.value || (client->v.flags & (FL_FAKECLIENT | FL_PROXY)) ||
        Supports(client)) {
        return true;
    }
    std::strcpy(rejectReason, "This server requires a CSE custom-weapon client (protocol 1).");
    return false;
}

void Connect(CBasePlayer& player) {
    const int index = player.entindex();
    if (index < 1 || index > MaxPlayers) {
        return;
    }
    Client& client = clients[static_cast<size_t>(index)];
    client = {};
    client.entity = &player;
}

void Disconnect(CBasePlayer& player) {
    if (Client* client = Session(player)) {
        *client = {};
    }
}

void UpdateClient(CBasePlayer& player) {
    Client* client = Session(player);
    if (!client || !mapActive || !committedDefinitions || player.IsBot() || (player.pev->flags & FL_PROXY)) {
        return;
    }
    if (client->readiness == Readiness::Unsupported) {
        if (!Supports(player.edict())) {
            return;
        }
        client->readiness = Readiness::Synchronizing;
        client->nextCatalog = gpGlobals->time;
    }
    if (client->catalogStarted || client->resyncRequested ||
        (client->readiness == Readiness::Synchronizing && client->catalogAttempts < MaxCatalogAttempts)) {
        if (gpGlobals->time >= client->nextCatalog) {
            if (!client->catalogStarted) {
                Send(player, Encode(Begin{generation, committedDefinitions}));
                client->catalogStarted = true;
                client->catalogCursor = 0;
                ++client->catalogAttempts;
                client->nextCatalog = gpGlobals->time + CatalogPacketInterval;
            } else {
                while (client->catalogCursor < catalog.size() && catalog[client->catalogCursor].status != Result::Ok) {
                    ++client->catalogCursor;
                }
                if (client->catalogCursor < catalog.size()) {
                    const Presentation& presentation = catalog[client->catalogCursor++].definition.presentation;
                    Send(player, Encode(CatalogItem{generation, presentation}));
                    client->nextCatalog = gpGlobals->time + CatalogPacketInterval;
                } else {
                    Send(player, Encode(End{generation, digest}));
                    client->catalogStarted = false;
                    client->catalogSent = true;
                    client->resyncRequested = false;
                    client->nextCatalog = gpGlobals->time + CatalogRetryDelay;
                    client->lastState.generation = 0;
                    client->nextState = 0;
                }
            }
        }
        if (client->catalogStarted) {
            return;
        }
    }
    if (client->readiness != Readiness::Ready) {
        return;
    }
    State state = Snapshot(player, true);
    if (const bool customActive = state.active != 0; customActive != client->legacyCustom) {
        client->legacyCustom = customActive;
        if (customActive) {
            MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, nullptr, player.edict());
            WRITE_BYTE(0);
            WRITE_BYTE(0);
            WRITE_BYTE(0);
            MESSAGE_END();
        }
    }
    const bool viewChanged = state.active != client->lastState.active || state.subject != client->lastState.subject;
    if ((!viewChanged && gpGlobals->time < client->nextState) ||
        (!client->pickup && !Changed(state, client->lastState))) {
        return;
    }
    state.sequence = ++client->sequence;
    if (!state.sequence) {
        state.sequence = ++client->sequence;
    }
    state.pickup = (state.flags & Observing) ? 0 : client->pickup;
    client->pickup = 0;
    client->lastState = state;
    client->nextState = gpGlobals->time + ThinSnapshotDelay;
    Send(player, Encode(state));
}

bool ClientCommand(CBasePlayer& player, const char* command) {
    Client* client = Session(player);
    if (!std::strcmp(command, "cse_wready") || !std::strcmp(command, "cse_wselect") ||
        !std::strcmp(command, "cse_wsync")) {
        uint32_t requestedGeneration = 0;
        if (!client || client->readiness == Readiness::Unsupported || CMD_ARGC() < 2 ||
            !Number(CMD_ARGV(1), requestedGeneration) || requestedGeneration != generation) {
            return true;
        }
        if (!std::strcmp(command, "cse_wsync")) {
            if (CMD_ARGC() == 2 && !client->catalogStarted && gpGlobals->time >= client->nextResync) {
                client->resyncRequested = true;
                client->nextCatalog = gpGlobals->time;
                client->nextResync = gpGlobals->time + CatalogRetryDelay;
            }
            return true;
        }
        uint32_t argument = 0;
        if (CMD_ARGC() != 3 || !Number(CMD_ARGV(2), argument)) {
            return true;
        }
        if (!std::strcmp(command, "cse_wready")) {
            if (client->catalogSent && argument == digest) {
                client->readiness = Readiness::Ready;
            }
            return true;
        }
        Request request{};
        request.player = player.entindex();
        request.generation = generation;
        request.instance = argument;
        Operate(Operation::Equip, &request, nullptr);
        return true;
    }
    if (requireClient.value && committedDefinitions && !player.IsBot() && !IsReady(player) &&
        (!std::strcmp(command, "jointeam") || !std::strcmp(command, "joinclass") ||
            (!std::strcmp(command, "menuselect") && (player.m_iTeam == UNASSIGNED || player.m_iTeam == SPECTATOR)))) {
        ClientPrint(player.pev, HUD_PRINTCENTER, "Waiting for CSE custom weapon synchronization.");
        return true;
    }
    return false;
}

void ClearAmmo(CBasePlayer& player) {
    if (Client* client = Session(player)) {
        client->ammo.fill(0);
        client->pickup = 0;
    }
}

bool IsCustom(const CBasePlayerItem* item) {
    return dynamic_cast<const CustomWeapon*>(item) != nullptr;
}

int Weight(CBasePlayerItem* item) {
    auto* custom = dynamic_cast<CustomWeapon*>(item);
    return custom ? custom->Specs().weight : item->iWeight();
}

bool BlocksStockPickup(const CBasePlayer& player, int slot) {
    if (SharedSlots() || (slot != PrimarySlot && slot != SecondarySlot)) {
        return false;
    }
    for (const CBasePlayerItem* item = player.m_rgpPlayerItems[slot]; item; item = item->m_pNext) {
        if (IsCustom(item)) {
            return true;
        }
    }
    return false;
}

bool BlocksStockPurchase(const CBasePlayer& player, bool primary) {
    if (SharedSlots()) {
        return false;
    }
    CBasePlayerItem* head = player.m_rgpPlayerItems[primary ? PrimarySlot : SecondarySlot];
    for (CBasePlayerItem* item = head; item; item = item->m_pNext) {
        if (IsCustom(item) && (item != head || !item->CanDrop() || player.m_bIsVIP)) {
            return true;
        }
    }
    return false;
}

bool Drop(CBasePlayer& player, CBasePlayerItem* item, bool death, bool packAmmo) {
    auto* custom = dynamic_cast<CustomWeapon*>(item);
    if (!custom) {
        return false;
    }
    MakeDrop(player, *custom, death, packAmmo);
    return true;
}

const char* DamageWeapon(entvars_t* attacker, entvars_t* inflictor) {
    if (!attacker || currentAttack.attacker != attacker || currentAttack.inflictor != inflictor ||
        !currentAttack.definition) {
        return nullptr;
    }
    const Entry* entry = FindEntry(currentAttack.definition);
    return entry ? entry->definition.presentation.key + WeaponPrefixLength : nullptr;
}

bool DeathNotice(CBasePlayer& victim, entvars_t* killer, entvars_t* inflictor, int killerIndex, bool headshot,
    const char*& logWeapon) {
    const char* customWeapon = DamageWeapon(killer, inflictor);
    if (!customWeapon) {
        return false;
    }
    logWeapon = customWeapon;
    Emit(currentAttack, EventKind::Kill, victim.entindex());
    const Kill kill{generation, ++killSequence, currentAttack.definition, static_cast<uint8_t>(killerIndex),
        static_cast<uint8_t>(victim.entindex()), static_cast<uint8_t>(headshot)};
    for (int i = 1; i <= gpGlobals->maxClients; ++i) {
        CBasePlayer* player = Player(i);
        if (!player || player->IsBot() || (player->pev->flags & FL_DORMANT)) {
            continue;
        }
        if (IsReady(*player)) {
            Send(*player, Encode(kill));
        } else {
            MESSAGE_BEGIN(MSG_ONE, gmsgDeathMsg, nullptr, player->edict());
            WRITE_BYTE(killerIndex);
            WRITE_BYTE(victim.entindex());
            WRITE_BYTE(headshot);
            WRITE_STRING("world");
            MESSAGE_END();
        }
    }
    return true;
}

} // namespace cse::weapons::server

extern "C" UTIL_DLLEXPORT const cse::weapons::Api* CseCustomWeaponsAPI(uint32_t version, uint32_t size) {
    return version == cse::weapons::ApiVersion && size == sizeof(cse::weapons::Api) ?
        &cse::weapons::server::api : nullptr;
}
