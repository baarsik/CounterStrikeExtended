#define CSE_ABI_ENGINE_VIEW
#include "fixture_types.h"
#include "../../../src/common/client_module_contracts.h"
#include "../../../src/common/client_module_entry.h"
#include "../../../src/public/native_module.h"
#include "../strings/high_offset_buffer.h"
#include <array>
#include <cstdarg>
#include <new>
#include <vector>

#define REQUIRE(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Client ABI failure at %d: %s\n", __LINE__, #condition); return 1; \
} } while (false)

struct HostData {
    cl_enginefunc_t functions;
    cldll_func_t exports;
    modfuncs_t modules;
    cldll_func_dst_t destinations;
    engine_studio_api_t studio;
    player_info_t players[2];
    hull_t hull;
    char directory[32];
    int mouseOutputs[4];
};
HostData* host = nullptr;
int printCalls = 0;
int traceCalls = 0;
int mouseCalls = 0;
int MousePositionAndCenter(int *x, int *y, int *centerX, int *centerY) {
    if (x != &host->mouseOutputs[0] || y != &host->mouseOutputs[1] || centerX != &host->mouseOutputs[2]
        || centerY != &host->mouseOutputs[3]) {
        std::abort();
    }
    ++mouseCalls;
    *x = -17;
    *y = 37;
    *centerX = 640;
    *centerY = 480;
    return 1;
}
const char* Directory() {
    return host->directory;
}
int ExpandFilename(const char* name, char* output, int bytes) {
    if (std::strcmp(name, "filesystem_stdio.dll") != 0) {
        return 0;
    }
    const std::size_t count = std::strlen(name) + 1;
    if (count > static_cast<std::size_t>(bytes)) {
        std::abort();
    }
    std::memcpy(output, name, count);
    return 1;
}
player_info_t* PlayerInfo(int index) {
    return index == 1 ? &host->players[1] : nullptr;
}
pmtrace_t Trace(float*, float*, int flags, int ignored) {
    if (flags != 7 || ignored != 11) {
        std::abort();
    }
    ++traceCalls;
    pmtrace_t result = {};
    result.endpos[0] = 12.5f;
    result.fraction = 0.5f;
    return result;
}
hull_t* Hull(physent_t*, float* offset) {
    offset[2] = 9;
    return &host->hull;
}
void Print(const char* format, ...) {
    if (std::strcmp(format, "owned %d %.1f") != 0) {
        std::abort();
    }
    va_list arguments;
    va_start(arguments, format);
    const int number = va_arg(arguments, int);
    const double scalar = va_arg(arguments, double);
    va_end(arguments);
    if (number != 42 || scalar != 2.5) {
        std::abort();
    }
    ++printCalls;
}

