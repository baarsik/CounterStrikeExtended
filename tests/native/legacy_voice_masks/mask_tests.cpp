#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "voice_status.h"

cl_enginefunc_t gEngfuncs = {};

namespace {

constexpr int PlayerCount = VOICE_MAX_PLAYERS;
constexpr int PlayerIdentityBytes = 16;
constexpr int WireWordBytes = 4;
constexpr int MaskMessageBytes = WireWordBytes * 2 + 1;
constexpr std::uint32_t SeedMultiplier = 1664525;
constexpr std::uint32_t SeedIncrement = 1013904223;

std::uint32_t validPlayers = 0;
bool modEnabled = false;
bool debugEnabled = false;
bool levelPresent = true;
std::vector<std::string> reliable;
std::vector<std::string> unreliable;
std::vector<std::string> diagnostics;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

const char* LevelName() {
    return levelPresent ? "maps/authored.bsp" : "";
}

float CvarValue(char* name) {
    if (std::strcmp(name, "voice_clientdebug") == 0) {
        return debugEnabled ? 1.0f : 0.0f;
    }
    Require(std::strcmp(name, "voice_modenable") == 0, "Unexpected cvar lookup");
    return modEnabled ? 1.0f : 0.0f;
}

int UniqueIdentity(int player, char identity[PlayerIdentityBytes]) {
    Require(player >= 1 && player <= PlayerCount, "Player index changed");
    std::memset(identity, 0, PlayerIdentityBytes);
    identity[0] = static_cast<char>(player);
    return (validPlayers & (std::uint32_t{1} << (player - 1))) != 0;
}

int ReliableCommand(char* text) {
    reliable.emplace_back(text);
    return 1;
}

int UnreliableCommand(char* text) {
    unreliable.emplace_back(text);
    return 1;
}

void ConsoleText(const char* text) {
    diagnostics.emplace_back(text);
}

float ClientTime() {
    return 12.5f;
}

void EncodeWord(unsigned char* destination, std::uint32_t value) {
    constexpr int ByteBits = 8;
    for (int byte = 0; byte < WireWordBytes; ++byte) {
        destination[byte] = static_cast<unsigned char>(value >> (byte * ByteBits));
    }
}

void SetBans(CVoiceStatus& voice, std::uint32_t mask) {
    for (int player = 1; player <= PlayerCount; ++player) {
        char identity[PlayerIdentityBytes] = {};
        identity[0] = static_cast<char>(player);
        voice.m_BanMgr.SetPlayerBan(identity, (mask & (std::uint32_t{1} << (player - 1))) != 0);
    }
}

void RunCase(unsigned int ordinal, std::uint32_t audible, std::uint32_t serverBans,
    std::uint32_t localBans, bool force, int enabledByte) {
    CVoiceStatus voice;
    unsigned char message[MaskMessageBytes] = {};
    EncodeWord(message, audible);
    EncodeWord(message + WireWordBytes, serverBans);
    message[MaskMessageBytes - 1] = static_cast<unsigned char>(enabledByte);
    diagnostics.clear();
    reliable.clear();
    unreliable.clear();
    voice.HandleVoiceMaskMsg(sizeof(message), message);
    Require(READ_OK() != 0, "Actual parser rejected complete voice mask");
    Require(voice.m_AudiblePlayers.GetDWord(0) == audible, "Audible mask changed");
    Require(voice.m_ServerBannedPlayers.GetDWord(0) == serverBans, "Server mask changed");
    Require(voice.m_bServerModEnable == enabledByte, "Enable byte changed");
    if (debugEnabled) {
        char expected[256];
        Require(diagnostics.size() == 3, "Mask diagnostic count changed");
        std::snprintf(expected, sizeof(expected), "    - m_AudiblePlayers[0] = %lu\n",
            static_cast<unsigned long>(audible));
        Require(diagnostics[1] == expected, "Audible diagnostic value changed");
        std::snprintf(expected, sizeof(expected), "    - m_ServerBannedPlayers[0] = %lu\n",
            static_cast<unsigned long>(serverBans));
        Require(diagnostics[2] == expected, "Server diagnostic value changed");
    }
    SetBans(voice, localBans);
    voice.UpdateServerState(force);
    const std::uint32_t actualBanMask = localBans & validPlayers;
    const std::uint32_t actualServerMask = serverBans & validPlayers;
    const bool sendsMask = levelPresent && (force || actualBanMask != actualServerMask);
    Require(unreliable.size() == static_cast<std::size_t>(sendsMask), "Mask send decision changed");
    const bool sendsEnable = levelPresent && (force || enabledByte != static_cast<int>(modEnabled));
    Require(reliable.size() == static_cast<std::size_t>(sendsEnable), "Enable send decision changed");
    if (sendsMask) {
        char expected[64];
        std::snprintf(expected, sizeof(expected), "vban %x", actualBanMask);
        Require(unreliable[0] == expected, "Encoded ban command changed");
    }
    if (sendsEnable) {
        Require(reliable[0] == (modEnabled ? "VModEnable 1" : "VModEnable 0"), "Enable command changed");
    }
    Require(voice.m_LastUpdateServerState == (levelPresent ? ClientTime() : 0.0f), "Update time changed");
    std::printf("%u %08x %08x %08x %08x %d %d %zu %zu\n", ordinal, audible, serverBans,
        actualBanMask, actualServerMask, enabledByte, sendsMask, reliable.size(), unreliable.size());
    for (const std::string& text : diagnostics) {
        std::fwrite(text.data(), 1, text.size(), stdout);
    }
}

}

