#include "ServerBrowserModel.h"
#include <stdexcept>
#include <iostream>
using namespace cse::server_browser;
// Exact pinned interface; unrelated lobby operations must never be invoked by the adapter.
struct Matchmaking final : ISteamMatchmaking {
    std::vector<FavoriteRecord> records;
    bool apply = true;
    bool failRead = false;
    bool mismatchQuery = false;
    int opaqueResult = -77;
    int GetFavoriteGameCount() override {
        return static_cast<int>(records.size());
    }
    bool GetFavoriteGame(int index, AppId_t* app, uint32* ip, uint16* connectionPort,
        uint16* queryPort, uint32* flags, uint32* timestamp) override {
        if (failRead || index < 0 || static_cast<size_t>(index) >= records.size()) {
            return false;
        }
        const auto& record = records[static_cast<size_t>(index)];
        *app = record.app;
        *ip = record.address.ip;
        *connectionPort = record.address.connectionPort;
        *queryPort = record.address.queryPort;
        *flags = record.flags;
        *timestamp = record.lastPlayed;
        return true;
    }
    int AddFavoriteGame(AppId_t app, uint32 ip, uint16 connectionPort, uint16 queryPort,
        uint32 flags, uint32 timestamp) override {
        if (apply) {
            records.push_back({app, {ip, connectionPort, static_cast<uint16>(mismatchQuery ? 1 : queryPort)},
                flags, timestamp});
        }
        return opaqueResult;
    }
    bool RemoveFavoriteGame(AppId_t, uint32, uint16, uint16, uint32) override {
        return false;
    }
    SteamAPICall_t RequestLobbyList() override {
        throw std::logic_error("unexpected lobby call");
    }
    void AddRequestLobbyListStringFilter([[maybe_unused]] const char *pchKeyToMatch, [[maybe_unused]] const char *pchValueToMatch, [[maybe_unused]] ELobbyComparison eComparisonType) override {
        throw std::logic_error("unexpected lobby call");
    }
    void AddRequestLobbyListNumericalFilter([[maybe_unused]] const char *pchKeyToMatch, [[maybe_unused]] int nValueToMatch, [[maybe_unused]] ELobbyComparison eComparisonType) override {
        throw std::logic_error("unexpected lobby call");
    }
    void AddRequestLobbyListNearValueFilter([[maybe_unused]] const char *pchKeyToMatch, [[maybe_unused]] int nValueToBeCloseTo) override {
        throw std::logic_error("unexpected lobby call");
    }
    void AddRequestLobbyListFilterSlotsAvailable([[maybe_unused]] int nSlotsAvailable) override {
        throw std::logic_error("unexpected lobby call");
    }
    void AddRequestLobbyListDistanceFilter([[maybe_unused]] ELobbyDistanceFilter eLobbyDistanceFilter) override {
        throw std::logic_error("unexpected lobby call");
    }
    void AddRequestLobbyListResultCountFilter([[maybe_unused]] int cMaxResults) override {
        throw std::logic_error("unexpected lobby call");
    }
    void AddRequestLobbyListCompatibleMembersFilter([[maybe_unused]] CSteamID steamIDLobby) override {
        throw std::logic_error("unexpected lobby call");
    }
    CSteamID GetLobbyByIndex([[maybe_unused]] int iLobby) override {
        throw std::logic_error("unexpected lobby call");
    }
    SteamAPICall_t CreateLobby([[maybe_unused]] ELobbyType eLobbyType, [[maybe_unused]] int cMaxMembers) override {
        throw std::logic_error("unexpected lobby call");
    }
    SteamAPICall_t JoinLobby([[maybe_unused]] CSteamID steamIDLobby) override {
        throw std::logic_error("unexpected lobby call");
    }
    void LeaveLobby([[maybe_unused]] CSteamID steamIDLobby) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool InviteUserToLobby([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] CSteamID steamIDInvitee) override {
        throw std::logic_error("unexpected lobby call");
    }
    int GetNumLobbyMembers([[maybe_unused]] CSteamID steamIDLobby) override {
        throw std::logic_error("unexpected lobby call");
    }
    CSteamID GetLobbyMemberByIndex([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] int iMember) override {
        throw std::logic_error("unexpected lobby call");
    }
    const char *GetLobbyData([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] const char *pchKey) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool SetLobbyData([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] const char *pchKey, [[maybe_unused]] const char *pchValue) override {
        throw std::logic_error("unexpected lobby call");
    }
    int GetLobbyDataCount([[maybe_unused]] CSteamID steamIDLobby) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool GetLobbyDataByIndex([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] int iLobbyData, [[maybe_unused]] char *pchKey, [[maybe_unused]] int cchKeyBufferSize, [[maybe_unused]] char *pchValue, [[maybe_unused]] int cchValueBufferSize) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool DeleteLobbyData([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] const char *pchKey) override {
        throw std::logic_error("unexpected lobby call");
    }
    const char *GetLobbyMemberData([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] CSteamID steamIDUser, [[maybe_unused]] const char *pchKey) override {
        throw std::logic_error("unexpected lobby call");
    }
    void SetLobbyMemberData([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] const char *pchKey, [[maybe_unused]] const char *pchValue) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool SendLobbyChatMsg([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] const void *pvMsgBody, [[maybe_unused]] int cubMsgBody) override {
        throw std::logic_error("unexpected lobby call");
    }
    int GetLobbyChatEntry([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] int iChatID, [[maybe_unused]] STEAM_OUT_STRUCT() CSteamID *pSteamIDUser, [[maybe_unused]] void *pvData, [[maybe_unused]] int cubData, [[maybe_unused]] EChatEntryType *peChatEntryType) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool RequestLobbyData([[maybe_unused]] CSteamID steamIDLobby) override {
        throw std::logic_error("unexpected lobby call");
    }
    void SetLobbyGameServer([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] uint32 unGameServerIP, [[maybe_unused]] uint16 unGameServerPort, [[maybe_unused]] CSteamID steamIDGameServer) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool GetLobbyGameServer([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] uint32 *punGameServerIP, [[maybe_unused]] uint16 *punGameServerPort, [[maybe_unused]] STEAM_OUT_STRUCT() CSteamID *psteamIDGameServer) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool SetLobbyMemberLimit([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] int cMaxMembers) override {
        throw std::logic_error("unexpected lobby call");
    }
    int GetLobbyMemberLimit([[maybe_unused]] CSteamID steamIDLobby) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool SetLobbyType([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] ELobbyType eLobbyType) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool SetLobbyJoinable([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] bool bLobbyJoinable) override {
        throw std::logic_error("unexpected lobby call");
    }
    CSteamID GetLobbyOwner([[maybe_unused]] CSteamID steamIDLobby) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool SetLobbyOwner([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] CSteamID steamIDNewOwner) override {
        throw std::logic_error("unexpected lobby call");
    }
    bool SetLinkedLobby([[maybe_unused]] CSteamID steamIDLobby, [[maybe_unused]] CSteamID steamIDLobbyDependent) override {
        throw std::logic_error("unexpected lobby call");
    }
};
int main() {
    Matchmaking provider;
    SteamFavoriteStore store(&provider);
    FavoriteRecord record{10, {0x7f000001U, 27015, 27016}, 3, 123};
    if (!store.Add(record).ok) {
        std::cerr << "FAIL: readback-confirmed write must not interpret opaque int as failure\n";
        return 1;
    }
    provider.apply = false;
    provider.opaqueResult = 0;
    record.address.connectionPort = 27017;
    if (store.Add(record).ok) {
        std::cerr << "FAIL: unconfirmed write must not interpret opaque int as success\n";
        return 1;
    }
    provider.apply = true;
    provider.mismatchQuery = true;
    if (store.Add(record).ok) {
        return 1;
    }
    provider.mismatchQuery = false;
    provider.failRead = true;
    if (store.Add(record).ok) {
        return 1;
    }
    std::cout << "1 adapter readback case passed\n";
}