class MovementStorage {
public:
    MovementStorage() {
#if UINTPTR_MAX > UINT32_MAX
        constexpr std::uintptr_t FirstHint = UINT64_C(64) << 30;
        constexpr unsigned int Attempts = 8;
        for (unsigned int attempt = 0; attempt < Attempts; ++attempt) {
            void* candidate = Map(reinterpret_cast<void*>(FirstHint << attempt));
            if (reinterpret_cast<std::uintptr_t>(candidate) > UINT32_MAX) {
                memory = candidate;
                break;
            }
            if (candidate) {
                Unmap(candidate);
            }
        }
#else
        memory = Map(nullptr);
#endif
    }
    ~MovementStorage() {
        if (memory) {
            Unmap(memory);
        }
    }
    void* memory = nullptr;
private:
    static void* Map(void* hint) {
#if defined(_WIN32)
        return VirtualAlloc(hint, sizeof(playermove_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        void* result = mmap(hint, sizeof(playermove_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        return result == MAP_FAILED ? nullptr : result;
#endif
    }
    static void Unmap(void* pointer) {
#if defined(_WIN32)
        VirtualFree(pointer, 0, MEM_RELEASE);
#else
        munmap(pointer, sizeof(playermove_t));
#endif
    }
};

template<typename Type>
Type Symbol(void* module, const char* name) {
    return reinterpret_cast<Type>(cse_platform::GetSymbol(module, name));
}

int main(int argc, char** argv) {
    using namespace cse::abi;
    REQUIRE(argc == 3);
    const bool accept = std::strcmp(argv[2], "accept") == 0;
    void* module = cse_platform::LoadModule(argv[1], cse_platform::ModuleFallback::None);
    REQUIRE(module != nullptr);
    const auto describe = Symbol<GetModuleAbi>(module, "CseGetModuleAbi");
    const auto initialize = Symbol<CseInitializeClient>(module, "CseInitializeClientModule");
    const auto exchange = Symbol<CseExchangeClient>(module, "CseExchangeClientFunctions");
    const auto studio = Symbol<CseClientStudio>(module, "CseGetClientStudioInterface");
    const auto effects = Symbol<int (*)()>(module, "CseFixtureEffects");
    const auto exchanges = Symbol<int (*)()>(module, "CseFixtureExchanges");
    REQUIRE(effects && effects() == 0 && exchanges && exchanges() == 0);
    const auto size = Symbol<std::size_t (*)(unsigned int)>(module, "CseFixtureTableSize");
    REQUIRE(size);
    REQUIRE(size(static_cast<unsigned int>(ClientContract::EngineFunctions)) == sizeof(cl_enginefunc_t));
    REQUIRE(size(static_cast<unsigned int>(ClientContract::NestedFunctions)) == sizeof(event_api_t));
    REQUIRE(size(static_cast<unsigned int>(ClientContract::ExchangeFunctions)) == sizeof(cldll_func_dst_t));
    const auto status = CheckRequired(ClientModuleDescriptor, describe ? describe() : nullptr);
    if (!accept) {
        REQUIRE(status != Compatibility::Compatible || !initialize || !exchange || !studio);
        REQUIRE(effects() == 0 && exchanges() == 0);
        cse_platform::UnloadModule(module);
        REQUIRE(!cse_platform::FindLoadedModule(argv[1]));
        return 0;
    }
    REQUIRE(status == Compatibility::Compatible && initialize && exchange && studio);
    HighOffsetBuffer storage(nullptr);
    REQUIRE(storage.get() && sizeof(HostData) <= 65536);
    host = new (storage.get()) HostData{};
    const auto hostAddress = reinterpret_cast<std::uintptr_t>(host);
    if (sizeof(void*) > sizeof(std::uint32_t)) {
        REQUIRE(hostAddress > UINT32_MAX);
    }
    std::strcpy(host->directory, "owned-high-directory");
    host->functions.pfnGetGameDirectory = Directory;
    host->functions.COM_ExpandFilename = ExpandFilename;
    host->functions.pfnGetClientMousePositionAndCenter = MousePositionAndCenter;
    host->modules.m_nVersion = 73;
    host->studio.PlayerInfo = PlayerInfo;
    host->hull.clip_mins[1] = 23.5f;
    std::memset(host->players[1].hashedcdkey, 0xA5, sizeof(host->players[1].hashedcdkey));
    host->players[1].m_nSteamID = UINT64_C(0xFEDCBA9876543210);
    const auto legacyInitialize = Symbol<INITIALIZE_FUNC>(module, "Initialize");
    const auto legacyF = Symbol<void (*)(void*)>(module, "F");
    const auto legacyStudio = Symbol<HUD_STUDIO_INTERFACE_FUNC>(module, "HUD_GetStudioModelInterface");
    REQUIRE(legacyInitialize && legacyF && legacyStudio);
    REQUIRE(!legacyInitialize(reinterpret_cast<cl_enginefunc_t*>(1), CLDLL_INTERFACE_VERSION));
    std::array<unsigned char, sizeof(cldll_func_t)> untouched;
    untouched.fill(0xA5);
    legacyF(untouched.data());
    for (unsigned char byte : untouched) {
        REQUIRE(byte == 0xA5);
    }
    r_studio_interface_t* renderer = reinterpret_cast<r_studio_interface_t*>(1);
    REQUIRE(!legacyStudio(STUDIO_INTERFACE_VERSION, &renderer, reinterpret_cast<engine_studio_api_t*>(1)));
    REQUIRE(renderer == reinterpret_cast<r_studio_interface_t*>(1) && effects() == 0);
    for (int mutation = 0; mutation < 8; ++mutation) {
        auto invalid = EngineClientDescriptor;
        if (mutation == 0) {
            --invalid.runtimeVersion;
        } else if (mutation == 1) {
            invalid.tables[0].signature = static_cast<std::uint64_t>(ClientRole::Client);
        } else if (mutation == 2) {
            invalid.tables[1].signature ^= 1;
        } else if (mutation == 3) {
            invalid.tables[2].signature ^= 1;
        } else if (mutation == 4) {
            invalid.tables[invalid.tableCount++] = invalid.tables[0];
        } else if (mutation == 5) {
            invalid.tables[invalid.tableCount++] = {0x7FFF, 1, 0};
        } else if (mutation == 6) {
            invalid.tableCount = static_cast<std::uint32_t>(MaxTables + 1);
        } else {
            ++invalid.runtimeVersion;
        }
        REQUIRE(!initialize(&invalid, reinterpret_cast<cl_enginefunc_t*>(1), CLDLL_INTERFACE_VERSION));
        REQUIRE(!exchange(&invalid, reinterpret_cast<cldll_func_t*>(1)));
        REQUIRE(effects() == 0);
    }
    auto offeredEngine = EngineClientDescriptor;
    std::swap(offeredEngine.tables[0], offeredEngine.tables[offeredEngine.tableCount - 1]);
    offeredEngine.tables[offeredEngine.tableCount++] = {0x7FFF, 0, 7};
    host->exports.pInitFunc = reinterpret_cast<INITIALIZE_FUNC>(&host->modules);
    host->exports.pHudVidInitFunc = reinterpret_cast<HUD_VIDINIT_FUNC>(&host->destinations);
    REQUIRE(exchange(&offeredEngine, &host->exports));
    REQUIRE(host->exports.pInitFunc && host->exports.pHudInitFunc && host->exports.pStudioInterface);
    REQUIRE(exchanges() == 1);
    legacyF(untouched.data());
    for (unsigned char byte : untouched) {
        REQUIRE(byte == 0xA5);
    }
    REQUIRE(exchanges() == 1);
    const auto destination = Symbol<const void* (*)()>(module, "CseFixtureDestination");
    REQUIRE(destination && destination() == &host->destinations && effects() == 0);
    const auto modifiers = Symbol<const void* (*)()>(module, "CseFixtureModifiers");
    const auto variant = Symbol<int (*)()>(module, "CseFixtureVariant");
    REQUIRE(modifiers && variant);
    if (variant() == 1) {
        REQUIRE(std::memcmp(modifiers(), &host->modules, sizeof(host->modules)) == 0);
    } else {
        const modfuncs_t empty = {};
        REQUIRE(std::memcmp(modifiers(), &empty, sizeof(empty)) == 0);
    }
    REQUIRE(!initialize(&EngineClientDescriptor, &host->functions, -1) && effects() == 0);
    REQUIRE(initialize(&offeredEngine, &host->functions, CLDLL_INTERFACE_VERSION));
    REQUIRE(effects() == 2);
    const auto string = Symbol<const void* (*)()>(module, "CseFixtureString");
    const auto copied = Symbol<const void* (*)()>(module, "CseFixtureCopiedEngine");
    REQUIRE(string && string() == host->directory && copied);
    REQUIRE(std::memcmp(copied(), &host->functions, sizeof(host->functions)) == 0);
    const auto mouse = Symbol<int (*)(int *, int *, int *, int *)>(module, "CseFixtureMouse");
    REQUIRE(mouse && mouseCalls == 0);
    REQUIRE(mouse(&host->mouseOutputs[0], &host->mouseOutputs[1], &host->mouseOutputs[2], &host->mouseOutputs[3]) == 1);
    REQUIRE(mouseCalls == 1 && host->mouseOutputs[0] == -17 && host->mouseOutputs[1] == 37);
    REQUIRE(host->mouseOutputs[2] == 640 && host->mouseOutputs[3] == 480);
    REQUIRE(!legacyInitialize(&host->functions, CLDLL_INTERFACE_VERSION) && effects() == 2);
    REQUIRE(studio(&offeredEngine, STUDIO_INTERFACE_VERSION, &renderer, &host->studio));
    REQUIRE(renderer && renderer->version == STUDIO_INTERFACE_VERSION && effects() == 3);
    const auto player = Symbol<const void* (*)()>(module, "CseFixturePlayer");
    REQUIRE(player && player() == &host->players[1] && host->players[1].gaitframe == 42.5f);
    REQUIRE(host->players[1].m_nSteamID == UINT64_C(0xFEDCBA9876543210));
    for (char byte : host->players[1].hashedcdkey) {
        REQUIRE(static_cast<unsigned char>(byte) == 0xA5);
    }
    MovementStorage movement;
    REQUIRE(movement.memory);
    auto* move = new (movement.memory) playermove_t{};
    if (sizeof(void*) > sizeof(std::uint32_t)) {
        REQUIRE(reinterpret_cast<std::uintptr_t>(move) > UINT32_MAX);
    }
    auto* suffix = reinterpret_cast<unsigned char*>(move) + ClientPrefixSize_playermove_t;
    const std::size_t suffixBytes = sizeof(*move) - ClientPrefixSize_playermove_t;
    std::memset(suffix, 0xA5, suffixBytes);
    move->PM_PlayerTrace = Trace;
    move->PM_HullForBsp = Hull;
    move->Con_DPrintf = Print;
    move->Con_Printf = Print;
    const auto movementCall = Symbol<int (*)(playermove_t*)>(module, "CseFixtureMovement");
    const auto hull = Symbol<const void* (*)()>(module, "CseFixtureHull");
    REQUIRE(movementCall && movementCall(move) && hull && hull() == &host->hull);
    REQUIRE(move->origin[0] == 12.5f && move->origin[1] == 23.5f && traceCalls == 1 && printCalls == 2);
    for (std::size_t index = 0; index < suffixBytes; ++index) {
        REQUIRE(suffix[index] == 0xA5);
    }
    const auto movementSize = Symbol<std::size_t (*)()>(module, "CseFixtureMovementSize");
    const auto playerSize = Symbol<std::size_t (*)()>(module, "CseFixturePlayerSize");
    REQUIRE(movementSize && playerSize);
    std::printf("Owned client ABI passed: pointer_bits=%u host=%llx movement_owner=%zu client=%zu required=%zu "
        "player_owner=%zu client=%zu required=%zu\n", NativePointerBits,
        static_cast<unsigned long long>(hostAddress), sizeof(playermove_t), movementSize(),
        ClientPrefixSize_playermove_t, sizeof(player_info_t), playerSize(), ClientPrefixSize_player_info_t);
    cse_platform::UnloadModule(module);
    host->~HostData();
    host = nullptr;
    REQUIRE(!cse_platform::FindLoadedModule(argv[1]));
    return 0;
}
