#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include "cse_extended_stats_protocol.h"
using namespace cse::extstats;
using Registration = std::pair<int, int>;
static std::map<std::string, Registration> wire;
static std::map<std::string, Registration> metadata;
static int RawRegister(const char* name, int size) {
    auto found = wire.find(name);
    if (found != wire.end()) {
        return found->second.first;
    }
    const int id = 64 + static_cast<int>(wire.size());
    wire[name] = {id, size};
    return id;
}
static int HookedRegister(const char* name, int size) {
    const int id = RawRegister(name, size);
    metadata[name] = {id, size};
    return id;
}
static int Lookup(void*, const char* name, int* size) {
    const auto found = metadata.find(name);
    if (found == metadata.end()) {
        return 0;
    }
    if (size) {
        *size = found->second.second;
    }
    return found->second.first;
}
#define GET_USER_MSG_ID Lookup
#define PLID nullptr
struct Engine {
    int (*pfnRegUserMsg)(const char*, int);
};
static Engine g_engfuncs{RawRegister};
static Engine hooked{HookedRegister};
static Engine* HookedEngine = &hooked;
static int HealthExId = 0;
static int ArmorExId = 0;
static int HealthId = 0;
static int ArmorId = 0;
static int SpecHealthId = 0;
static int SpecHealth2Id = 0;
static int HltvId = 0;
static int ResetHudId = 0;
static int InitHudId = 0;
static void MF_Log(const char*, ...) {
}
#include "registration_bodies.inc"
int main(int, char** argv) {
    const bool conflict = std::strcmp(argv[1], "conflict") == 0;
    if (conflict) {
        HookedRegister("HealthEx", 4);
    }
    RegisterMessages();
    if (std::strcmp(argv[1], "late") == 0) {
        HookedRegister("Health", 1);
        HookedRegister("Battery", 2);
        HookedRegister("ResetHUD", 1);
        ResolveLegacyMessages();
        if (!HealthId || !ArmorId || !ResetHudId) {
            std::fputs("FAIL: game activation did not resolve late registered legacy IDs\n", stderr);
            return 1;
        }
    }
    if (conflict) {
        if (HealthExId != 0 || ArmorExId != 0) {
            std::fputs("FAIL: incompatible existing message was adopted\n", stderr);
            return 1;
        }
    } else if (!Lookup(nullptr, "HealthEx", nullptr) || !Lookup(nullptr, "BatteryEx", nullptr)) {
        std::fputs("FAIL: Ex registration bypassed AMXX/Metamod metadata\n", stderr);
        return 1;
    }
    return 0;
}
