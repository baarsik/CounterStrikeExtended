#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "../../../src/common/native_int_count.h"
#include "../../../src/pm_shared/pm_shared.h"

#define REQUIRE(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); return 1; \
} } while (false)

constexpr int NET_SUCCESS = 0;
constexpr int NETAPI_REQUEST_DETAILS = 4;
constexpr int HUD_PRINTCENTER = 4;

struct net_response_s {
    int error = NET_SUCCESS;
    int context = 0;
    int type = NETAPI_REQUEST_DETAILS;
    int remote_address = 0;
    double ping = 0.025;
    void* response = nullptr;
};

struct NetworkApi {
    std::vector<int> capacities;
    const char* AdrToString(int*) {
        return "127.0.0.1:27015";
    }
    void SetValueForKey(char* info, const char* key, const char* value, int capacity) {
        capacities.push_back(capacity);
        const std::string result = std::string(info) + "\\" + key + "\\" + value;
        if (result.size() < static_cast<size_t>(capacity)) {
            std::memcpy(info, result.c_str(), result.size() + 1);
        }
    }
};

NetworkApi networkApi;
struct EngineFunctions {
    NetworkApi* pNetAPI = &networkApi;
} gEngfuncs;
#define NET_API (&networkApi)

struct CHudServers {
    struct request_t {};
    struct server_t {
        int remote_address = 0;
        char* info = nullptr;
        int ping = 0;
    };
    request_t active;
    request_t* m_pActiveList = &active;
    server_t* m_pServers = nullptr;
    int m_nActiveQueries = 1;
    request_t* FindRequest(int, request_t* head) {
        return head;
    }
    void RemoveServerFromList(request_t** head, request_t*) {
        *head = nullptr;
    }
    void AddServer(server_t** head, server_t* server) {
        *head = server;
    }
    ~CHudServers() {
        if (m_pServers) {
            delete[] m_pServers->info;
            delete m_pServers;
        }
    }
    void ServerResponse(net_response_s* response);
};

struct PlayerInfo {
    const char* name = nullptr;
} g_PlayerInfoList[2];
struct NameResult {
    std::string text;
    int offset;
};

std::string observedMode;
int g_iUser1 = 0;
struct CHudTextMessage {
    void MsgFunc_TextMsg(const char*, int bytes, void* data) {
        observedMode.assign(static_cast<const char*>(data), static_cast<size_t>(bytes));
    }
};
struct Hud {
    CHudTextMessage m_TextMessage;
} gHUD;

#include "hl_bodies.inc"

int main(int argc, char** argv) {
    REQUIRE(argc == 2);
    const std::string test = argv[1];
    if (test == "basename") {
        const std::pair<const char*, const char*> cases[] = {
            {"maps/c1a0.bsp", "c1a0"}, {"C:\\valve\\liblist.gam", "liblist"},
            {"maps.v1/map", "map"}, {"relative", "relative"}, {"file.a.b", "file.a"},
            {"", ""}, {"/", ""}, {".hidden", ""}, {"a", "a"}
        };
        for (const auto& item : cases) {
            char output[64];
            std::memset(output, 0x5A, sizeof(output));
            COM_FileBase(item.first, output);
            REQUIRE(std::strcmp(output, item.second) == 0);
            REQUIRE(output[std::strlen(item.second) + 1] == 0x5A);
        }
        const std::string large(100000, 'x');
        std::vector<char> output(large.size() + 2, 'z');
        COM_FileBase(large.c_str(), output.data());
        REQUIRE(std::strcmp(output.data(), large.c_str()) == 0 && output.back() == 'z');
    } else if (test == "newline") {
        const std::pair<const char*, const char*> cases[] = {
            {"", ""}, {"x", "x"}, {"\r", ""}, {"\n", ""}, {"word\n", "word"},
            {"word\r", "word"}, {"word\r\n", "word\r"}, {"word\n\n", "word\n"}
        };
        for (const auto& item : cases) {
            std::vector<char> value(std::strlen(item.first) + 3, 'z');
            std::strcpy(value.data() + 1, item.first);
            StripEndNewlineFromString(value.data() + 1);
            REQUIRE(std::strcmp(value.data() + 1, item.second) == 0);
            REQUIRE(value.front() == 'z' && value.back() == 'z');
        }
        const std::string large = std::string(100000, 'x') + "\n";
        std::vector<char> output(large.begin(), large.end());
        output.push_back(0);
        StripEndNewlineFromString(output.data());
        REQUIRE(std::strlen(output.data()) == large.size() - 1);
    } else if (test == "server-response") {
        for (size_t length : {size_t(0), size_t(1), size_t(2047), size_t(5000)}) {
            const std::string text(length, 'x');
            net_response_s response;
            response.response = const_cast<char*>(text.c_str());
            CHudServers servers;
            networkApi.capacities.clear();
            servers.ServerResponse(&response);
            REQUIRE(servers.m_pActiveList == nullptr && servers.m_nActiveQueries == 0);
            REQUIRE(servers.m_pServers != nullptr && servers.m_pServers->ping == 25);
            REQUIRE(std::string(servers.m_pServers->info) == text + "\\address\\127.0.0.1:27015\\ping\\25");
            REQUIRE(networkApi.capacities == std::vector<int>(2, static_cast<int>(length) + 101));
        }
        for (int error : {0, 1}) {
            CHudServers servers;
            net_response_s response;
            response.error = error;
            servers.ServerResponse(&response);
            REQUIRE(servers.m_pActiveList == nullptr && servers.m_nActiveQueries == 0);
            REQUIRE(servers.m_pServers == nullptr);
        }
        int result = 0;
        REQUIRE(cse::counts::TryAddIntCount(101, static_cast<size_t>(INT_MAX) - 101, result));
        REQUIRE(result == INT_MAX);
        REQUIRE(!cse::counts::TryAddIntCount(101, static_cast<size_t>(INT_MAX) - 100, result));
        REQUIRE(!cse::counts::TryAddIntCount(101, SIZE_MAX, result));
    } else if (test == "spectator-name") {
        for (size_t length : {size_t(0), size_t(1), size_t(254), size_t(255), size_t(256), size_t(100000)}) {
            const std::string name(length, 'x');
            const auto result = SpectatorName(name.c_str());
            const size_t expected = std::min(length, size_t(255));
            REQUIRE(result.text == std::string(expected, 'x'));
            REQUIRE(result.offset == static_cast<int>(expected) * 3);
        }
    } else if (test == "mode-message") {
        for (int mode = OBS_CHASE_LOCKED; mode <= OBS_MAP_CHASE; ++mode) {
            observedMode.clear();
            ModeMessage(mode);
            const std::string expected = std::string(1, HUD_PRINTCENTER) + "#Spec_Mode" + std::to_string(mode) + '\0';
            REQUIRE(observedMode == expected);
        }
    } else {
        return 2;
    }
    std::printf("PASS hl-%s\n", test.c_str());
    return 0;
}
