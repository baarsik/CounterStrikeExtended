#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include "extdll.h"
#if defined(M_PI)
#undef M_PI
#endif
#include "util.h"
#include "cbase.h"
#include "nav_area.h"
#include "cs_bot_chatter.h"

enginefuncs_t g_engfuncs{};
globalvars_t globals{};
globalvars_t *gpGlobals = &globals;
cvar_t cv_bot_debug{};
cvar_t cv_bot_traceview{};
cvar_t cv_bot_nav_zdraw{};
cvar_t cv_bot_quicksave{};
class CBotManager;
class CHostageManager;
CBotManager *TheBots = nullptr;
CHostageManager *g_pHostages = nullptr;
BotPhraseManager *TheBotPhrases = nullptr;

namespace {
constexpr std::uint32_t FileVersion = 5;
constexpr std::uint32_t BspBytes = 1024;
constexpr std::uint32_t FirstAreaId = 1;
constexpr std::uint32_t SecondAreaId = 2;
constexpr std::uint32_t SpotId = 17;
constexpr std::uint32_t MissingId = 77;
constexpr float AreaWidth = 64.0f;
constexpr unsigned char LastPathFraction = 255;
std::vector<unsigned char> loadedFile;

void Require(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "Navigation file check failed: %s\n", message);
        std::exit(1);
    }
}

void Unexpected(const char *boundary) {
    std::fprintf(stderr, "Unexpected fixture boundary: %s\n", boundary);
    std::exit(2);
}

void PutByte(std::vector<unsigned char> &bytes, unsigned char value) {
    bytes.push_back(value);
}

void Put32(std::vector<unsigned char> &bytes, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        PutByte(bytes, static_cast<unsigned char>(value >> shift));
    }
}

void PutFloat(std::vector<unsigned char> &bytes, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    Put32(bytes, bits);
}

void PutArea(std::vector<unsigned char> &bytes, bool first, unsigned int version,
             std::uint32_t adjacent, std::uint32_t approach, std::uint32_t spot) {
    Put32(bytes, first ? FirstAreaId : SecondAreaId);
    PutByte(bytes, NAV_CROUCH);
    const float origin = first ? 0.0f : AreaWidth;
    for (float coordinate : {origin, 0.0f, 0.0f, origin + AreaWidth, AreaWidth, 0.0f, 0.0f, 0.0f}) {
        PutFloat(bytes, coordinate);
    }
    for (int direction = 0; direction < NUM_DIRECTIONS; ++direction) {
        const bool connected = direction == (first ? EAST : WEST);
        Put32(bytes, connected ? 1 : 0);
        if (connected) {
            Put32(bytes, first ? adjacent : FirstAreaId);
        }
    }
    PutByte(bytes, first ? 1 : 0);
    if (first) {
        if (version > 1) {
            Put32(bytes, SpotId);
        }
        for (float coordinate : {1.0f, 2.0f, 0.0f}) {
            PutFloat(bytes, coordinate);
        }
        if (version > 1) {
            PutByte(bytes, HidingSpot::IN_COVER);
        }
    }
    PutByte(bytes, first ? 1 : 0);
    if (first) {
        Put32(bytes, FirstAreaId);
        Put32(bytes, 0); // Optional previous approach area.
        PutByte(bytes, GO_EAST);
        Put32(bytes, approach);
        PutByte(bytes, GO_WEST);
    }
    const bool encounter = first && version >= 3;
    Put32(bytes, encounter ? 1 : 0);
    if (encounter) {
        Put32(bytes, FirstAreaId);
        PutByte(bytes, EAST);
        Put32(bytes, SecondAreaId);
        PutByte(bytes, WEST);
        PutByte(bytes, 1);
        Put32(bytes, spot);
        PutByte(bytes, LastPathFraction);
    }
    if (version >= 5) {
        PutByte(bytes, 0); // Undefined place, encoded as a 16-bit directory entry.
        PutByte(bytes, 0);
    }
}

