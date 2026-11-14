#include "client.h"
#include "decals.h"
#include "gl_model.h"
#include "sound.h"
#include "conprint.h"
#include "mem.h"
#include "vgui_intwrap2.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

client_state_t cl;
cvar_t fs_startup_timings{};
cvar_t fs_lazy_precache{};
int cl_playerindex = 0;
char decal_names[MAX_BASE_DECALS][DECAL_NAME_BYTES];

void CL_PrecacheBSPModels(const char* mapName);

namespace {
constexpr unsigned char StateSentinel = 0x5a;
constexpr int InitialModelCount = 7;
// Resource-list indices occupy 12 bits on the wire.
constexpr int LargestWireIndex = 4095;
model_t loadedModel{};
sfx_t loadedSound{};
char loadedName[] = "fixture";
byte loadedScript[] = {0};
bool allowLoads = false;
int soundLoads = 0;
int modelLoads = 0;
int lazyModelLoads = 0;
int scriptLoads = 0;
int nameAllocations = 0;
int disconnects = 0;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

void Reset() {
    std::memset(&cl, StateSentinel, sizeof(cl));
    std::fill(std::begin(cl.model_precache), std::end(cl.model_precache), nullptr);
    std::fill(std::begin(cl.sound_precache), std::end(cl.sound_precache), &loadedSound);
    cl.model_precache_count = InitialModelCount;
    cl.resourcesonhand.pNext = &cl.resourcesonhand;
    cl.resourcesonhand.pPrev = &cl.resourcesonhand;
    fs_lazy_precache.value = 0;
    soundLoads = 0;
    modelLoads = 0;
    lazyModelLoads = 0;
    scriptLoads = 0;
    nameAllocations = 0;
    disconnects = 0;
}

void Link(resource_t& resource) {
    resource.pPrev = cl.resourcesonhand.pPrev;
    resource.pNext = &cl.resourcesonhand;
    resource.pPrev->pNext = &resource;
    cl.resourcesonhand.pPrev = &resource;
}

void CheckResource(const std::string& mode, int index, bool valid) {
    Reset();
    resource_t resource{};
    resource.nIndex = index;
    std::strcpy(resource.szFileName, "fixture");
    if (mode == "sound" || mode == "missing-sound") {
        resource.type = t_sound;
        if (mode == "missing-sound") {
            resource.ucFlags = RES_WASMISSING;
        }
    } else if (mode == "event") {
        resource.type = t_eventscript;
    } else {
        resource.type = t_model;
        if (mode == "inline-model" || mode == "bsp") {
            std::strcpy(resource.szFileName, "*1");
        }
        if (mode == "lazy-model") {
            fs_lazy_precache.value = 1;
        }
    }
    Link(resource);
    const auto* before = reinterpret_cast<const byte*>(&cl);
    const std::vector<byte> saved(before, before + sizeof(cl));
    allowLoads = valid;
    if (mode == "bsp") {
        CL_PrecacheBSPModels("maps/fixture.bsp");
    } else {
        Require(CL_PrecacheResources() == TRUE, "resource traversal completes");
        Require((resource.ucFlags & RES_PRECACHED) != 0, "resource traversal advances past each record");
    }
    Require(disconnects == 0, "invalid indices do not change missing-file disconnect policy");
    if (!valid) {
        Require(std::memcmp(saved.data(), &cl, sizeof(cl)) == 0, "invalid index changed client state");
        Require(soundLoads + modelLoads + lazyModelLoads + scriptLoads + nameAllocations == 0,
            "invalid resource invoked a loader or allocator");
    } else if (mode == "sound") {
        Require(soundLoads == 1 && cl.sound_precache[index] == &loadedSound, "valid sound is precached");
    } else if (mode == "missing-sound") {
        Require(soundLoads == 0 && cl.sound_precache[index] == nullptr, "valid missing sound clears its slot");
    } else if (mode == "event") {
        Require(scriptLoads == 1 && nameAllocations == 1, "valid event is loaded once");
        Require(cl.event_precache[index].filename == loadedName
            && cl.event_precache[index].pszScript == reinterpret_cast<const char*>(loadedScript),
            "valid event retains its filename and script");
    } else {
        if (mode == "inline-model") {
            Require(modelLoads == 0 && lazyModelLoads == 0, "inline models are deferred by general precache");
        } else {
            Require(cl.model_precache[index] == &loadedModel, "valid model is stored in its slot");
            Require(mode == "lazy-model" ? lazyModelLoads == 1 : modelLoads == 1,
                "valid model uses the requested loading path");
        }
        const int expectedCount = mode == "bsp" ? InitialModelCount : std::max(InitialModelCount, index + 1);
        Require(cl.model_precache_count == expectedCount, "only general model precache updates the count");
    }
}

void CheckDecal(int index, bool valid) {
    std::memset(decal_names, StateSentinel, sizeof(decal_names));
    const auto* before = reinterpret_cast<const byte*>(decal_names);
    const std::vector<byte> saved(before, before + sizeof(decal_names));
    char name[] = "fixture-decal-name-longer-than-the-slot";
    Draw_DecalSetName(index, name);
    if (valid) {
        Require(std::strncmp(decal_names[index], name, DECAL_NAME_BYTES - 1) == 0,
            "valid decal name is copied to its slot");
        Require(decal_names[index][DECAL_NAME_BYTES - 1] == 0, "decal name remains terminated");
    } else {
        Require(std::memcmp(saved.data(), decal_names, sizeof(decal_names)) == 0,
            "invalid decal index changed the name table");
    }
}

void CheckMixedList() {
    Reset();
    resource_t resources[4]{};
    resources[0].type = t_sound;
    resources[0].nIndex = MAX_SOUNDS;
    resources[1].type = t_model;
    resources[1].nIndex = 1;
    resources[2].type = t_eventscript;
    resources[2].nIndex = MAX_EVENT_SCRIPTS;
    resources[3].type = t_sound;
    resources[3].nIndex = 1;
    for (resource_t& resource : resources) {
        std::strcpy(resource.szFileName, "fixture");
        Link(resource);
    }
    allowLoads = true;
    Require(CL_PrecacheResources() == TRUE, "mixed list traversal completes");
    Require(modelLoads == 1 && soundLoads == 1 && scriptLoads == 0 && nameAllocations == 0,
        "malformed resources are skipped while later valid resources still load");
    Require(cl.model_precache[1] == &loadedModel && cl.sound_precache[1] == &loadedSound,
        "mixed list preserves valid precache results");
}
}

