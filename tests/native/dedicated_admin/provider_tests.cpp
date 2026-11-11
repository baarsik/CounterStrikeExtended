// Real provider implementation; engine services below are controlled local fixtures.
#include "sv_remoteaccess.cpp"
#include <cstdlib>
#include <string>
#include <vector>
#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                                       \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)
server_t sv{};
server_static_t svs{};
DLL_FUNCTIONS gEntityInterface{};
double realtime = 0;
int numuserfilters = 0;
int numipfilters = 0;
userfilter_t userfilters[MAX_USERFILTERS]{};
ipfilter_t ipfilters[MAX_IPFILTERS]{};
netadr_t net_local_adr{};
char com_gamedir[GAME_DIRECTORY_BYTES] = "/fixture/valve";
cvar_t mapcyclefile{const_cast<char*>("mapcyclefile"), const_cast<char*>("mapcycle.txt"), 0, 0, nullptr};
cvar_t hostname{const_cast<char*>("hostname"), const_cast<char*>("Fixture server"), 0, 0, nullptr};
AdminProviderStatus Steam_QueryAdminStatus() {
    return {};
}
std::string commands;
std::string cvarValue;
std::string mapcycle;
int executions = 0;
bool mapcycleMissing = false;
int mapcycleLoads = 0;
int mapcycleFrees = 0;
void Cbuf_AddText(const char* text) {
    commands += text;
}
void Cbuf_Execute() {
    ++executions;
}
cvar_t* Cvar_FindVar(const char* name) {
    return std::strcmp(name, "hostname") == 0 ? &hostname : nullptr;
}
void Cvar_DirectSet(cvar_t* var, const char* value) {
    cvarValue = value;
    var->string = cvarValue.data();
}
FileHandle_t FS_Open(const char*, const char*) {
    return reinterpret_cast<FileHandle_t>(1);
}
void FS_Close(FileHandle_t) {
}
int FS_Write(const void* data, int size, int count, FileHandle_t) {
    mapcycle.assign(static_cast<const char*>(data), size * count);
    return count;
}
void Con_Printf(const char*, ...) {
}
char* SV_GetIDString(USERID_t*) {
    static char id[] = "STEAM_0:1:123";
    return id;
}
char* NET_AdrToString(netadr_t) {
    static char address[] = "127.0.0.1:27015";
    return address;
}
int mapIndex = 0;
const char* Sys_FindFirst(const char*, char*) {
    mapIndex = 0;
    return "crossfire.bsp";
}
const char* Sys_FindNext(char*) {
    return ++mapIndex == 1 ? "boot_camp.bsp" : nullptr;
}
void Sys_FindClose() {
}
const char* FS_GetLocalPath(const char* name, char* local, int capacity) {
    const std::string path = std::string(com_gamedir) + "/" + name;
    std::snprintf(local, capacity, "%s", path.c_str());
    return local;
}
void GetStatsString(char* buffer, int size) {
    std::snprintf(buffer, size, "1 2 3 4 5 6 7");
}
double Sys_FloatTime() {
    return 100;
}
int Host_GetStartTime() {
    return 50;
}
byte* COM_LoadFileForMe(char*, int* length) {
    *length = static_cast<int>(mapcycle.size());
    if (mapcycleMissing) {
        return nullptr;
    }
    ++mapcycleLoads;
    auto* data = static_cast<byte*>(std::malloc(mapcycle.size() + 1));
    std::memcpy(data, mapcycle.c_str(), mapcycle.size() + 1);
    return data;
}
void COM_FreeFile(void* data) {
    ++mapcycleFrees;
    std::free(data);
}
void AppendInt(std::vector<char>& packet, int value) {
    auto* start = reinterpret_cast<const char*>(&value);
    packet.insert(packet.end(), start, start + sizeof(value));
}
std::vector<char> Request(int type, const std::string& name, const std::string& value = "") {
    std::vector<char> packet;
    AppendInt(packet, 7);
    AppendInt(packet, type);
    packet.insert(packet.end(), name.c_str(), name.c_str() + name.size() + 1);
    packet.insert(packet.end(), value.c_str(), value.c_str() + value.size() + 1);
    return packet;
}
void Send(const std::vector<char>& packet) {
    g_ServerRemoteAccess.WriteDataRequest(packet.data(), static_cast<int>(packet.size()));
}
IGameServerData* ActualProvider() {
    return &g_ServerRemoteAccess;
}
void PopulateProviderFixture() {
    gEntityInterface.pfnGetGameDescription = [] { return "Half-Life fixture"; };
    std::strcpy(sv.name, "crossfire");
    static client_t client{};
    static edict_t edict{};
    client.active = true;
    std::strcpy(client.name, "Fixture player");
    client.edict = &edict;
    svs.clients = &client;
    svs.maxclients = 1;
    numuserfilters = 1;
    userfilters[0].banTime = 10;
}
#ifndef CSE_PROVIDER_NO_MAIN
int main(int argc, char** argv) {
    CHECK(argc == 2);
    std::string mode = argv[1];
    if (mode == "oversized_response_queue") {
        mapcycle.assign(70000, 'x');
        Send(Request(SERVERDATA_REQUESTVALUE, "mapcycle"));
        g_ServerRemoteAccess.SendMessageToAdminUI("retired-update");
        char response[65536];
        CHECK(g_ServerRemoteAccess.ReadDataResponse(nullptr, sizeof(response)) == 0);
        CHECK(g_ServerRemoteAccess.ReadDataResponse(response, 0) == 0);
        CHECK(g_ServerRemoteAccess.ReadDataResponse(response, -1) == 0);
        const int bytes = g_ServerRemoteAccess.ReadDataResponse(response, sizeof(response));
        CHECK(bytes > 8 && std::strcmp(response + 8, "retired-update") == 0);
        CHECK(g_ServerRemoteAccess.ReadDataResponse(response, sizeof(response)) == 0);
        Send(Request(SERVERDATA_REQUESTVALUE, "hostname"));
        char small[1] = {'z'};
        CHECK(g_ServerRemoteAccess.ReadDataResponse(small, sizeof(small)) == 0);
        CHECK(small[0] == 'z');
        CHECK(g_ServerRemoteAccess.ReadDataResponse(response, sizeof(response)) == 0);
        g_ServerRemoteAccess.SendMessageToAdminUI("new-update");
        CHECK(g_ServerRemoteAccess.ReadDataResponse(response, sizeof(response)) > 8);
        CHECK(std::strcmp(response + 8, "new-update") == 0);
    } else if (mode == "provider_malformed") {
        auto packet = Request(2, "status");
        packet.resize(packet.size() - 2); // command terminator and empty-value terminator absent
        Send(packet);
        CHECK(executions == 0 && commands.empty());
        Send(Request(2, std::string(512, 'x')));
        CHECK(executions == 0 && commands.empty());
        g_ServerRemoteAccess.WriteDataRequest(nullptr, -1);
        CHECK(executions == 0);
        for (std::int32_t type : {-1, 3, 2147483647}) {
            auto malformed = Request(2, "quit");
            std::memcpy(malformed.data() + sizeof(std::int32_t), &type, sizeof(type));
            Send(malformed);
        }
        auto negativeID = Request(2, "quit");
        const std::int32_t invalidID = -1;
        std::memcpy(negativeID.data(), &invalidID, sizeof(invalidID));
        Send(negativeID);
        Send(Request(1, std::string(256, 'v'), "bad"));
        Send(Request(1, "hostname", std::string(512, 'v')));
        auto missingValue = Request(1, "hostname", "bad");
        missingValue.pop_back();
        Send(missingValue);
        CHECK(executions == 0 && commands.empty() && cvarValue.empty());
    } else {
        Send(Request(2, "say \"hello world\""));
        CHECK(commands == "say \"hello world\"\n" && executions == 1);
        Send(Request(1, "hostname", "zażółć server"));
        CHECK(cvarValue == "zażółć server");
        Send(Request(1, "mapcycle", "crossfire\nboot_camp\n"));
        CHECK(mapcycle == std::string("crossfire\nboot_camp\n\0", 21));
        for (const char* name : {"hostname", "mapcycle", "playerlist", "banlist", "stats", "uptime", "ipaddress"}) {
            Send(Request(0, name));
            char response[65536];
            int count = g_ServerRemoteAccess.ReadDataResponse(response, sizeof(response));
            CHECK(count > 12 && std::strcmp(response + 8, name) == 0);
        }
        g_ServerRemoteAccess.SendMessageToAdminUI("UpdatePlayers");
        char response[128];
        CHECK(g_ServerRemoteAccess.ReadDataResponse(response, sizeof(response)) > 8);
        CHECK(std::strcmp(response + 8, "UpdatePlayers") == 0);
    }
}

#endif
