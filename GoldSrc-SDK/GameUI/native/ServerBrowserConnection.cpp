#include "ServerBrowserConnection.h"
#include <algorithm>
#include <cstring>

namespace cse::server_browser {
namespace {
void Erase(char *bytes, size_t size) {
    volatile char *output = bytes;
    while (size) {
        *output++ = 0;
        --size;
    }
}
bool ValidPassword(std::string_view value) {
    if (value.size() >= ServerPasswordCapacity || value.find("..") != std::string_view::npos) {
        return false;
    }
    size_t index = 0;
    while (index < value.size()) {
        const auto first = static_cast<unsigned char>(value[index++]);
        if (first < ' ' || first == 127 || first == '\\' || first == '"') {
            return false;
        }
        if (first < 128) {
            continue;
        }
        constexpr unsigned char ContinuationMask = 0xc0;
        constexpr unsigned char ContinuationTag = 0x80;
        constexpr uint32_t MaximumUnicode = 0x10ffff;
        constexpr uint32_t SurrogateFirst = 0xd800;
        constexpr uint32_t SurrogateLast = 0xdfff;
        size_t continuation = 0;
        uint32_t codepoint = 0;
        uint32_t minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            continuation = 1;
            codepoint = first & 0x1f;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuation = 2;
            codepoint = first & 0x0f;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation = 3;
            codepoint = first & 0x07;
            minimum = 0x10000;
        } else {
            return false;
        }
        constexpr unsigned int PayloadBits = 6;
        constexpr unsigned char PayloadMask = 0x3f;
        while (continuation--) {
            if (index == value.size()) {
                return false;
            }
            const auto next = static_cast<unsigned char>(value[index++]);
            if ((next & ContinuationMask) != ContinuationTag) {
                return false;
            }
            codepoint = (codepoint << PayloadBits) | (next & PayloadMask);
        }
        if (codepoint < minimum || codepoint > MaximumUnicode ||
            (codepoint >= SurrogateFirst && codepoint <= SurrogateLast)) {
            return false;
        }
    }
    return true;
}
}

ServerBrowserConnection::ServerBrowserConnection(IConnectionEngine &engine, ServerBrowserModel &model, AppId_t app,
                                                 std::string game)
    : engine_(engine), model_(model), app_(app), game_(std::move(game)) {}

ServerBrowserConnection::~ServerBrowserConnection() {
    Retire();
}

bool ServerBrowserConnection::CanConnect(const Row &row) const {
    return row.app == app_ && row.gameDirectory == game_;
}

ConnectResult ServerBrowserConnection::Begin(const Row &row, std::string_view password) {
    if (!CanConnect(row)) {
        return ConnectResult::OtherGame;
    }
    Address parsed;
    if (!row.responded || row.failed || !row.key.queryPort || !ParseAddress(ConnectionAddress(row.key), parsed) ||
        parsed.ip != row.key.ip || parsed.connectionPort != row.key.connectionPort) {
        return ConnectResult::InvalidServer;
    }
    if (!ValidPassword(password)) {
        return ConnectResult::InvalidPassword;
    }
    Retire();
    const char *previous = engine_.Password();
    previous_ = previous ? previous : "";
    std::copy(password.begin(), password.end(), password_.begin());
    pending_ = row.key;
    engine_.SetPassword(password_.data());
    const std::string command = "connect " + ConnectionAddress(row.key) + "\n";
    engine_.Connect(command.c_str());
    return ConnectResult::Started;
}

void ServerBrowserConnection::Accepted(const char *game, uint32_t rawIP, uint16_t connectionPort, uint32_t unixTime) {
    unsigned char bytes[sizeof(rawIP)]{};
    std::memcpy(bytes, &rawIP, sizeof(rawIP));
    constexpr unsigned int OctetBits = 8;
    uint32_t ip = 0;
    for (const unsigned char byte : bytes) {
        ip = (ip << OctetBits) | byte;
    }
    ++acceptanceSerial_;
    current_.reset();
    if (game && game_ == game && ip && connectionPort) {
        Address address{ip, connectionPort, connectionPort};
        if (pending_ && pending_->ip == ip && pending_->connectionPort == connectionPort) {
            address = *pending_;
        }
        current_ = FavoriteRecord{app_, address, k_unFavoriteFlagHistory, unixTime};
        model_.RecordAcceptedConnection(app_, address, unixTime);
    }
    Retire();
}

std::optional<FavoriteRecord> ServerBrowserConnection::CurrentServer() const {
    return current_;
}

void ServerBrowserConnection::Disconnected() {
    current_.reset();
    Retire();
}

void ServerBrowserConnection::Retire() {
    if (pending_) {
        const char *current = engine_.Password();
        if (current && std::strcmp(current, password_.data()) == 0) {
            engine_.SetPassword(previous_.c_str());
        }
    }
    pending_.reset();
    Erase(password_.data(), password_.size());
    if (!previous_.empty()) {
        Erase(previous_.data(), previous_.size());
        previous_.clear();
    }
}
}
