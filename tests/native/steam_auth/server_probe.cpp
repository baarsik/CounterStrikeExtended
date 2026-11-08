#include "production_boundary.h"
#include "steam_service.h"
#include "isteamuser.h"
#include "isteamapps.h"
#include "isteamutils.h"
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>
#include <stdexcept>

namespace {
using Clock = std::chrono::steady_clock;
constexpr auto PollInterval = std::chrono::milliseconds(10);
constexpr auto ChallengeTimeout = std::chrono::seconds(5);
constexpr auto AuthenticationObservation = std::chrono::seconds(15);
#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket InvalidSocket = INVALID_SOCKET;
void CloseSocket(Socket socket) {
    closesocket(socket);
}
#else
using Socket = int;
constexpr Socket InvalidSocket = -1;
void CloseSocket(Socket socket) {
    close(socket);
}
#endif
Socket connection = InvalidSocket;
void Check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
void Send(int length, const void* data) {
    const int sent = static_cast<int>(send(connection, static_cast<const char*>(data), length, 0));
    Check(sent == length, "complete UDP datagram transmission");
}
int Receive(byte* buffer, int capacity) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(connection, &readable);
    timeval immediate{};
    const int result = select(static_cast<int>(connection + 1), &readable, nullptr, nullptr, &immediate);
    Check(result >= 0, "UDP polling");
    if (result == 0) {
        return 0;
    }
    const int received = static_cast<int>(recv(connection, reinterpret_cast<char*>(buffer), capacity, 0));
    Check(received > 0, "UDP receive");
    return received;
}
struct Session {
    bool initialized = false;
    ~Session() {
        if (initialized) {
            TerminateFixtureConnection();
            SteamAPI_Shutdown();
        }
        if (connection != InvalidSocket) {
            CloseSocket(connection);
            connection = InvalidSocket;
        }
#if defined(_WIN32)
        WSACleanup();
#endif
    }
};
class DenialObserver {
public:
    int denials = 0;
    unsigned int lastReason = 0;
    DenialObserver() : denial(this, &DenialObserver::OnDenial) {}
private:
    void OnDenial(ClientGameServerDeny_t* result) {
        ++denials;
        lastReason = result->m_uReason;
    }
    CCallback<DenialObserver, ClientGameServerDeny_t> denial;
};
}

int main(int argc, char** argv) {
    if (argc != 2 || std::strcmp(argv[1], "--connect-isolated-win32-loopback") != 0) {
        std::fputs("Explicit isolated network fixture invocation required\n", stderr);
        return 2;
    }
    Session session;
    try {
#if defined(_WIN32)
        WSADATA data{};
        Check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "Winsock startup");
#endif
        ResetFixture();
        Check(SteamAPI_Init(), "real native Steam client initialization");
        session.initialized = true;
        DenialObserver denials;
        Check(SteamUser() && SteamApps() && SteamUtils(), "required Steam interfaces");
        Check(reinterpret_cast<std::uintptr_t>(SteamUser()) > UINT32_MAX, "real Steam user interface above 4 GiB");
        Check(SteamUser()->BLoggedOn() && SteamApps()->BIsSubscribed(), "logged-in licensed game");
        Check(SteamUtils()->GetAppID() == 10, "Counter-Strike App10 identity");
        connection = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        Check(connection != InvalidSocket, "UDP socket creation");
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = htonl(LoopbackHostIP);
        server.sin_port = htons(FixtureServerPort);
        Check(connect(connection, reinterpret_cast<const sockaddr*>(&server), sizeof(server)) == 0,
            "isolated loopback server endpoint");
        constexpr char ChallengeRequest[] = "\xff\xff\xff\xff" "getchallenge steam\n";
        Send(sizeof(ChallengeRequest) - 1, ChallengeRequest);
        byte buffer[65536];
        bool challengeReceived = false;
        const auto challengeDeadline = Clock::now() + ChallengeTimeout;
        while (Clock::now() < challengeDeadline && !challengeReceived) {
            SteamAPI_RunCallbacks();
            const int length = Receive(buffer, sizeof(buffer) - 1);
            if (length >= 5) {
                Check(std::memcmp(buffer, "\xff\xff\xff\xff", 4) == 0, "connectionless challenge envelope");
                buffer[length] = 0;
                std::istringstream arguments(reinterpret_cast<const char*>(buffer + 4));
                std::string marker;
                int protocol = 0;
                int secure = 0;
                std::uint64_t serverIdentity = 0;
                Check(static_cast<bool>(arguments >> marker >> cls.challenge >> protocol >> serverIdentity >> secure),
                    "five-field Steam challenge");
                Check(marker == "A00000000" && protocol == PROTOCOL_STEAM && secure == 1,
                    "unchanged server requires secure protocol48 Steam authentication");
                Check(CSteamID(serverIdentity).IsValid(), "valid server Steam identity");
                cls.GameServerSteamID = serverIdentity;
                cls.authprotocol = PROTOCOL_STEAM;
                cls.isVAC2Secure = true;
                challengeReceived = true;
            }
            std::this_thread::sleep_for(PollInterval);
        }
        Check(challengeReceived, "server challenge before deadline");
        fixturePacketSink = &Send;
        CL_SendConnectPacket();
        Check(!fixtureDisconnected && !capturedPacket.empty(), "actual production connection packet transmitted");
        std::size_t headerBytes = 0;
        while (headerBytes < capturedPacket.size() && capturedPacket[headerBytes++] != '\n') {
            // Find the unchanged textual envelope without persisting its authentication suffix.
        }
        const std::size_t ticketBytes = capturedPacket.size() - headerBytes;
        // Never log, hash or persist the ticket bytes.
        capturedPacket.clear();
        Check(ticketBytes > 0 && ticketBytes <= 1024, "nonempty bounded legacy Steam authentication ticket");
        std::printf("secure_challenge=1 native_legacy_ticket_bytes=%zu\n", ticketBytes);
        std::fflush(stdout);
        bool accepted = false;
        unsigned int channelPackets = 0;
        const auto observationDeadline = Clock::now() + AuthenticationObservation;
        while (Clock::now() < observationDeadline) {
            SteamAPI_RunCallbacks();
            const int length = Receive(buffer, sizeof(buffer) - 1);
            if (length >= 5) {
                if (std::memcmp(buffer, "\xff\xff\xff\xff", 4) == 0) {
                    if (buffer[4] == 'B') {
                        accepted = true;
                    } else {
                        std::fprintf(stderr, "connectionless_response_type=%u\n", buffer[4]);
                        throw std::runtime_error("server rejected connection or sent unexpected response");
                    }
                } else {
                    ++channelPackets;
                }
            }
            if (denials.denials != 0) {
                std::fprintf(stderr, "steam_denials=%d reason=%u\n", denials.denials, denials.lastReason);
                throw std::runtime_error("Steam client denied server connection");
            }
            std::this_thread::sleep_for(PollInterval);
        }
        Check(accepted, "server connection acceptance response");
        std::printf("server_connection_response=1 steam_denials=0 channel_packets=%u\n", channelPackets);
        std::puts("native_legacy_connection_probe_passed=1");
        // Authentication approval is independently correlated with fresh unchanged-server logs.
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "fixture_failure=%s\n", error.what());
        return 1;
    }
}
