#include "production_boundary.h"
#include <cstring>
#include <climits>

int main() {
    ResetFixture();
    cls.challenge = -1234567;
    cls.authprotocol = PROTOCOL_STEAM;
    cls.GameServerSteamID = 0x0110000100000001ULL;
    cls.isVAC2Secure = true;
    FakeSteamUser().ticketSize = 128;
    CL_SendConnectPacket();
    Require(capturedPacket.size() > 128, "Steam connect packet captured");
    const char* header = reinterpret_cast<const char*>(capturedPacket.data());
    constexpr char ExpectedHeader[] = "\xff\xff\xff\xff" "connect 48 -1234567 \"\\prot\\3\\unique\\-1\\raw\\steam\\cdkey\\f528764d624db129b32c21fbca0cb8d6\" \"\\name\\CSE Native Auth Fixture\\rate\\25000\\cl_updaterate\\20\\cl_lw\\1\\cl_lc\\1\"\n";
    Require(capturedPacket.size() == sizeof(ExpectedHeader) - 1 + 128, "exact protocol packet extent");
    Require(std::memcmp(header, ExpectedHeader, sizeof(ExpectedHeader) - 1) == 0, "exact legacy header encoding");
    Require(capturedPacket[sizeof(ExpectedHeader) - 1] == 0xa5, "ticket immediately follows newline without NUL");
    Require(FakeSteamUser().initiations == 1, "first connection requests one ticket");
    if (sizeof(void*) == 8) {
        Require(FakeSteamUser().lastTicketBuffer > UINT32_MAX, "actual ticket storage above 4 GiB");
    }
    Require(FakeSteamUser().lastIP == LoopbackHostIP && FakeSteamUser().lastPort == FixtureServerPort,
        "SDK address arguments use host byte order");
    Require(FakeSteamUser().lastServer == cls.GameServerSteamID && FakeSteamUser().lastSecure,
        "server identity and secure policy reach SDK");
    const auto first = capturedPacket;
    CL_SendConnectPacket();
    Require(capturedPacket == first && FakeSteamUser().initiations == 1, "retry reuses current ticket");
    ++cls.GameServerSteamID;
    CL_SendConnectPacket();
    Require(FakeSteamUser().initiations == 2 && FakeSteamUser().terminations == 1,
        "changed server identity retires previous connection");
    TerminateFixtureConnection();
    Require(FakeSteamUser().terminations == 2, "explicit disconnect terminates SDK connection");
    FakeSteamUser().ticketSize = 0;
    capturedPacket.clear();
    CL_SendConnectPacket();
    Require(fixtureDisconnected && capturedPacket.empty(), "empty Steam ticket disconnects before transmission");
    for (const int invalidSize : {-1, 1025, INT_MAX, INT_MIN}) {
        TerminateFixtureConnection();
        FakeSteamUser().ticketSize = invalidSize;
        fixtureDisconnected = false;
        capturedPacket.clear();
        CL_SendConnectPacket();
        Require(fixtureDisconnected && capturedPacket.empty(), "invalid SDK ticket size cannot reach network");
        const int attempts = FakeSteamUser().initiations;
        FakeSteamUser().ticketSize = 128;
        fixtureDisconnected = false;
        CL_SendConnectPacket();
        Require(!fixtureDisconnected && !capturedPacket.empty(), "ticket generation recovers after failure");
        Require(FakeSteamUser().initiations == attempts + 1, "failed ticket is never reused from cache");
    }
    TerminateFixtureConnection();
    cls.authprotocol = PROTOCOL_HASHEDCDKEY;
    capturedPacket.clear();
    CL_SendConnectPacket();
    capturedPacket.push_back(0);
    Require(std::strstr(reinterpret_cast<const char*>(capturedPacket.data()), "\\prot\\2\\raw\\f528764d") != nullptr,
        "legacy hashed-host connection mode preserved");
    Require(capturedPacket[capturedPacket.size() - 2] == '\n', "non-Steam envelope has no ticket suffix");
    std::puts("actual_steam_connect_packet_passed=1");
}
