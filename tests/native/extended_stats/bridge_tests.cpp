#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "extended_stats.h"
enum { MAX_PLAYERS = 64 };
struct cl_entity_t {
    int index = 1;
    float angles[3] = {};
};
static bool playingDemo = false;
static double clientTime = 1;
static int maxClients = 32;
struct Demo {
    bool IsPlayingback() {
        return playingDemo;
    }
};
struct Engine {
    cl_entity_t player;
    Demo* pDemoAPI = nullptr;
    cl_entity_t* GetLocalPlayer() {
        if (maxClients == 0) {
            std::fputs("FAIL: queried the engine's unallocated local entity during HUD initialization\n", stderr);
            std::abort();
        }
        return &player;
    }
    bool IsSpectateOnly() {
        return false;
    }
    cl_entity_t* GetEntityByIndex(int) {
        return &player;
    }
    int GetMaxClients() {
        return maxClients;
    }
    double GetClientTime() {
        return clientTime;
    }
} gEngfuncs;
struct Extra {
    int health = 0;
    int sb_health = 0;
} g_PlayerExtraInfo[MAX_PLAYERS + 1];
static const unsigned char* readBuffer = nullptr;
#define BEGIN_READ(buffer, size) (readBuffer = static_cast<const unsigned char*>(buffer))
#define READ_BYTE() (*readBuffer++)
static int ReadShort() {
    const int result = readBuffer[0] | (readBuffer[1] << 8);
    readBuffer += 2;
    return result;
}
#define READ_SHORT() ReadShort()
static int ReadLong() {
    const unsigned int result = static_cast<unsigned int>(readBuffer[0]) |
                                (static_cast<unsigned int>(readBuffer[1]) << 8) |
                                (static_cast<unsigned int>(readBuffer[2]) << 16) |
                                (static_cast<unsigned int>(readBuffer[3]) << 24);
    readBuffer += 4;
    return static_cast<int>(result);
}
#define READ_LONG() ReadLong()
struct CHudHealth {
    int MsgFunc_Health(const char*, int, void*);
    int MsgFunc_HealthInfo(const char*, int, void*);
    void SetHealth(int);
};
struct CHudBattery {
    int MsgFunc_Battery(const char*, int, void*);
};
struct Hud {
    struct Health {
        int amount = 100;
        void SetHealth(int value) {
            amount = value;
        }
    } m_Health;
    struct Armor {
        int amount = 0;
        void SetArmor(int value) {
            amount = value;
        }
    } m_Battery;
} gHUD;
struct ViewPort {
    int updates = 0;
    void UpdateSpectatorPanel() {
        ++updates;
    }
} viewport;
static ViewPort* gViewPortInterface = &viewport;
int g_iUser1 = 0;
int g_iUser2 = 2;
void CHudHealth::SetHealth(int health) {
    gHUD.m_Health.SetHealth(health);
}
constexpr int ROLL = 2;
constexpr int GIB_NORMAL = 0;
struct ref_params_s {
    int health = 0;
    int viewentity = 1;
    float viewheight[3] = {0, 0, 17};
    float viewangles[3] = {};
    float simvel[3] = {};
    struct MoveVars {
        float rollangle = 0;
        float rollspeed = 0;
    } settings;
    MoveVars* movevars = &settings;
};
float V_CalcRoll(const float*, const float*, float, float) {
    return 0;
}
struct PredictedPlayer {
    int deaths = 0;
    int spawns = 0;
    void Killed(void*, int) {
        ++deaths;
    }
    void Spawn() {
        ++spawns;
    }
} player;
#include "bridge_bodies.inc"
int main(int, char** argv) {
    if (std::strcmp(argv[1], "fractional_health") == 0) {
        CHudHealth hud;
        unsigned char truncated[] = {1, 0, 0, 0, 0};
        CSE_UpdateLegacyHealth(1, 1, true);
        hud.MsgFunc_HealthInfo(nullptr, sizeof(truncated), truncated);
        if (gHUD.m_Health.amount != 1) {
            return 1;
        }
        CSE_UpdateLegacyHealth(1, 0, true);
        if (gHUD.m_Health.amount != 0) {
            return 1;
        }
        CSE_UpdateLegacyHealth(1, 1, true);
        if (gHUD.m_Health.amount != 1) {
            return 1;
        }
    } else if (std::strcmp(argv[1], "healthinfo") == 0) {
        CHudHealth hud;
        unsigned char hidden[] = {1, 255, 255, 255, 255};
        hud.MsgFunc_HealthInfo(nullptr, sizeof(hidden), hidden);
        if (gHUD.m_Health.amount != 100) {
            return 1;
        }
        CSE_UpdateLegacyHealth(1, 255, true);
        unsigned char exact[] = {1, 0xf4, 0x01, 0, 0}; // 500
        hud.MsgFunc_HealthInfo(nullptr, sizeof(exact), exact);
        if (gHUD.m_Health.amount != 500 || g_PlayerExtraInfo[1].sb_health != 500) {
            return 1;
        }
        const unsigned char wide[] = {1, 0x9f, 0x86, 1, 0}; // 99999
        CSE_HandleExtendedStats(cse::extstats::Stat::Health, wide, sizeof(wide));
        hud.MsgFunc_HealthInfo(nullptr, sizeof(exact), exact);
        if (gHUD.m_Health.amount != 99999) {
            return 1;
        }
        const unsigned char invalid[] = {1, 255, 255, 255, 255};
        CSE_HandleExtendedStats(cse::extstats::Stat::Health, invalid, sizeof(invalid));
        if (gHUD.m_Health.amount != 500) {
            return 1;
        }
        unsigned char foreign[] = {2, 0xbc, 0x02, 0, 0}; // 700
        hud.MsgFunc_HealthInfo(nullptr, sizeof(foreign), foreign);
        if (gHUD.m_Health.amount != 500 || g_PlayerExtraInfo[2].sb_health != 700 ||
            g_PlayerExtraInfo[2].health != 0) {
            return 1;
        }
        hud.MsgFunc_HealthInfo(nullptr, sizeof(hidden), hidden);
        if (gHUD.m_Health.amount != 255) {
            return 1;
        }
    } else if (std::strcmp(argv[1], "startup") == 0) {
        maxClients = 0;
        CSE_ResetExtendedStats(true);
        CSE_ResetExtendedStats(false);
        if (CSE_LocalPlayerIndex() != 0) {
            return 1;
        }
        maxClients = 32;
        if (CSE_LocalPlayerIndex() != 1) {
            return 1;
        }
    } else if (std::strcmp(argv[1], "wide_life") == 0 || std::strcmp(argv[1], "wide_view") == 0) {
        const unsigned char wide[] = {1, 0, 0, 1, 0}; // 65536, a legacy delta boundary.
        CSE_HandleExtendedStats(cse::extstats::Stat::Health, wide, 5);
        if (std::strcmp(argv[1], "wide_life") == 0) {
            ApplyPredictedLife(100);
            ApplyPredictedLife(0);
            if (player.deaths != 0 || player.spawns != 1) {
                std::fputs("FAIL: truncated delta health holstered a living player's weapon\n", stderr);
                return 1;
            }
            const unsigned char dead[] = {1, 0, 0, 0, 0};
            CSE_HandleExtendedStats(cse::extstats::Stat::Health, dead, 5);
            ApplyPredictedLife(100);
            if (player.deaths != 1) {
                return 1;
            }
        } else {
            ref_params_s view;
            V_CalcViewRoll(&view);
            if (view.viewangles[ROLL] != 0) {
                std::fputs("FAIL: truncated delta health produced dead-view roll\n", stderr);
                return 1;
            }
            const unsigned char dead[] = {1, 0, 0, 0, 0};
            CSE_HandleExtendedStats(cse::extstats::Stat::Health, dead, 5);
            view.health = 100;
            g_iUser1 = 1;
            V_CalcViewRoll(&view);
            if (view.viewangles[ROLL] != 0) {
                return 1;
            }
            g_iUser1 = 0;
            V_CalcViewRoll(&view);
            if (view.viewangles[ROLL] != 80) {
                return 1;
            }
            CSE_ResetExtendedStats(true);
            view.viewangles[ROLL] = 0;
            V_CalcViewRoll(&view);
            if (view.viewangles[ROLL] != 0) {
                return 1;
            }
        }
    } else if (std::strcmp(argv[1], "demo") == 0) {
        Demo demo;
        gEngfuncs.pDemoAPI = &demo;
        playingDemo = true;
        clientTime = 20;
        const unsigned char packet[] = {1, 0x9f, 0x86, 1, 0};
        CSE_HandleExtendedStats(cse::extstats::Stat::Health, packet, 5);
        CSE_HandleExtendedStats(cse::extstats::Stat::Armor, packet, 5);
        clientTime = 5;
        CSE_UpdateLegacyHealth(1, 42, true);
        if (gHUD.m_Health.amount != 42 || gHUD.m_Battery.amount != 0) {
            std::fputs("FAIL: demo reversal kept canonical future values or discarded incoming legacy data\n", stderr);
            return 1;
        }
    } else if (std::strcmp(argv[1], "trailing") == 0) {
        unsigned char health[] = {42, 0};
        unsigned char armor[] = {0xf4, 1, 0};
        CHudHealth{}.MsgFunc_Health("Health", sizeof(health), health);
        CHudBattery{}.MsgFunc_Battery("Battery", sizeof(armor), armor);
        if (gHUD.m_Health.amount != 42 || gHUD.m_Battery.amount != 500) {
            std::fputs("FAIL: legacy numeric message with trailing bytes was ignored\n", stderr);
            return 1;
        }
    } else if (std::strcmp(argv[1], "legacy") == 0) {
        CSE_UpdateLegacyHealth(1, 5);
        if (gHUD.m_Health.amount != 100 || g_PlayerExtraInfo[1].health != 5) {
            std::fputs("FAIL: status/spectator cache write altered local canonical health\n", stderr);
            return 1;
        }
    } else {
        const unsigned char packet[] = {1, 0x9f, 0x86, 1, 0};
        CSE_HandleExtendedStats(cse::extstats::Stat::Health, packet, 5);
        if (viewport.updates != 0 || gHUD.m_Health.amount != 99999) {
            std::fputs("FAIL: own health packet invoked spectator gameplay UI\n", stderr);
            return 1;
        }
    }
    return 0;
}
