#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#define ARRAYSIZE(array) static_cast<int>(sizeof(array) / sizeof((array)[0]))
enum FORCE_TYPE {
    force_exactfile,
    force_model_samebounds,
    force_model_specifybounds,
    force_model_specifybounds_if_avail
};
struct Vector {
    float values[3];
    Vector(float x = 0, float y = 0, float z = 0) : values{x, y, z} {}
    operator float*() { return values; }
};
Vector g_vecZero;
std::map<std::string, int> availableFiles;
std::vector<const char*> genericResources;
std::vector<const char*> exactResources;
std::vector<std::string> probes;
int GetFileSize(char* filename) {
    probes.emplace_back(filename);
    const auto found = availableFiles.find(filename);
    return found == availableFiles.end() ? -1 : found->second;
}
struct EngineFunctions {
    int (*pfnGetFileSize)(char*) = GetFileSize;
} g_engfuncs;
int PrecacheGeneric(char* filename) {
    genericResources.push_back(filename);
    return static_cast<int>(genericResources.size());
}
void ForceUnmodified(FORCE_TYPE type, float* mins, float* maxs, const char* filename) {
    if (type == force_exactfile) {
        if (mins[0] != 0 || mins[1] != 0 || mins[2] != 0 || maxs[0] != 0 || maxs[1] != 0 || maxs[2] != 0) {
            throw std::runtime_error("image exact-file registration changed zero bounds");
        }
        exactResources.push_back(filename);
    }
}
#define PRECACHE_GENERIC PrecacheGeneric
#define ENGINE_FORCE_UNMODIFIED ForceUnmodified
void PrecacheSound(const char*) {}
int PrecacheModel(const char*) { return 1; }
int PrecacheEvent(int, const char*) { return 1; }
#define PRECACHE_SOUND PrecacheSound
#define PRECACHE_MODEL PrecacheModel
#define PRECACHE_EVENT PrecacheEvent
bool UTIL_IsGame(const char*) { return false; }
void UTIL_PrecacheOther(const char*) {}
char* sPlayerModelFiles[] = {nullptr, nullptr, nullptr};
struct BotProfiles {
    const char* GetCustomSkinFname(int) { return nullptr; }
} profiles;
BotProfiles* TheBotProfiles = &profiles;
int giPrecacheGrunt = 0;
unsigned short g_iShadowSprite = 0;
unsigned short m_usResetDecals = 0;

#include "resources.inc"

void Require(bool condition, const char* description) {
    if (!condition) {
        throw std::runtime_error(description);
    }
}
std::vector<std::string> Names(const std::vector<const char*>& resources) {
    std::vector<std::string> names;
    for (const char* filename : resources) {
        names.emplace_back(filename);
    }
    std::sort(names.begin(), names.end());
    return names;
}
void Verify(const std::vector<std::string>& images) {
    std::vector<std::string> expectedImages = images;
    std::sort(expectedImages.begin(), expectedImages.end());
    Require(Names(genericResources) == expectedImages, "precache registered missing or incorrect scope image paths");
    std::vector<std::string> expectedExact = {
        "sprites/black_smoke1.spr", "sprites/black_smoke2.spr", "sprites/black_smoke3.spr",
        "sprites/black_smoke4.spr", "sprites/fast_wallpuff1.spr", "sprites/smokepuff.spr", "sprites/gas_puff_01.spr"
    };
    expectedExact.insert(expectedExact.end(), images.begin(), images.end());
    std::sort(expectedExact.begin(), expectedExact.end());
    Require(Names(exactResources) == expectedExact, "scope consistency paths differ or unrelated sprite checks changed");
    for (const char* generic : genericResources) {
        Require(std::find(exactResources.begin(), exactResources.end(), generic) != exactResources.end(),
            "precache and exact-file checks did not retain the same filename pointer");
    }
}
void ChurnStack() {
    volatile char bytes[16384];
    for (std::size_t i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = static_cast<char>(i);
    }
}
int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected one scenario");
        const std::string scenario = argv[1];
        const std::vector<std::string> targa = {
            "sprites/scope_arc.tga", "sprites/scope_arc_nw.tga", "sprites/scope_arc_ne.tga", "sprites/scope_arc_sw.tga"
        };
        const std::vector<std::string> webp = {
            "sprites/scope_arc.webp", "sprites/scope_arc_nw.webp", "sprites/scope_arc_ne.webp",
            "sprites/scope_arc_sw.webp"
        };
        std::vector<std::string> expected;
        if (scenario == "tga-only" || scenario == "both") {
            for (const auto& name : targa) {
                availableFiles[name] = 42;
            }
            expected = targa;
        }
        if (scenario == "webp-only" || scenario == "both") {
            for (const auto& name : webp) {
                availableFiles[name] = 42;
            }
            expected.insert(expected.end(), webp.begin(), webp.end());
        }
        if (scenario == "missing-both") {
            expected = targa;
        } else if (scenario == "mixed") {
            availableFiles[targa[0]] = 42;
            availableFiles[webp[1]] = 42;
            availableFiles[targa[2]] = 42;
            availableFiles[webp[2]] = 42;
            expected = {"sprites/scope_arc.tga", "sprites/scope_arc_nw.webp", "sprites/scope_arc_ne.tga",
                "sprites/scope_arc_ne.webp", "sprites/scope_arc_sw.tga"};
        } else if (scenario != "tga-only" && scenario != "webp-only" && scenario != "both") {
            throw std::runtime_error("unknown scenario");
        }
        ClientPrecache();
        // The engine retains raw filename pointers after this function returns.
        ChurnStack();
        Verify(expected);
        Require(probes.size() == 8, "scope availability did not use the engine filesystem callback");
        const auto savedGeneric = genericResources;
        const auto savedExact = exactResources;
        genericResources.clear();
        exactResources.clear();
        ClientPrecache();
        ChurnStack();
        Verify(expected);
        Require(genericResources == savedGeneric && exactResources == savedExact,
            "registration filename storage did not survive or remain stable across calls");
        std::printf("PASS %s\n", scenario.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s: %s\n", argc > 1 ? argv[1] : "", error.what());
        return 1;
    }
}