std::vector<unsigned char> MakeFile(unsigned int version = FileVersion,
                                    std::uint32_t adjacent = SecondAreaId,
                                    std::uint32_t approach = SecondAreaId, std::uint32_t spot = SpotId) {
    std::vector<unsigned char> bytes;
    Put32(bytes, NAV_MAGIC_NUMBER);
    Put32(bytes, version);
    if (version >= 4) {
        Put32(bytes, BspBytes);
    }
    if (version >= 5) {
        PutByte(bytes, 0); // Empty place directory.
        PutByte(bytes, 0);
    }
    Put32(bytes, 2);
    PutArea(bytes, true, version, adjacent, approach, spot);
    PutArea(bytes, false, version, adjacent, approach, spot);
    return bytes;
}

byte *LoadFile(char *name, int *length) {
    if (std::strstr(name, ".loc")) {
        *length = 0; // The fixture has no legacy location sidecar.
        return nullptr;
    }
    Require(std::strstr(name, ".nav") != nullptr, "only owned navigation input is requested");
    *length = static_cast<int>(loadedFile.size());
    auto *copy = static_cast<byte *>(std::malloc(loadedFile.size()));
    Require(copy != nullptr, "allocate engine file buffer");
    std::memcpy(copy, loadedFile.data(), loadedFile.size());
    return copy;
}

int FileSize(char *) {
    return BspBytes;
}

void CheckLoaded(unsigned int version) {
    Require(TheNavAreaList.size() == 2, "two real areas loaded");
    CNavArea *first = TheNavAreaGrid.GetNavAreaByID(FirstAreaId);
    CNavArea *second = TheNavAreaGrid.GetNavAreaByID(SecondAreaId);
    Require(first && second, "area identity lookup");
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        Require(reinterpret_cast<std::uintptr_t>(first) > UINT32_MAX, "area stored above 4 GiB");
    }
    Require(first->GetAdjacentArea(EAST, 0) == second, "native adjacent pointer");
    Require(second->GetAdjacentArea(WEST, 0) == first, "reverse adjacency");
    const auto &connection = first->GetAdjacentList(EAST)->front();
    Require(connection.id == SecondAreaId, "saved adjacent identity survives resolution");
    Require(first->GetApproachInfoCount() == 1, "approach count");
    const auto *approach = first->GetApproachInfo(0);
    Require(approach->here.area == first && approach->prev.area == nullptr && approach->next.area == second,
            "required and optional approach pointers");
    Require(approach->next.id == SecondAreaId, "saved approach identity survives resolution");
    Require(first->GetHidingSpotList()->size() == 1, "hiding spot count");
    HidingSpot *hidingSpot = first->GetHidingSpotList()->front();
    Require(hidingSpot->HasGoodCover() && hidingSpot->GetPosition()->y == 2.0f, "hiding spot data");
    if (version >= 3) {
        SpotEncounter *encounter = first->GetSpotEncounter(first, second);
        Require(encounter && encounter->spotList.size() == 1, "encounter path");
        const auto &order = encounter->spotList.front();
        Require(order.spot == hidingSpot && order.id == SpotId && order.t == 1.0f, "native encounter spot");
        Require(encounter->path.from.z == HalfHumanHeight && encounter->path.to.z == HalfHumanHeight,
                "encounter eye height reconstruction");
    }
    Require(first->PostLoad() == NAV_OK && first->GetAdjacentArea(EAST, 0) == second,
            "repeated reference resolution retains identities");
}
} // namespace

