#include "parsemsg.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <limits>

static bool overrideLength = false;
static std::size_t syntheticLength = 0;

std::size_t CseFixtureLength(const char* value) {
    return overrideLength ? syntheticLength : std::strlen(value);
}

namespace {
// The retained header declares HasOverflowed without providing a definition.
// Observe the real protected state without adding an unowned production API.
class ObservedWriter : public BufferWriter {
public:
    using BufferWriter::BufferWriter;
    bool Overflowed() const {
        return m_overflow;
    }
};
constexpr std::size_t GuardBytes = 8;
constexpr std::size_t MaximumCapacity = 64;
constexpr unsigned char Untouched = 0x5a;
std::uint64_t digest = 14695981039346656037ULL;

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

void Hash(unsigned char value) {
    digest ^= value;
    digest *= 1099511628211ULL;
}

void Strings() {
    for (std::size_t capacity = 0; capacity <= MaximumCapacity; ++capacity) {
        for (std::size_t length = 0; length <= MaximumCapacity + GuardBytes; ++length) {
            std::array<unsigned char, MaximumCapacity + GuardBytes * 2> output;
            output.fill(Untouched);
            std::string text(length, static_cast<char>(0xe9));
            ObservedWriter writer(output.data() + GuardBytes, static_cast<int>(capacity));
            writer.WriteString(text.c_str());
            const bool fits = length + 1 <= capacity;
            const std::size_t used = capacity == 0 ? 0 : fits ? length + 1 : 1;
            Check(writer.GetSpaceUsed() == static_cast<int>(used), "string transfer count");
            Check(writer.Overflowed() == !fits, "overflow status");
            for (std::size_t index = 0; index < output.size(); ++index) {
                unsigned char expected = Untouched;
                if (index >= GuardBytes && index < GuardBytes + used) {
                    expected = fits && index < GuardBytes + length ? 0xe9 : 0;
                }
                Check(output[index] == expected, "exact message byte or untouched guard changed");
                Hash(output[index]);
            }
            Hash(static_cast<unsigned char>(used));
            Hash(writer.Overflowed() ? 1 : 0);
        }
    }
}

void MixedMessages() {
    for (int capacity = 0; capacity <= static_cast<int>(MaximumCapacity); ++capacity) {
        std::array<unsigned char, MaximumCapacity + GuardBytes> output;
        output.fill(Untouched);
        ObservedWriter writer(output.data(), capacity);
        writer.WriteByte(0xa5);
        writer.WriteString("HUD");
        writer.WriteLong(-1985229329); // Retained signed32 bit pattern 0x89abcdef.
        if (capacity >= 9) {
            const unsigned char expected[] = {0xa5, 'H', 'U', 'D', 0, 0xef, 0xcd, 0xab, 0x89};
            Check(std::memcmp(output.data(), expected, sizeof(expected)) == 0, "mixed field wire bytes");
            Check(writer.GetSpaceUsed() == 9 && !writer.Overflowed(), "mixed field transfer state");
        }
        for (std::size_t index = static_cast<std::size_t>(capacity); index < output.size(); ++index) {
            Check(output[index] == Untouched, "mixed message wrote beyond advertised capacity");
        }
        for (unsigned char byte : output) {
            Hash(byte);
        }
        Hash(static_cast<unsigned char>(writer.GetSpaceUsed()));
        Hash(writer.Overflowed() ? 1 : 0);
    }
    ObservedWriter missing;
    missing.WriteString("ignored");
    Check(missing.Overflowed() && missing.GetSpaceUsed() == 0, "absent output storage");
    unsigned char output[2] = {Untouched, Untouched};
    ObservedWriter nullString(output, sizeof(output));
    nullString.WriteString(nullptr);
    Check(output[0] == 0 && output[1] == Untouched && nullString.GetSpaceUsed() == 1
        && !nullString.Overflowed(), "null input remains an empty wire string");
}

void NativeLengths() {
    if (sizeof(std::size_t) <= sizeof(std::uint32_t)) {
        return;
    }
    const std::uint64_t largeLengths[] = {
        0x7fffffffULL, 0x80000000ULL, 0xffffffffULL, 0x100000000ULL,
        0x100000004ULL, std::numeric_limits<std::uint64_t>::max()
    };
    overrideLength = true;
    for (std::uint64_t length : largeLengths) {
        syntheticLength = static_cast<std::size_t>(length);
        unsigned char output[16];
        std::memset(output, Untouched, sizeof(output));
        ObservedWriter writer(output, sizeof(output));
        writer.WriteString("length-probe");
        Check(writer.Overflowed() && writer.GetSpaceUsed() == 1 && output[0] == 0,
            "native string length must be checked before narrowing or adding the terminator");
        for (std::size_t index = 1; index < sizeof(output); ++index) {
            Check(output[index] == Untouched, "oversize length copied payload rather than an empty string");
        }
    }
    overrideLength = false;
}
}

int main() {
    Strings();
    MixedMessages();
    NativeLengths();
    std::printf("message writer exact bytes %016llx\n", static_cast<unsigned long long>(digest));
}
