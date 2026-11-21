#include "hud.h"
#include "cl_util.h"
#include "ammohistory.h"
#include "custom_weapon_ammo_layout.h"
#include "custom_weapon_client.h"
#include "demo.h"
#include "demo_api.h"
#include "event_api.h"
#include "pm_shared.h"
#include "weapontype.h"
#include "../../launcher/public/filesystem.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

extern IFileSystem* g_pFileSystem;

namespace cse::weapons {
namespace {

constexpr std::size_t SpriteRoleCount = static_cast<std::size_t>(SpriteRole::Count);
constexpr std::array<std::string_view, SpriteRoleCount> SpriteNames{
    "weapon", "weapon_s", "ammo", "ammo2", "crosshair", "zoom", "death"
};
constexpr unsigned MaxDescriptorBytes = 8192;
constexpr unsigned MaxDescriptorRows = 64;
// The engine has 256 HUD sprite handles shared with the normal HUD.
constexpr std::size_t MaxCustomAtlases = 24;
constexpr std::size_t MaxPickupRows = 4;
constexpr double ResyncInterval = 1.0;

struct CatalogEntry {
    Presentation weapon{};
    std::array<HudSprite, SpriteRoleCount> sprites{};
    bool artLoaded{};
};

struct Catalog {
    uint32_t generation{};
    uint32_t digest{InitialDigest};
    uint16_t count{};
    std::array<CatalogEntry, MaxDefinitions + 1> entries{};
    std::array<uint16_t, MaxDefinitions> order{};
};

struct Pickup {
    uint16_t definition{};
    float expires{};
};

struct Atlas {
    std::string path;
    HSPRITE handle{};
};

Catalog catalog;
Catalog pending;
uint16_t pendingCount;
State observed;
State local;
uint32_t stateSequence;
uint32_t killSequence;
uint32_t recordedGeneration;
uint32_t observedAnimationInstance;
uint32_t observedAnimationSerial;
int observerTarget;
double nextResync;
std::array<Pickup, MaxPickupRows> pickups;
std::array<Atlas, MaxCustomAtlases> atlases;
std::string levelName;

bool IsDemoPlayback() {
    return gEngfuncs.pDemoAPI && gEngfuncs.pDemoAPI->IsPlayingback();
}

bool IsReplacement(uint32_t newer, uint32_t previous) {
    constexpr uint32_t halfSequenceRange = uint32_t{1} << 31;
    return newer && (!previous || (newer != previous && newer - previous < halfSequenceRange));
}

CatalogEntry* FindEntry(uint16_t definition) {
    if (!definition || definition > MaxDefinitions || !catalog.entries[definition].weapon.id) {
        return nullptr;
    }
    return &catalog.entries[definition];
}

int LocalPlayerIndex() {
    const auto* player = gEngfuncs.GetLocalPlayer();
    return player ? player->index : 0;
}

void SendCommand(const char* name, uint32_t generation, uint32_t value) {
    char command[64];
    const int bytes = std::snprintf(command, sizeof(command), "%s %u %u\n", name, generation, value);
    if (bytes > 0 && static_cast<std::size_t>(bytes) < sizeof(command)) {
        ServerCmd(command);
    }
}

void RequestResync() {
    if (IsDemoPlayback() || !catalog.generation || gHUD.m_flTime < nextResync) {
        return;
    }
    nextResync = gHUD.m_flTime + ResyncInterval;
    char command[48];
    const int bytes = std::snprintf(command, sizeof(command), "cse_wsync %u\n", catalog.generation);
    if (bytes > 0 && static_cast<std::size_t>(bytes) < sizeof(command)) {
        ServerCmd(command);
    }
}

std::string ReadDescriptor(const char* path) {
    if (!g_pFileSystem || !path[0]) {
        return {};
    }
    FileHandle_t file = g_pFileSystem->Open(path, "rb");
    if (!file) {
        return {};
    }

    const unsigned length = g_pFileSystem->Size(file);
    std::string descriptor;
    if (length && length <= MaxDescriptorBytes) {
        descriptor.resize(length);
        if (g_pFileSystem->Read(descriptor.data(), static_cast<int>(length), file) != static_cast<int>(length)) {
            descriptor.clear();
        }
    }
    g_pFileSystem->Close(file);
    return descriptor;
}

HSPRITE LoadAtlas(std::string path) {
    if (!IsAssetPath(path, "sprites/", ".spr")) {
        return 0;
    }
    for (auto& atlas : atlases) {
        if (atlas.path == path) {
            return atlas.handle;
        }
        if (atlas.path.empty()) {
            atlas.path = std::move(path);
            if (g_pFileSystem && g_pFileSystem->FileExists(atlas.path.c_str())) {
                atlas.handle = SPR_Load(atlas.path.c_str());
            }
            return atlas.handle;
        }
    }
    return 0;
}

void LoadArt(CatalogEntry& entry) {
    entry.artLoaded = true;
    std::istringstream descriptor(ReadDescriptor(entry.weapon.hud));
    unsigned count = 0;
    if (!(descriptor >> count) || count > MaxDescriptorRows) {
        return;
    }

    const int resolution = ScreenWidth >= 640 ? 640 : 320;
    std::string line;
    std::getline(descriptor, line);
    for (unsigned row = 0; row < count && std::getline(descriptor, line);) {
        const auto comment = line.find("//");
        if (comment != std::string::npos) {
            line.resize(comment);
        }
        std::istringstream fields(line);
        std::string name;
        if (!(fields >> name)) {
            continue;
        }
        ++row;
        const auto role = std::find(SpriteNames.begin(), SpriteNames.end(), name);
        if (role == SpriteNames.end()) {
            continue;
        }

        int res = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string base;
        if (!(fields >> res >> base >> x >> y >> width >> height) ||
            res != resolution || x < 0 || y < 0 || width <= 0 || height <= 0 ||
            width > 512 || height > 512 || x > 1024 || y > 1024) {
            continue;
        }
        std::string path = "sprites/" + base + ".spr";
        const HSPRITE handle = LoadAtlas(std::move(path));
        if (handle && x + width <= SPR_Width(handle, 0) && y + height <= SPR_Height(handle, 0)) {
            entry.sprites[static_cast<std::size_t>(role - SpriteNames.begin())] =
                {handle, {x, x + width, y, y + height}};
        }
    }
}

bool ValidateState(const State& state) {
    const bool isObserver = (state.flags & Observing) != 0;
    if (state.pickup && (isObserver || !FindEntry(state.pickup))) {
        return false;
    }

    for (uint8_t i = 0; i < state.count; ++i) {
        if (!FindEntry(state.items[i].definition)) {
            return false;
        }
    }
    return true;
}

void RememberPickup(uint16_t definition) {
    constexpr float defaultDuration = 5.0f;
    const auto* duration = gEngfuncs.pfnGetCvarPointer("hud_drawhistory_time");
    const float seconds = duration ? duration->value : defaultDuration;
    if (seconds > 0) {
        std::move_backward(pickups.begin(), pickups.end() - 1, pickups.end());
        pickups.front() = {definition, gHUD.m_flTime + seconds};
    }
}

void ApplyPacket(const void* data, int size, bool playback, bool demoBootstrap = false) {
    switch (Identify(data, size)) {
        case Message::Begin: {
            Begin begin{};
            if (Decode(data, size, begin) && begin.generation && begin.count <= MaxDefinitions) {
                pending = {};
                pending.generation = begin.generation;
                pendingCount = begin.count;
                if (const char* level = gEngfuncs.pfnGetLevelName()) {
                    levelName = level;
                }
            }
            break;
        }
        case Message::Definition: {
            CatalogItem item{};
            if (!Decode(data, size, item) || !pending.generation || item.generation != pending.generation) {
                break;
            }
            if (!item.weapon.id || item.weapon.id > MaxDefinitions || item.weapon.slot >= MaxHudSlots ||
                pending.count >= pendingCount || pending.entries[item.weapon.id].weapon.id) {
                pending = {};
                break;
            }
            bool duplicateKey = false;
            for (uint16_t i = 0; i < pending.count; ++i) {
                const auto& previous = pending.entries[pending.order[i]].weapon;
                duplicateKey |= std::strcmp(previous.key, item.weapon.key) == 0;
            }
            if (duplicateKey) {
                pending = {};
                break;
            }
            pending.entries[item.weapon.id].weapon = item.weapon;
            pending.order[pending.count++] = item.weapon.id;
            pending.digest = UpdateDigest(pending.digest, item);
            break;
        }
        case Message::End: {
            End end{};
            if (!Decode(data, size, end) || !pending.generation || end.generation != pending.generation) {
                break;
            }
            if (pending.count != pendingCount || end.digest != pending.digest) {
                pending = {};
                break;
            }
            if (catalog.generation != end.generation || catalog.digest != end.digest) {
                catalog = pending;
                local = {};
                observed = {};
                pickups = {};
                stateSequence = 0;
                killSequence = 0;
                nextResync = 0;
            }
            pending = {};
            if (!playback) {
                SendCommand("cse_wready", end.generation, end.digest);
            }
            break;
        }
        case Message::State: {
            State state{};
            if (!Decode(data, size, state) || !catalog.generation || state.generation != catalog.generation ||
                !IsReplacement(state.sequence, stateSequence)) {
                break;
            }
            if (!ValidateState(state)) {
                RequestResync();
                break;
            }
            const bool observing = (state.flags & Observing) != 0;
            if (!observing && !playback && state.subject != LocalPlayerIndex()) {
                break;
            }
            stateSequence = state.sequence;
            if (observing) {
                observed = state;
            } else {
                observed = {};
                local = state;
                if (demoBootstrap && state.active && state.animationSerial && gEngfuncs.pEventAPI) {
                    gEngfuncs.pEventAPI->EV_WeaponAnimation(state.animation, 0);
                }
                if (state.pickup) {
                    RememberPickup(state.pickup);
                }
            }
            break;
        }
        case Message::Kill: {
            Kill kill{};
            if (!Decode(data, size, kill) || !catalog.generation || kill.generation != catalog.generation ||
                !IsReplacement(kill.sequence, killSequence) || !FindEntry(kill.definition) ||
                kill.killer > gEngfuncs.GetMaxClients() || kill.victim > gEngfuncs.GetMaxClients() || !kill.victim) {
                break;
            }
            killSequence = kill.sequence;
            const auto sprite = WeaponSprite(kill.definition, SpriteRole::Death);
            gHUD.m_DeathNotice.ShowCustomDeath(kill.killer, kill.victim, kill.headshot, sprite.handle, sprite.rect,
                                                  WeaponPresentation(kill.definition)->label);
            break;
        }
        default:
            break;
    }
}

int ReceiveWeaponMessage(const char*, int size, void* data) {
    ApplyPacket(data, size, IsDemoPlayback());
    return 1;
}

void WriteDemoPacket(Packet packet) {
    if (packet.size > 0) {
        Demo_WriteBuffer(TYPE_CUSTOM_WEAPON, packet.size, packet.bytes);
    }
}

void WriteDemoSnapshot() {
    WriteDemoPacket(Encode(Begin{catalog.generation, catalog.count}));
    for (uint16_t i = 0; i < catalog.count; ++i) {
        const auto& entry = catalog.entries[catalog.order[i]];
        WriteDemoPacket(Encode(CatalogItem{catalog.generation, entry.weapon}));
    }
    WriteDemoPacket(Encode(End{catalog.generation, catalog.digest}));
    const State* current = g_iUser1 == OBS_IN_EYE && observed.subject == g_iUser2 ? &observed : LocalWeapons();
    if (current && current->sequence) {
        State snapshot = *current;
        snapshot.pickup = 0;
        WriteDemoPacket(Encode(snapshot));
    }
}

void DrawIcon(const HudSprite& sprite, int x, int y, int r, int g, int b) {
    if (sprite.handle) {
        SPR_Set(sprite.handle, r, g, b);
        SPR_DrawAdditive(0, x, y, &sprite.rect);
    }
}

HudSprite AmmoIcon(uint16_t definition, SpriteRole role) {
    const auto icon = WeaponSprite(definition, role);
    if (icon.handle) {
        return icon;
    }
    const WEAPON* fallback = gWR.GetWeapon(role == SpriteRole::SecondaryAmmo ? WEAPON_M3 : WEAPON_AK47);
    return {fallback->hAmmo, fallback->rcAmmo};
}

struct AmmoHud {
    int digitWidth;
    HudSprite primary;
    HudSprite secondary;
    AmmoHudRows rows;
};

AmmoHud PrepareAmmoHud(const Presentation& weapon) {
    const int digitWidth = gHUD.GetSpriteWidth(gHUD.m_HUD_number_0);
    const auto primary = AmmoIcon(weapon.id, SpriteRole::Ammo);
    const auto secondary = (weapon.flags & HasSecondary) ? AmmoIcon(weapon.id, SpriteRole::SecondaryAmmo) : HudSprite{};
    return {digitWidth, primary, secondary, AmmoHudLayout(ScreenWidth, ScreenHeight, gHUD.m_iFontHeight, digitWidth,
                                                          primary.Height(), secondary.Height())};
}

int DrawAmmoCount(int right, int y, int digitWidth, uint16_t count, int r, int g, int b) {
    constexpr int hudNumberColumns = 5;
    gHUD.DrawHudNumber(right - hudNumberColumns * digitWidth, y, count, r, g, b);
    return right - AmmoDigits(count) * digitWidth;
}

} // namespace

void RegisterWeaponClient() {
    ResetWeaponSession();
    gEngfuncs.PlayerInfo_SetValueForKey(CapabilityKey, CapabilityVersion);
    gEngfuncs.pfnHookUserMsg(const_cast<char*>(UserMessage), ReceiveWeaponMessage);
}

void ResetWeaponSession() {
    catalog = {};
    pending = {};
    local = {};
    observed = {};
    pickups = {};
    atlases = {};
    stateSequence = 0;
    killSequence = 0;
    nextResync = 0;
    levelName.clear();
    recordedGeneration = 0;
    observerTarget = 0;
    observedAnimationInstance = 0;
    observedAnimationSerial = 0;
}

void RefreshWeaponArt() {
    atlases = {};
    for (auto& entry : catalog.entries) {
        entry.artLoaded = false;
        entry.sprites = {};
    }
}

void RunWeaponClientFrame() {
    if (!levelName.empty()) {
        const char* current = gEngfuncs.pfnGetLevelName();
        if (!current || levelName != current) {
            ResetWeaponSession();
        }
    }
    const State* view = ActiveWeaponView();
    if (view && (view->flags & Observing) && view->animationSerial && gEngfuncs.pEventAPI) {
        if (view->active != observedAnimationInstance || view->animationSerial != observedAnimationSerial) {
            observedAnimationInstance = view->active;
            observedAnimationSerial = view->animationSerial;
            gEngfuncs.pEventAPI->EV_WeaponAnimation(view->animation, 0);
        }
    } else {
        observedAnimationInstance = 0;
        observedAnimationSerial = 0;
    }
    if (!gEngfuncs.pDemoAPI || !gEngfuncs.pDemoAPI->IsRecording()) {
        recordedGeneration = 0;
        return;
    }
    if (catalog.generation && recordedGeneration != catalog.generation) {
        WriteDemoSnapshot();
        recordedGeneration = catalog.generation;
    }
}

void ReadWeaponDemoPacket(const void* data, int size) {
    ApplyPacket(data, size, true, true);
}

const State* LocalWeapons() {
    return local.generation ? &local : nullptr;
}

const State* ActiveWeaponView() {
    const int target = g_iUser1 == OBS_IN_EYE ? g_iUser2 : 0;
    if (target != observerTarget) {
        if (observed.subject == observerTarget) {
            observed = {};
        }
        observerTarget = target;
    }
    if (target) {
        return observed.generation && observed.subject == target && observed.active ? &observed : nullptr;
    }
    return !g_iUser1 && local.active && !gHUD.m_fPlayerDead ? &local : nullptr;
}

const Presentation* WeaponPresentation(uint16_t definition) {
    const auto* entry = FindEntry(definition);
    return entry ? &entry->weapon : nullptr;
}

const Presentation* ActiveWeaponPresentation(const State& state) {
    for (uint8_t i = 0; i < state.count; ++i) {
        if (state.items[i].instance == state.active) {
            return WeaponPresentation(state.items[i].definition);
        }
    }
    return nullptr;
}

HudSprite WeaponSprite(uint16_t definition, SpriteRole role) {
    auto* entry = FindEntry(definition);
    if (!entry || !entry->weapon.hud[0]) {
        return {};
    }
    if (!entry->artLoaded) {
        LoadArt(*entry);
    }
    return entry->sprites[static_cast<std::size_t>(role)];
}

void SelectCustomWeapon(uint32_t instance) {
    if (IsDemoPlayback() || !catalog.generation || !instance) {
        return;
    }
    for (uint8_t i = 0; i < local.count; ++i) {
        if (local.items[i].instance == instance) {
            SendCommand("cse_wselect", catalog.generation, instance);
            return;
        }
    }
}

void DrawCustomPickups(float time) {
    constexpr float fadeTime = 1.0f;
    int y = ScreenHeight / 2;
    for (const auto& pickup : pickups) {
        const auto* definition = WeaponPresentation(pickup.definition);
        if (!definition || time >= pickup.expires) {
            continue;
        }
        const auto sprite = WeaponSprite(pickup.definition, SpriteRole::Weapon);
        const float brightness = (std::min)(fadeTime, pickup.expires - time);
        const int inset = gHUD.m_iFontHeight;
        if (sprite.handle) {
            DrawIcon(sprite, ScreenWidth - inset - sprite.Width(), y,
                     static_cast<int>(255 * brightness), static_cast<int>(160 * brightness), 0);
        } else {
            const int width = ConsoleStringLen(definition->label);
            gEngfuncs.pfnDrawSetTextColor(brightness, 0.625f * brightness, 0.0f);
            DrawConsoleString(ScreenWidth - inset - width, y, definition->label);
        }
        y += (std::max)(gHUD.m_iFontHeight, sprite.Height()) + inset / 4;
    }
}

void DrawCustomAmmo(const State& state, const Presentation& weapon) {
    constexpr int PrimaryRed = 255;
    constexpr int PrimaryGreen = 160;
    constexpr int PrimaryBlue = 0;
    constexpr int SecondaryRed = 64;
    constexpr int SecondaryGreen = 160;
    constexpr int SecondaryBlue = 255;

    const auto [digitWidth, primary, secondary, layout] = PrepareAmmoHud(weapon);
    const int primaryX = layout.rightEdge - primary.Width();
    const int reserveX = DrawAmmoCount(primaryX, layout.primaryY, digitWidth, state.reserve,
                                      PrimaryRed, PrimaryGreen, PrimaryBlue);
    DrawIcon(primary, primaryX, layout.primaryY - primary.Height() / 8, PrimaryRed, PrimaryGreen, PrimaryBlue);
    if (!(state.flags & NoClip)) {
        const int gap = digitWidth / 2;
        const int lineWidth = (std::max)(1, digitWidth / 10);
        const int lineX = reserveX - gap - lineWidth;
        FillRGBA(lineX, layout.primaryY, lineWidth, gHUD.m_iFontHeight, PrimaryRed, PrimaryGreen, PrimaryBlue, 255);
        DrawAmmoCount(lineX - gap, layout.primaryY, digitWidth, state.clip, PrimaryRed, PrimaryGreen, PrimaryBlue);
    }
    if (weapon.flags & HasSecondary) {
        const int secondaryX = layout.rightEdge - secondary.Width();
        DrawAmmoCount(secondaryX, layout.secondaryY, digitWidth, state.secondary,
                      SecondaryRed, SecondaryGreen, SecondaryBlue);
        DrawIcon(secondary, secondaryX, layout.secondaryY - secondary.Height() / 8,
                 SecondaryRed, SecondaryGreen, SecondaryBlue);
    }
}

int MoneyYAboveSecondaryAmmo(int stockY) {
    if (gHUD.m_iIntermission || g_iUser1 || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL))) {
        return stockY;
    }
    const auto* state = ActiveWeaponView();
    const auto* weapon = state ? ActiveWeaponPresentation(*state) : nullptr;
    if (!weapon || !(weapon->flags & HasSecondary)) {
        return stockY;
    }
    const auto ammo = PrepareAmmoHud(*weapon);
    return MoneyHudY(stockY, &ammo.rows);
}

} // namespace cse::weapons
