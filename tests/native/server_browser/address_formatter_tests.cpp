#include "steam_api_common.h"
#include "matchmakingtypes.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
constexpr size_t OctetCount = 4;
constexpr unsigned int MaximumOctet = 255;
constexpr uint16 MaximumPort = 65535;
constexpr uint16 ConnectionPort = 27015;
constexpr uint16 QueryPort = 27016;

void Check(bool condition) {
    if (!condition) {
        throw std::runtime_error("SDK address formatter output mismatch");
    }
}

void CheckOctets(const std::array<unsigned char, OctetCount>& octets, uint16 queryPort, uint16 connectionPort) {
    // Supply the memory layout expected by the compiled helper branch. On a little-endian host,
    // VALVE_BIG_ENDIAN exercises that branch with simulated input bytes, not a big-endian runtime.
    auto bytes = octets;
#ifndef VALVE_BIG_ENDIAN
    for (size_t index = 0; index < OctetCount; ++index) {
        bytes[index] = octets[OctetCount - index - 1];
    }
#endif
    uint32 ip = 0;
    static_assert(sizeof(ip) == sizeof(bytes), "The pinned IPv4 helper consumes four octets");
    std::memcpy(&ip, bytes.data(), sizeof(ip));
    servernetadr_t address;
    address.Init(ip, queryPort, connectionPort);
    std::string expected;
    for (unsigned char octet : octets) {
        if (!expected.empty()) {
            expected += '.';
        }
        expected += std::to_string(static_cast<unsigned int>(octet));
    }
    Check(std::string(address.GetConnectionAddressString()) == expected + ':' + std::to_string(connectionPort));
    Check(std::string(address.GetQueryAddressString()) == expected + ':' + std::to_string(queryPort));
    Check(address.GetIP() == ip && address.GetConnectionPort() == connectionPort && address.GetQueryPort() == queryPort);
}
}

int main() {
    try {
        constexpr uint32 HostMarker = 0x01020304U;
        std::array<unsigned char, OctetCount> physical{};
        std::memcpy(physical.data(), &HostMarker, sizeof(HostMarker));
        const bool physicalLittleEndian = physical.front() == 4;
        Check(physicalLittleEndian || physical.front() == 1);
        servernetadr_t numeric;
        numeric.Init(0xff800001U, 0, MaximumPort);
#ifdef VALVE_BIG_ENDIAN
        Check(std::string(numeric.GetConnectionAddressString()) ==
            (physicalLittleEndian ? "1.0.128.255:65535" : "255.128.0.1:65535"));
        constexpr char Branch[] = "big";
#else
        Check(std::string(numeric.GetConnectionAddressString()) ==
            (physicalLittleEndian ? "255.128.0.1:65535" : "1.0.128.255:65535"));
        constexpr char Branch[] = "little";
#endif
        size_t cases = 1;
        CheckOctets({0, 0, 0, 0}, MaximumPort, 0);
        CheckOctets({255, 255, 255, 255}, 0, MaximumPort);
        cases += 2;
        for (size_t position = 0; position < OctetCount; ++position) {
            for (unsigned int value = 0; value <= MaximumOctet; ++value) {
                std::array<unsigned char, OctetCount> octets{17, 34, 51, 68};
                octets[position] = static_cast<unsigned char>(value);
                CheckOctets(octets, QueryPort, ConnectionPort);
                ++cases;
            }
        }
        std::cout << cases << " actual SDK formatter cases passed; physical="
                  << (physicalLittleEndian ? "little" : "big") << "; compiled branch=" << Branch
                  << "; opposite branch uses explicitly simulated byte layout\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