// Controlled construction binds the actual complete class. Unrelated virtual
// entry points are fatal; no HUD, audio, clipboard, file or provider is opened.
CVoiceStatus::CVoiceStatus() : m_LastUpdateServerState(0), m_bServerModEnable(0) {
}

CVoiceStatus::~CVoiceStatus() {
}

int CVoiceStatus::Init(IVoiceStatusHelper*, vgui::Panel**) {
    std::abort();
}

int CVoiceStatus::VidInit() {
    std::abort();
}

#include "actual_ban_methods.inc"
#include "actual_voice_methods.inc"

int main() {
    static_assert(sizeof(uint32) == WireWordBytes, "Voice wire words remain 32 bits");
    gEngfuncs.pfnGetLevelName = LevelName;
    gEngfuncs.pfnGetCvarFloat = CvarValue;
    gEngfuncs.GetPlayerUniqueID = UniqueIdentity;
    gEngfuncs.pfnServerCmd = ReliableCommand;
    gEngfuncs.pfnServerCmdUnreliable = UnreliableCommand;
    gEngfuncs.pfnConsolePrint = ConsoleText;
    gEngfuncs.GetClientTime = ClientTime;
    std::uint32_t seed = 0x01234567;
    constexpr unsigned int CaseCount = 512;
    for (unsigned int ordinal = 0; ordinal < CaseCount; ++ordinal) {
        seed = seed * SeedMultiplier + SeedIncrement;
        const std::uint32_t audible = ordinal < PlayerCount ? std::uint32_t{1} << ordinal : seed;
        seed = seed * SeedMultiplier + SeedIncrement;
        const std::uint32_t serverBans = ordinal < PlayerCount ? 0 : seed;
        seed = seed * SeedMultiplier + SeedIncrement;
        const std::uint32_t localBans = ordinal < PlayerCount ? audible : seed;
        seed = seed * SeedMultiplier + SeedIncrement;
        validPlayers = ordinal < PlayerCount ? UINT32_MAX : seed;
        debugEnabled = ordinal < PlayerCount || (ordinal & 1) == 0;
        modEnabled = (ordinal & 2) != 0;
        levelPresent = ordinal < PlayerCount || (ordinal & 4) == 0;
        RunCase(ordinal, audible, serverBans, localBans, (ordinal & 8) != 0, ordinal & 0xff);
    }
    validPlayers = UINT32_MAX;
    debugEnabled = true;
    modEnabled = false;
    levelPresent = true;
    constexpr std::uint32_t HighestPlayerMask = std::uint32_t{1} << (PlayerCount - 1);
    RunCase(CaseCount, UINT32_MAX, HighestPlayerMask, HighestPlayerMask, true, 0);
    RunCase(CaseCount + 1, UINT32_MAX, HighestPlayerMask, HighestPlayerMask, false, 0);
    return 0;
}
