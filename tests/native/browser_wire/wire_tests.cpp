#include "browser_wire.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

using namespace cse::browser;

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "browser_wire_failure=%s\n", message);
        std::exit(1);
    }
}

int main() {
    constexpr std::size_t FixturePayloadLimit = 4 * 1024 * 1024;
    constexpr std::uint64_t WideValue = 0xfedcba9876543210ull;
    Writer writer;
    Require(writer.U32(0x12345678) && writer.I32(std::numeric_limits<std::int32_t>::min()) &&
        writer.U64(WideValue) && writer.Boolean(true) && writer.String("A\xd0\xaf\xf0\x9f\x98\x80"), "encode fields");
    const std::vector<std::uint8_t> expectedPayload = {0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0x80,
        0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe, 1, 0, 0, 0, 7, 0, 0, 0,
        0x41, 0xd0, 0xaf, 0xf0, 0x9f, 0x98, 0x80};
    Require(writer.Data() == expectedPayload, "explicit little-endian field bytes");
    Header header{Kind::Command, 0x1234, 7, 9, static_cast<std::uint32_t>(writer.Data().size())};
    const auto bytes = EncodeHeader(header);
    const std::array<std::uint8_t, HeaderBytes> expectedHeader = {0x43, 0x53, 0x45, 0x42, 1, 0, 1, 0,
        0x34, 0x12, 0, 0, 7, 0, 0, 0, 9, 0, 0, 0, 31, 0, 0, 0};
    Require(bytes == expectedHeader, "fixed header bytes independent of native layout");
    std::vector<std::uint8_t> packet(bytes.begin(), bytes.end());
    packet.insert(packet.end(), writer.Data().begin(), writer.Data().end());
    for (std::size_t stride = 1; stride <= packet.size(); ++stride) {
        Decoder decoder(FixturePayloadLimit);
        Message message;
        std::size_t consumed = 0;
        while (consumed < packet.size()) {
            const std::size_t remaining = packet.size() - consumed;
            const std::size_t amount = remaining < stride ? remaining : stride;
            std::size_t used = 0;
            const DecodeStatus status = decoder.Feed(packet.data() + consumed, amount, used, message);
            Require(used == amount, "fragment consumption");
            consumed += used;
            Require(status == (consumed == packet.size() ? DecodeStatus::Complete : DecodeStatus::NeedMore),
                "fragment completion");
        }
        Require(decoder.Finish(), "complete frame boundary");
        Require(message.header.operation == header.operation && message.header.browser == header.browser &&
            message.header.request == header.request && message.payload == expectedPayload, "decoded frame identity");
        Reader reader(message.payload);
        std::uint32_t unsignedValue = 0;
        std::int32_t signedValue = 0;
        std::uint64_t wideValue = 0;
        bool boolean = false;
        std::string text;
        Require(reader.U32(unsignedValue) && reader.I32(signedValue) && reader.U64(wideValue) &&
            reader.Boolean(boolean) && reader.String(text) && reader.Finish(), "decode fields");
        Require(unsignedValue == 0x12345678 && signedValue == std::numeric_limits<std::int32_t>::min() &&
            wideValue == WideValue && boolean && text == "A\xd0\xaf\xf0\x9f\x98\x80", "field values");
    }
    for (std::size_t missing = 1; missing < packet.size(); ++missing) {
        Decoder decoder(FixturePayloadLimit);
        Message message;
        std::size_t used = 0;
        Require(decoder.Feed(packet.data(), packet.size() - missing, used, message) == DecodeStatus::NeedMore &&
            !decoder.Finish(), "truncated stream rejected");
    }
    for (const std::size_t offset : {0u, 4u, 6u}) {
        auto invalid = packet;
        invalid[offset] = 0xff;
        Decoder decoder(FixturePayloadLimit);
        Message message;
        std::size_t used = 0;
        Require(decoder.Feed(invalid.data(), invalid.size(), used, message) == DecodeStatus::Invalid,
            "magic/version/kind mismatch rejected");
    }
    {
        auto invalid = packet;
        invalid[20] = 0xff;
        invalid[21] = 0xff;
        invalid[22] = 0xff;
        invalid[23] = 0xff;
        Decoder decoder(FixturePayloadLimit);
        Message message;
        std::size_t used = 0;
        Require(decoder.Feed(invalid.data(), invalid.size(), used, message) == DecodeStatus::Invalid,
            "declared payload rejected before allocation");
    }
    {
        const std::vector<std::uint8_t> invalidBoolean{2, 0, 0, 0};
        Reader reader(invalidBoolean);
        bool value = false;
        Require(!reader.Boolean(value) && !reader.Finish(), "invalid boolean rejected");
        const std::vector<std::uint8_t> invalidString{0xff, 0xff, 0xff, 0xff};
        Reader stringReader(invalidString);
        std::string text;
        Require(!stringReader.String(text) && !stringReader.Finish(), "string length checked before access");
    }
    {
        Decoder decoder(FixturePayloadLimit);
        Message message;
        std::size_t used = 0;
        std::vector<std::uint8_t> joined = packet;
        joined.insert(joined.end(), packet.begin(), packet.end());
        Require(decoder.Feed(joined.data(), joined.size(), used, message) == DecodeStatus::Complete &&
            used == packet.size(), "coalesced messages keep exact boundary");
        std::size_t secondUsed = 0;
        Require(decoder.Feed(joined.data() + used, joined.size() - used, secondUsed, message) == DecodeStatus::Complete &&
            secondUsed == packet.size(), "decoder can consume following message");
        Header empty{Kind::Event, 1, 0, 0, 0};
        const auto encoded = EncodeHeader(empty);
        Require(decoder.Feed(encoded.data(), encoded.size(), used, message) == DecodeStatus::Complete &&
            message.payload.empty() && decoder.Finish(), "empty message completes without an extra byte");
    }
    std::puts("native_browser_wire_passed=1");
}