void SetLoadingProgressBarStatusText(const char*) {}
void ContinueLoadingProgressBar(const char*, int, float) {}
void AddStartupTiming(const char*) {}
void Con_Printf(const char*, ...) {}
void Con_DPrintf(const char*, ...) {}
void COM_ExplainDisconnection(qboolean, const char*, ...) {}

void CL_Disconnect() {
    ++disconnects;
}

void S_BeginPrecaching() {}
void S_EndPrecaching() {}

sfx_t* S_PrecacheSound(char*) {
    Require(allowLoads, "invalid resource reached the sound loader");
    ++soundLoads;
    return &loadedSound;
}

model_t* Mod_ForName(const char*, qboolean, qboolean) {
    Require(allowLoads, "invalid resource reached the model loader");
    ++modelLoads;
    return &loadedModel;
}

model_t* Mod_FindName(qboolean, const char*) {
    Require(allowLoads, "invalid resource reached the lazy model loader");
    ++lazyModelLoads;
    return &loadedModel;
}

char* Mem_Strdup(const char*) {
    Require(allowLoads, "invalid resource reached the event-name allocator");
    ++nameAllocations;
    return loadedName;
}

byte* COM_LoadFile(char*, int, int*) {
    Require(allowLoads, "invalid resource reached the script loader");
    ++scriptLoads;
    return loadedScript;
}

void Q_strncpy(char* destination, const char* source, int count) {
    std::strncpy(destination, source, static_cast<std::size_t>(count));
}

int Q_strnicmp(const char* first, const char* second, int count) {
    return std::strncmp(first, second, static_cast<std::size_t>(count));
}

int Q_strcasecmp(const char* first, const char* second) {
    return std::strcmp(first, second);
}

int Q_strlen(const char* text) {
    return static_cast<int>(std::strlen(text));
}

char* Q_strstr(const char* text, const char* substring) {
    return const_cast<char*>(std::strstr(text, substring));
}

#include "decal_names.inc"
#include "precache.inc"

int main(int argc, char** argv) {
    Require(argc == 2, "expected a precache case name");
    const std::string mode = argv[1];
    if (mode == "mixed") {
        CheckMixedList();
        return 0;
    }
    int capacity = MAX_MODELS;
    if (mode == "sound" || mode == "missing-sound") {
        capacity = MAX_SOUNDS;
    } else if (mode == "event") {
        capacity = MAX_EVENT_SCRIPTS;
    } else if (mode == "decal") {
        capacity = MAX_BASE_DECALS;
    }
    const int validIndices[] = {0, capacity - 1};
    const int invalidIndices[] = {-1, capacity, LargestWireIndex, INT_MAX};
    for (int index : validIndices) {
        if (mode == "decal") {
            CheckDecal(index, true);
        } else {
            CheckResource(mode, index, true);
        }
    }
    for (int index : invalidIndices) {
        if (mode == "decal") {
            CheckDecal(index, false);
        } else {
            CheckResource(mode, index, false);
        }
    }
}
