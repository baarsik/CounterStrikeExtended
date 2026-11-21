#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cse_custom_weapons.h"
#include "native_module.h"
#include "feature_hooks.h"

namespace {

using namespace cse::weapons;

constexpr int InvalidForward = -1;
constexpr int MaxEventsPerFrame = 64;
constexpr std::size_t MaxDeferredOperations = 64;
constexpr std::size_t MaxPathBytes = 255;
constexpr std::string_view WeaponKeyPrefix = "weapon_";

struct DeferredOperation {
    Operation operation;
    Request request;
    uint32_t playerEpoch;
};

const Api* Backend = nullptr;
bool ProbedActiveBackend = false;
std::vector<DeferredOperation> DeferredOperations;
std::array<Readiness, MaxPlayers + 1> ClientReadiness{};
std::array<uint32_t, MaxPlayers + 1> PlayerEpoch{};
int ReadyForward = InvalidForward;
int EventForward = InvalidForward;
bool MapActive = false;
bool DeferringOperations = false;
bool LoggedUnavailable = false;

bool SameModule(void* first, void* second) {
#if defined(_WIN32)
    constexpr DWORD Flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    HMODULE firstModule = nullptr;
    HMODULE secondModule = nullptr;
    return ::GetModuleHandleExA(Flags, reinterpret_cast<LPCSTR>(first), &firstModule) &&
           ::GetModuleHandleExA(Flags, reinterpret_cast<LPCSTR>(second), &secondModule) &&
           firstModule == secondModule;
#else
    Dl_info firstModule{};
    Dl_info secondModule{};
    return dladdr(first, &firstModule) != 0 && dladdr(second, &secondModule) != 0 &&
           firstModule.dli_fbase == secondModule.dli_fbase;
#endif
}

const Api* GetBackend() {
    if (Backend || ProbedActiveBackend) {
        return Backend;
    }
    if (!gpMetaUtilFuncs || !gpMetaUtilFuncs->pfnGetGameInfo || !gpGamedllFuncs || !MDLL_FUNC || !MDLL_Spawn) {
        return nullptr;
    }

    ProbedActiveBackend = MapActive;
    const char* path = GET_GAME_INFO(PLID, GINFO_DLL_FULLPATH);
    void* module = cse_platform::FindLoadedModule(path);
    void* symbol = cse_platform::GetSymbol(module, ApiSymbol);
    if (!symbol || !SameModule(symbol, reinterpret_cast<void*>(MDLL_Spawn))) {
        return nullptr;
    }

    const Api* api = reinterpret_cast<GetApi>(symbol)(ApiVersion, sizeof(Api));
    if (api && api->version == ApiVersion && api->size == sizeof(Api) && api->registerFile && api->find &&
        api->registrationStatus && api->readiness && api->operate && api->query && api->pollEvent &&
        api->generation && api->definition) {
        Backend = api;
    }
    return Backend;
}

bool HasArguments(const cell* params, int count) {
    return params[0] >= static_cast<cell>(count * sizeof(cell));
}

bool IsPlayer(cell player) {
    return player > 0 && player <= MaxPlayers && player <= gpGlobals->maxClients;
}

bool IsDefinition(cell definition) {
    return definition > 0 && definition <= MaxDefinitions;
}

bool IsPolicy(cell policy) {
    return policy >= static_cast<cell>(SlotPolicy::Reject) && policy <= static_cast<cell>(SlotPolicy::Drop);
}

const char* ReadKey(AMX* amx, cell address) {
    int length = 0;
    const char* key = MF_GetAmxString(amx, address, 0, &length);
    constexpr auto MaxKeyBytes = static_cast<int>(sizeof(Presentation::key) - 1);
    if (!key || length <= static_cast<int>(WeaponKeyPrefix.size()) || length > MaxKeyBytes) {
        return nullptr;
    }
    const std::string_view name(key, static_cast<std::size_t>(length));
    const char first = name[WeaponKeyPrefix.size()];
    if (name.substr(0, WeaponKeyPrefix.size()) != WeaponKeyPrefix || first < 'a' || first > 'z' ||
        !std::all_of(name.begin(), name.end(), [](char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
        })) {
        return nullptr;
    }
    return key;
}

bool DataDirectory(std::string& path) {
    const char* directory = MF_GetLocalInfo("amxx_datadir", "addons/amxmodx/data");
    if (!directory || std::strlen(directory) > MaxPathBytes) {
        return false;
    }

    path = directory;
    std::replace(path.begin(), path.end(), '\\', '/');
    if (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
    const std::string_view segments(path);
    std::size_t start = 0;
    do {
        const std::size_t separator = segments.find('/', start);
        const auto segment = segments.substr(start, separator == segments.npos ? separator : separator - start);
        if (segment.empty() || segment == "." || segment == ".." ||
            segment.find_first_of(":\r\n") != segment.npos) {
            return false;
        }
        if (separator == segments.npos) {
            return true;
        }
        start = separator + 1;
    } while (start <= segments.size());
    return false;
}

cell AMX_NATIVE_CALL Register(AMX* amx, cell* params) {
    const char* basename = HasArguments(params, 1) ? ReadKey(amx, params[1]) : nullptr;
    if (!basename) {
        return static_cast<cell>(Result::InvalidDefinition);
    }
    const auto* api = GetBackend();
    if (!api) {
        if (!LoggedUnavailable) {
            MF_Log("Custom weapons unavailable: the active GameDLL under Metamod did not provide %s v%u",
                   ApiSymbol, ApiVersion);
            LoggedUnavailable = true;
        }
        return static_cast<cell>(Result::BackendUnavailable);
    }
    if (MapActive) {
        return static_cast<cell>(Result::WrongPhase);
    }

    std::string path;
    if (!DataDirectory(path)) {
        MF_Log("Cannot register custom weapon %s: amxx_datadir must be relative to the game directory", basename);
        return static_cast<cell>(Result::InvalidDefinition);
    }
    path += "/csextended/weapons/";
    path += basename;
    path += ".cfg";
    if (path.size() > MaxPathBytes) {
        return static_cast<cell>(Result::InvalidDefinition);
    }

    const auto result = api->registerFile(path.c_str());
    if (result != Result::Ok) {
        const int pluginId = MF_FindScriptByAmx(amx);
        const char* script = pluginId >= 0 ? MF_GetScriptName(pluginId) : nullptr;
        MF_Log("Custom weapon registration failed for %s from %s (result %d)", path.c_str(),
               script ? script : "an AMXX plugin", static_cast<int>(result));
    }
    return static_cast<cell>(result);
}

cell AMX_NATIVE_CALL Version(AMX*, cell*) {
    return GetBackend() ? static_cast<cell>(ApiVersion) : 0;
}

cell AMX_NATIVE_CALL Generation(AMX*, cell*) {
    const auto* api = GetBackend();
    return api ? static_cast<cell>(api->generation()) : 0;
}

cell AMX_NATIVE_CALL Find(AMX* amx, cell* params) {
    const auto* api = GetBackend();
    const char* key = HasArguments(params, 1) ? ReadKey(amx, params[1]) : nullptr;
    return api && key ? api->find(key) : 0;
}

cell AMX_NATIVE_CALL Status(AMX* amx, cell* params) {
    const auto* api = GetBackend();
    if (!api) {
        return static_cast<cell>(Result::BackendUnavailable);
    }
    const char* key = HasArguments(params, 1) ? ReadKey(amx, params[1]) : nullptr;
    return static_cast<cell>(key ? api->registrationStatus(key) : Result::InvalidDefinition);
}

cell AMX_NATIVE_CALL ReadClientReadiness(AMX*, cell* params) {
    const auto* api = GetBackend();
    if (!api) {
        return static_cast<cell>(Result::BackendUnavailable);
    }
    return HasArguments(params, 1) && IsPlayer(params[1]) ? api->readiness(params[1])
                                                         : static_cast<cell>(Result::InvalidPlayer);
}

cell AMX_NATIVE_CALL ReadDefinition(AMX* amx, cell* params) {
    const auto* api = GetBackend();
    if (!api) {
        return static_cast<cell>(Result::BackendUnavailable);
    }
    if (!HasArguments(params, 7) || !IsDefinition(params[1]) || params[3] < 0 || params[5] < 0) {
        return static_cast<cell>(Result::InvalidDefinition);
    }
    if (static_cast<uint32_t>(params[7]) != api->generation()) {
        return static_cast<cell>(Result::WrongPhase);
    }

    Definition definition{};
    const auto result = api->definition(static_cast<uint16_t>(params[1]), &definition);
    if (result != Result::Ok) {
        return static_cast<cell>(result);
    }
    const auto& presentation = definition.presentation;
    MF_SetAmxString(amx, params[2], presentation.key, params[3]);
    MF_SetAmxStringUTF8Char(amx, params[4], presentation.label, std::strlen(presentation.label), params[5]);
    const cell metadata[] = {presentation.slot, presentation.order, presentation.flags, definition.clip,
                             definition.primary.capacity, definition.secondary.capacity, definition.price};
    std::copy(std::begin(metadata), std::end(metadata), MF_GetAmxAddr(amx, params[6]));
    return static_cast<cell>(result);
}

cell AMX_NATIVE_CALL ReadState(AMX* amx, cell* params) {
    const auto* api = GetBackend();
    if (!api) {
        return static_cast<cell>(Result::BackendUnavailable);
    }
    if (!HasArguments(params, 4) || !IsPlayer(params[1])) {
        return static_cast<cell>(Result::InvalidPlayer);
    }
    State state{};
    const auto result = api->query(params[1], &state);
    if (result != Result::Ok) {
        return static_cast<cell>(result);
    }
    if (state.count > MaxOwned) {
        return static_cast<cell>(Result::Blocked);
    }

    const cell fields[] = {static_cast<cell>(state.generation), static_cast<cell>(state.active),
                           static_cast<cell>(state.last), state.clip, state.reserve, state.secondary, state.pickup,
                           state.subject, state.flags, state.zoom, state.count};
    std::copy(std::begin(fields), std::end(fields), MF_GetAmxAddr(amx, params[2]));
    auto* instances = MF_GetAmxAddr(amx, params[3]);
    auto* definitions = MF_GetAmxAddr(amx, params[4]);
    std::fill_n(instances, MaxOwned, 0);
    std::fill_n(definitions, MaxOwned, 0);
    for (std::size_t index = 0; index < state.count; ++index) {
        instances[index] = static_cast<cell>(state.items[index].instance);
        definitions[index] = state.items[index].definition;
    }
    return static_cast<cell>(result);
}

Result Apply(const Api& api, Operation operation, const Request& request, uint32_t* instance) {
    DeferringOperations = true;
    const auto result = api.operate(operation, &request, instance);
    DeferringOperations = false;
    return result;
}

cell Operate(Operation operation, const Request& request, cell* output = nullptr) {
    const auto* api = GetBackend();
    if (!api) {
        return static_cast<cell>(Result::BackendUnavailable);
    }
    if (request.generation == 0 || api->generation() != request.generation) {
        return static_cast<cell>(Result::WrongPhase);
    }
    if (operation != Operation::Spawn && !IsPlayer(request.player)) {
        return static_cast<cell>(Result::InvalidPlayer);
    }
    if (output) {
        *output = 0;
    }
    if (DeferringOperations) {
        if (DeferredOperations.size() == MaxDeferredOperations) {
            return static_cast<cell>(Result::Capacity);
        }
        DeferredOperations.push_back({operation, request, PlayerEpoch[request.player]});
        return static_cast<cell>(Result::Pending);
    }

    uint32_t instance = 0;
    const auto result = Apply(*api, operation, request, &instance);
    if (output && result == Result::Ok) {
        *output = static_cast<cell>(instance);
    }
    return static_cast<cell>(result);
}

cell AMX_NATIVE_CALL Give(AMX* amx, cell* params) {
    if (!HasArguments(params, 5) || !IsDefinition(params[3]) || !IsPolicy(params[5])) {
        return static_cast<cell>(Result::InvalidDefinition);
    }
    Request request{};
    request.player = params[1];
    request.generation = static_cast<uint32_t>(params[2]);
    request.definition = static_cast<uint16_t>(params[3]);
    request.policy = static_cast<SlotPolicy>(params[5]);
    return Operate(Operation::Give, request, MF_GetAmxAddr(amx, params[4]));
}

cell AMX_NATIVE_CALL Spawn(AMX* amx, cell* params) {
    if (!HasArguments(params, 4) || !IsDefinition(params[2])) {
        return static_cast<cell>(Result::InvalidDefinition);
    }
    Request request{};
    request.generation = static_cast<uint32_t>(params[1]);
    request.definition = static_cast<uint16_t>(params[2]);
    const cell* origin = MF_GetAmxAddr(amx, params[3]);
    for (std::size_t axis = 0; axis < std::size(request.origin); ++axis) {
        request.origin[axis] = amx_ctof(origin[axis]);
        if (!std::isfinite(request.origin[axis])) {
            return static_cast<cell>(Result::Blocked);
        }
    }
    return Operate(Operation::Spawn, request, MF_GetAmxAddr(amx, params[4]));
}

template<Operation operation>
cell AMX_NATIVE_CALL OnInstance(AMX*, cell* params) {
    if (!HasArguments(params, 3) || params[3] == 0) {
        return static_cast<cell>(Result::NotOwned);
    }
    Request request{};
    request.player = params[1];
    request.generation = static_cast<uint32_t>(params[2]);
    request.instance = static_cast<uint32_t>(params[3]);
    return Operate(operation, request);
}

cell AMX_NATIVE_CALL GrantAmmo(AMX*, cell* params) {
    if (!HasArguments(params, 5) || params[3] == 0) {
        return static_cast<cell>(Result::NotOwned);
    }
    constexpr auto MaxAmount = static_cast<cell>(UINT16_MAX);
    if (params[4] < 0 || params[4] > MaxAmount) {
        return static_cast<cell>(Result::Blocked);
    }
    Request request{};
    request.player = params[1];
    request.generation = static_cast<uint32_t>(params[2]);
    request.instance = static_cast<uint32_t>(params[3]);
    request.amount = static_cast<uint16_t>(params[4]);
    request.secondary = params[5] != 0;
    return Operate(Operation::GrantAmmo, request);
}

cell AMX_NATIVE_CALL Buy(AMX* amx, cell* params) {
    if (!HasArguments(params, 6) || !IsDefinition(params[3]) || !IsPolicy(params[6])) {
        return static_cast<cell>(Result::InvalidDefinition);
    }
    Request request{};
    request.player = params[1];
    request.generation = static_cast<uint32_t>(params[2]);
    request.definition = static_cast<uint16_t>(params[3]);
    request.transaction = static_cast<uint32_t>(params[4]);
    request.policy = static_cast<SlotPolicy>(params[6]);
    return Operate(Operation::Buy, request, MF_GetAmxAddr(amx, params[5]));
}

AMX_NATIVE_INFO Natives[] = {
    {"cse_weapons_api_version", Version},
    {"cse_weapons_generation", Generation},
    {"cse_weapons_register", Register},
    {"cse_weapons_find", Find},
    {"cse_weapons_registration_status", Status},
    {"cse_weapons_readiness", ReadClientReadiness},
    {"cse_weapons_definition", ReadDefinition},
    {"cse_weapons_state", ReadState},
    {"cse_weapons_give", Give},
    {"cse_weapons_spawn", Spawn},
    {"cse_weapons_equip", OnInstance<Operation::Equip>},
    {"cse_weapons_drop", OnInstance<Operation::Drop>},
    {"cse_weapons_remove", OnInstance<Operation::Remove>},
    {"cse_weapons_grant_ammo", GrantAmmo},
    {"cse_weapons_buy", Buy},
    {nullptr, nullptr}
};

} // namespace

void WeaponsAttach() {
    MF_AddNatives(Natives);
}

void WeaponsPluginsLoaded() {
    ReadyForward = MF_RegisterForward("cse_weapons_ready", ET_IGNORE, FP_CELL, FP_CELL, FP_DONE);
    EventForward = MF_RegisterForward("cse_weapons_event", ET_IGNORE, FP_CELL, FP_CELL, FP_CELL, FP_CELL, FP_CELL,
                                      FP_CELL, FP_CELL, FP_CELL, FP_CELL, FP_DONE);
}

void WeaponsPluginsUnloading() {
    EventForward = InvalidForward;
    ReadyForward = InvalidForward;
    DeferredOperations.clear();
}

void WeaponsServerActivate() {
    MapActive = true;
}

void WeaponsServerDeactivate() {
    MapActive = false;
    Backend = nullptr;
    ProbedActiveBackend = false;
    LoggedUnavailable = false;
    DeferredOperations.clear();
    ClientReadiness.fill(Readiness::Unsupported);
    PlayerEpoch.fill(0);
}

void WeaponsClientLeave(edict_t* entity) {
    const int player = ENTINDEX(entity);
    if (IsPlayer(player)) {
        ClientReadiness[player] = Readiness::Unsupported;
        ++PlayerEpoch[player];
    }
}

void WeaponsStartFrame() {
    const auto* api = GetBackend();
    if (!api) {
        return;
    }
    const auto generation = api->generation();
    const auto requests = std::exchange(DeferredOperations, {});
    for (const auto& queued : requests) {
        if (queued.request.generation == api->generation() &&
            queued.playerEpoch == PlayerEpoch[queued.request.player]) {
            uint32_t instance = 0;
            Apply(*api, queued.operation, queued.request, &instance);
        }
    }

    DeferringOperations = true;
    Event event{};
    for (int count = 0; count < MaxEventsPerFrame && api->pollEvent(&event); ++count) {
        if (EventForward != InvalidForward && event.generation == generation) {
            MF_ExecuteForward(EventForward, static_cast<cell>(event.kind), static_cast<cell>(event.generation),
                              static_cast<cell>(event.instance), static_cast<cell>(event.definition),
                              static_cast<cell>(event.player), static_cast<cell>(event.target),
                              static_cast<cell>(event.clip), static_cast<cell>(event.reserve),
                              static_cast<cell>(event.secondary));
        }
    }
    for (int player = 1; IsPlayer(player); ++player) {
        const auto previous = ClientReadiness[player];
        const auto readiness = static_cast<Readiness>(api->readiness(player));
        ClientReadiness[player] = readiness;
        if (ReadyForward != InvalidForward && readiness == Readiness::Ready && previous != Readiness::Ready) {
            MF_ExecuteForward(ReadyForward, static_cast<cell>(player), static_cast<cell>(generation));
        }
    }
    DeferringOperations = false;
}
