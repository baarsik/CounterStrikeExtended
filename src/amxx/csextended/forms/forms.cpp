#include "session.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

#include "feature_hooks.h"

#ifdef snprintf
#undef snprintf
#endif
#ifdef min
#undef min
#endif

namespace {

using namespace cse::forms;
using namespace cse::forms::server;

constexpr int MaxClients = 32; // GoldSrc client slots, excluding the world entity.
constexpr int VariableMessageSize = -1;
constexpr int InvalidForward = -1;
constexpr int RejectReasonBytes = 128;
constexpr std::size_t HexDigitsPerByte = 2;
constexpr int MaxGameCommandBytes = 127; // GoldSrc truncates commands delivered to pfnClientCommand.
constexpr int MaxControlIndexDigits = 2;
constexpr int ActionArgumentSeparators = 3;
constexpr char HelloCommand[] = "cseform_hello";
constexpr char AssetCommand[] = "cseform_asset";
constexpr char ActionCommand[] = "cseform_action";
constexpr char NoValue[] = "-";
constexpr char RequireClientCvar[] = "cse_forms_require_client";

static_assert(kMaxControls < 100);
static_assert(sizeof(ActionCommand) - 1 + ActionArgumentSeparators +
              std::numeric_limits<std::uint32_t>::digits10 + 1 + MaxControlIndexDigits +
              kMaxValueBytes * HexDigitsPerByte <= MaxGameCommandBytes);

struct Player {
    Session session;
    bool capable = false;
};

std::array<Player, MaxClients + 1> Players;
Bundle Assets;
std::size_t AssetBytes = 0;
enginefuncs_t* HookedEngine = nullptr;
int UserMessageId = 0;
int ActionForward = InvalidForward;
bool MapActive = false;
cvar_t RequireClient = {const_cast<char*>(RequireClientCvar), const_cast<char*>("0"), 0, 0.0f, nullptr};

Player* GetPlayer(int index) {
    if (index < 1 || index > gpGlobals->maxClients || index > MaxClients) {
        return nullptr;
    }
    return &Players[index];
}

Player* GetPlayingPlayer(int index) {
    Player* player = GetPlayer(index);
    if (!player || !player->capable || !MF_IsPlayerIngame(index) || MF_IsPlayerBot(index) || MF_IsPlayerHLTV(index)) {
        return nullptr;
    }
    return player;
}

void Reset(Player& player) {
    player.session.Reset();
    player.capable = false;
}

void RegisterMessage() {
    UserMessageId = 0;
    if (!HookedEngine) {
        MF_Log("Server forms disabled: Metamod hook table is unavailable");
        return;
    }
    int size = 0;
    UserMessageId = GET_USER_MSG_ID(PLID, kUserMessage, &size);
    if (UserMessageId != 0 && size != VariableMessageSize) {
        UserMessageId = 0;
        MF_Log("Server forms disabled: existing %s has incompatible size %d", kUserMessage, size);
        return;
    }
    if (UserMessageId == 0) {
        UserMessageId = HookedEngine->pfnRegUserMsg(kUserMessage, VariableMessageSize);
    }
    if (UserMessageId == 0) {
        MF_Log("Server forms disabled: user-message registration failed");
    }
}

std::string_view PawnName(AMX* amx, cell address) {
    int length = 0;
    const char* name = MF_GetAmxString(amx, address, 0, &length);
    return name && length > 0 ? std::string_view(name, static_cast<std::size_t>(length)) : std::string_view{};
}

bool Precache(std::string_view name, AssetKind kind) {
    if (MapActive || !IsName(name)) {
        return false;
    }
    const bool form = kind == AssetKind::Form;
    for (const auto& asset : Assets) {
        if (asset.name == name) {
            return form ? asset.kind == AssetKind::Form
                        : IsImage(asset.kind) && IsPersistentImage(asset.kind) == IsPersistentImage(kind);
        }
    }
    if (Assets.size() >= kMaxAssets || AssetBytes >= kMaxBundleBytes) {
        return false;
    }

    const std::string identifier(name);
    const char* suffix = form ? ".form" : ".rgba";
    const char* directory = MF_GetLocalInfo("amxx_datadir", "addons/amxmodx/data");
    const auto open = [&](const char* extension) {
        const char* path = MF_BuildPathname("%s/csextended/forms/%s%s", directory, identifier.c_str(), extension);
        return std::fopen(path, "rb");
    };
    std::FILE* file = open(suffix);
    if (!file && !form && errno == ENOENT) {
        suffix = ".webp";
        file = open(suffix);
        kind = IsPersistentImage(kind) ? AssetKind::PersistentWebPImage : AssetKind::WebPImage;
    }
    if (!file) {
        MF_Log("Unable to open server forms asset %s%s", identifier.c_str(), suffix);
        return false;
    }
    const auto limit = std::min<std::size_t>(form ? kMaxFormBytes : kMaxImageBytes, kMaxBundleBytes - AssetBytes);
    std::string bytes(limit + 1, '\0');
    const std::size_t count = std::fread(bytes.data(), 1, bytes.size(), file);
    const bool readError = std::ferror(file) != 0;
    std::fclose(file);
    if (readError || count == 0 || count > limit) {
        MF_Log("Unreadable or oversized server forms asset %s%s", identifier.c_str(), suffix);
        return false;
    }
    bytes.resize(count);

    Form layout;
    Image image;
    if (!(form ? ParseForm(bytes, layout) : ParseImage(kind, bytes, image))) {
        MF_Log("Invalid server forms asset %s%s", identifier.c_str(), suffix);
        return false;
    }
    const auto digest = Hash(bytes);
    Assets.push_back({identifier, kind, std::move(bytes), digest, std::move(layout)});
    AssetBytes += count;
    return true;
}

cell AMX_NATIVE_CALL PrecacheForm(AMX* amx, cell* params) {
    return params[0] >= static_cast<cell>(sizeof(cell)) && Precache(PawnName(amx, params[1]), AssetKind::Form);
}

cell AMX_NATIVE_CALL PrecacheImage(AMX* amx, cell* params) {
    if (params[0] < static_cast<cell>(sizeof(cell))) {
        return 0;
    }
    const bool persistent = params[0] < static_cast<cell>(2 * sizeof(cell)) || params[2] != 0;
    return Precache(PawnName(amx, params[1]), persistent ? AssetKind::PersistentImage : AssetKind::Image);
}

cell AMX_NATIVE_CALL Show(AMX* amx, cell* params) {
    Player* player = params[0] >= static_cast<cell>(2 * sizeof(cell)) ? GetPlayingPlayer(params[1]) : nullptr;
    return player && player->session.Show(Assets, PawnName(amx, params[2]));
}

cell AMX_NATIVE_CALL Hide(AMX*, cell* params) {
    Player* player = params[0] >= static_cast<cell>(sizeof(cell)) ? GetPlayingPlayer(params[1]) : nullptr;
    return player && player->session.Hide();
}

cell AMX_NATIVE_CALL Ready(AMX*, cell* params) {
    Player* player = params[0] >= static_cast<cell>(sizeof(cell)) ? GetPlayingPlayer(params[1]) : nullptr;
    return player && player->session.Ready(Assets);
}

AMX_NATIVE_INFO Natives[] = {
    {"cse_forms_precache_form", PrecacheForm},
    {"cse_forms_precache_image", PrecacheImage},
    {"cse_forms_show", Show},
    {"cse_forms_hide", Hide},
    {"cse_forms_ready", Ready},
    {nullptr, nullptr},
};

template <typename Integer>
bool ReadUnsigned(int argument, Integer& out) {
    const std::string_view text = g_engfuncs.pfnCmd_Argv(argument);
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

void ReceiveAction(Session& session, int index) {
    std::uint32_t token = 0;
    unsigned control = 0;
    if (!ReadUnsigned(1, token) || !ReadUnsigned(2, control)) {
        return;
    }
    const std::string_view encoded = g_engfuncs.pfnCmd_Argv(3);
    std::array<char, kMaxValueBytes + 1> value{};
    std::size_t length = 0;
    if (encoded != NoValue) {
        if (encoded.empty() || encoded.size() > kMaxValueBytes * HexDigitsPerByte ||
            encoded.size() % HexDigitsPerByte != 0) {
            return;
        }
        length = encoded.size() / HexDigitsPerByte;
        if (!DecodeHex(encoded, reinterpret_cast<std::uint8_t*>(value.data()), length)) {
            return;
        }
    }
    const Asset* asset = session.Action(Assets, token, control, {value.data(), length}, gpGlobals->time);
    if (asset && ActionForward != InvalidForward) {
        MF_ExecuteForward(ActionForward, static_cast<cell>(index), asset->name.c_str(),
                          static_cast<cell>(control), value.data());
    }
}

} // namespace

void FormsAttach() {
    MF_AddNatives(Natives);
    g_engfuncs.pfnCVarRegister(&RequireClient);
    GET_HOOK_TABLES(PLID, &HookedEngine, nullptr, nullptr);
}

void FormsPluginsLoaded() {
    ActionForward = MF_RegisterForward("cse_forms_action", ET_IGNORE, FP_CELL, FP_STRING, FP_CELL, FP_STRING, FP_DONE);
}

void FormsPluginsUnloading() {
    ActionForward = InvalidForward;
}

void FormsServerActivate() {
    for (auto& player : Players) {
        Reset(player);
    }
    RegisterMessage();
    MapActive = true;
}

void FormsServerDeactivate() {
    MapActive = false;
    UserMessageId = 0;
    Assets.clear();
    AssetBytes = 0;
    for (auto& player : Players) {
        Reset(player);
    }
}

BOOL FormsClientConnect(edict_t* entity, const char*, const char*, char rejectReason[RejectReasonBytes]) {
    Player* player = GetPlayer(ENTINDEX(entity));
    if (!player) {
        RETURN_META_VALUE(MRES_IGNORED, TRUE);
    }
    Reset(*player);
    if ((entity->v.flags & FL_FAKECLIENT) != 0) {
        RETURN_META_VALUE(MRES_IGNORED, TRUE);
    }
    char* info = g_engfuncs.pfnGetInfoKeyBuffer(entity);
    const char* capability = info ? g_engfuncs.pfnInfoKeyValue(info, kCapabilityKey) : nullptr;
    player->capable = capability && std::strcmp(capability, kCapabilityVersion) == 0;
    if (g_engfuncs.pfnCVarGetFloat(RequireClientCvar) != 0.0f && !player->capable) {
        std::snprintf(rejectReason, RejectReasonBytes,
                      "This server requires a CounterStrikeExtended client with server forms support.");
        RETURN_META_VALUE(MRES_SUPERCEDE, FALSE);
    }
    RETURN_META_VALUE(MRES_IGNORED, TRUE);
}

void FormsClientEnter(edict_t* entity) {
    Player* player = GetPlayer(ENTINDEX(entity));
    if (UserMessageId != 0 && player && player->capable && (entity->v.flags & (FL_FAKECLIENT | FL_PROXY)) == 0) {
        player->session.Begin();
    }
}

void FormsClientLeave(edict_t* entity) {
    if (Player* player = GetPlayer(ENTINDEX(entity))) {
        Reset(*player);
    }
}

void FormsClientCommand(edict_t* entity) {
    const std::string_view command = g_engfuncs.pfnCmd_Argv(0);
    if (command != HelloCommand && command != AssetCommand && command != ActionCommand) {
        RETURN_META(MRES_IGNORED);
    }
    const int index = ENTINDEX(entity);
    if (Player* player = GetPlayingPlayer(index)) {
        const int count = g_engfuncs.pfnCmd_Argc();
        if (command == HelloCommand && count == 2) {
            player->session.AcceptHello(g_engfuncs.pfnCmd_Argv(1));
        } else if (command == AssetCommand && count == 3) {
            unsigned id = 0;
            if (ReadUnsigned(1, id)) {
                player->session.AcceptAsset(Assets, id, g_engfuncs.pfnCmd_Argv(2));
            }
        } else if (command == ActionCommand && count == 4) {
            ReceiveAction(player->session, index);
        }
    }
    RETURN_META(MRES_SUPERCEDE);
}

void FormsStartFrame() {
    if (UserMessageId != 0) {
        const int last = std::min(gpGlobals->maxClients, MaxClients);
        for (int index = 1; index <= last; ++index) {
            Player* player = GetPlayingPlayer(index);
            edict_t* entity = player ? MF_GetPlayerEdict(index) : nullptr;
            if (!entity || entity->free) {
                continue;
            }
            const auto message = player->session.Tick(Assets, gpGlobals->time);
            if (message.size != 0) {
                HookedEngine->pfnMessageBegin(MSG_ONE, UserMessageId, nullptr, entity);
                for (std::size_t byte = 0; byte < message.size; ++byte) {
                    HookedEngine->pfnWriteByte(message.bytes[byte]);
                }
                HookedEngine->pfnMessageEnd();
            }
        }
    }
}