// Owned engine/world boundaries. Navigation parsing, grid, references and writing use production sources.
// The fixture contains no ladder entities, place names or legacy location sidecar.
void CONSOLE_ECHO(char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stdout, format, arguments);
    va_end(arguments);
}
const char *BotPhraseManager::IDToName(unsigned int) const {
    Unexpected("place name provider");
    return nullptr;
}
unsigned int BotPhraseManager::NameToID(const char *) const {
    Unexpected("place ID provider");
    return 0;
}
CBaseEntity *UTIL_FindEntityByClassname(CBaseEntity *, const char *name) {
    Require(std::strcmp(name, "func_ladder") == 0, "only ladder enumeration is requested");
    return nullptr;
}
CBaseEntity *UTIL_PlayerByIndex(int) {
    Unexpected("player lookup");
    return nullptr;
}
edict_t *DBG_EntOfVars(const entvars_t *) {
    Unexpected("entity variables lookup");
    return nullptr;
}
void UTIL_MakeVectors(const Vector &) {
    Unexpected("player view vectors");
}
void UTIL_SetOrigin(entvars_t *, const Vector &) {
    Unexpected("entity movement");
}
void UTIL_SayTextAll(const char *, CBaseEntity *) {
    Unexpected("chat");
}
void EMIT_SOUND_DYN(edict_t *, int, const char *, float, float, int, int) {
    Unexpected("sound");
}
void UTIL_DrawBeamPoints(Vector, Vector, int, byte, byte, byte) {
    Unexpected("debug drawing");
}
CBasePlayer *UTIL_GetClosestPlayer(const Vector *, float *) {
    Unexpected("closest player");
    return nullptr;
}
CBasePlayer *UTIL_GetClosestPlayer(const Vector *, int, float *) {
    Unexpected("closest team player");
    return nullptr;
}
CBasePlayer *UTIL_GetLocalPlayer() {
    Unexpected("local player");
    return nullptr;
}
void UTIL_TraceLine(const Vector &, const Vector &, IGNORE_MONSTERS, edict_t *, TraceResult *) {
    Unexpected("world trace");
}
void UTIL_TraceLine(const Vector &, const Vector &, IGNORE_MONSTERS, IGNORE_GLASS, edict_t *, TraceResult *) {
    Unexpected("world trace with glass");
}
void HintMessageToAllPlayers(const char *) {
    Unexpected("out-of-date BSP warning");
}
char *MP_COM_Parse(char *) {
    Unexpected("legacy location parser");
    return nullptr;
}
char *MP_COM_GetToken() {
    Unexpected("legacy location token");
    return nullptr;
}

int main(int argc, char **argv) {
    Require(argc == 2, "provide an owned round-trip output path");
    globals.pStringBase = "fixture";
    globals.mapname = 0;
    g_engfuncs.pfnLoadFileForMe = LoadFile;
    g_engfuncs.pfnFreeFile = std::free;
    g_engfuncs.pfnGetFileSize = FileSize;
    for (unsigned int version = 1; version <= FileVersion; ++version) {
        loadedFile = MakeFile(version);
        Require(LoadNavigationMap() == NAV_OK, "production map loader accepts legacy versions 1 through 5");
        CheckLoaded(version);
        DestroyNavigationMap();
    }
    loadedFile = MakeFile();
    Require(LoadNavigationMap() == NAV_OK, "load before save");
    char output[4096]{};
    Require(std::strlen(argv[1]) < sizeof(output), "output path fits");
    std::strcpy(output, argv[1]);
    std::filesystem::remove(output);
    Require(SaveNavigationMap(output), "production map writer");
    std::ifstream saved(output, std::ios::binary);
    std::vector<unsigned char> roundTrip((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
    Require(roundTrip == loadedFile, "version 5 byte-exact save round trip");
    DestroyNavigationMap();
    loadedFile = roundTrip;
    Require(LoadNavigationMap() == NAV_OK, "reload serialized native map");
    CheckLoaded(FileVersion);
    DestroyNavigationMap();
    for (int invalid = 0; invalid < 3; ++invalid) {
        loadedFile = MakeFile(FileVersion, invalid == 0 ? MissingId : SecondAreaId,
                              invalid == 1 ? MissingId : SecondAreaId, invalid == 2 ? MissingId : SpotId);
        Require(LoadNavigationMap() == NAV_CORRUPT_DATA, "missing reference rejected by production map loader");
        Require(TheNavAreaList.empty() && TheHidingSpotList.empty(), "rejected map is fully removed");
        Require(TheNavAreaGrid.GetNavAreaByID(FirstAreaId) == nullptr, "rejected map leaves no grid references");
    }
    std::printf("CS navigation versions 1-5, byte round trip and invalid references passed\n");
    return 0;
}
