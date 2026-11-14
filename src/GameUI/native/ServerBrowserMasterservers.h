#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cse::server_browser {
inline constexpr size_t MaximumCustomMasterservers = 32;
inline constexpr size_t MaximumMasterEndpoints = 8;
inline constexpr size_t MaximumMasterNameBytes = 64;
inline constexpr size_t MaximumMasterAddressBytes = 2048;
struct MasterEndpoint {
    std::string host;
    uint16_t port = 0;
};
bool ParseMasterEndpoint(std::string_view text, MasterEndpoint& endpoint);
struct Masterserver {
    std::string id;
    std::string name;
    std::vector<std::string> endpoints;
    bool predefined = false;
};
struct MasterserverSettings {
    std::string selected = "varq";
    std::vector<Masterserver> custom;
    std::vector<Masterserver> Providers() const;
    Masterserver Selected() const;
    bool Add(std::string_view name, std::string_view addresses);
    bool Remove(std::string_view id);
    bool Valid() const;
};
}
