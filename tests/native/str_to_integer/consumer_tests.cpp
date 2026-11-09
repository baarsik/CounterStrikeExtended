#include "legacy_str_to_integer.h"
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s errno=%d\n", message, errno);
        std::abort();
    }
}
}

struct OwnedMemory {
    std::vector<unsigned char> bytes;
    unsigned char* m_pMemory;
    int m_NumAllocated;

    explicit OwnedMemory(const std::string& text) : bytes(text.begin(), text.end()), m_pMemory(nullptr),
        m_NumAllocated(static_cast<int>(text.size() + 1)) {
        bytes.push_back(0);
        m_pMemory = bytes.data();
    }

    int NumAllocated() const {
        return m_NumAllocated;
    }

    unsigned char* Base() {
        return m_pMemory;
    }

    unsigned char& operator[](int index) {
        return bytes[static_cast<std::size_t>(index)];
    }
};

class FixtureBuffer {
public:
    enum { TEXT_BUFFER = 1, GET_OVERFLOW = 2 };
    OwnedMemory m_Memory;
    int m_Get = 0;
    unsigned char m_Error = 0;
    unsigned char m_Flags = TEXT_BUFFER;

    explicit FixtureBuffer(const std::string& text) : m_Memory(text) {}
    virtual ~FixtureBuffer() = default;
    bool IsText() const {
        return (m_Flags & TEXT_BUFFER) != 0;
    }

    bool IsValid() const {
        return m_Error == 0;
    }

    int Size() const {
        return m_Memory.NumAllocated();
    }

    void GetString(char*) {
        Require(false, "unselected string path must not execute");
    }

    void EatWhiteSpace() {
        Require(false, "unselected explicit whitespace path must not execute");
    }

    virtual int VaScanf(const char*, va_list) = 0;
    int Scan(const char* format, ...) {
        va_list list;
        va_start(list, format);
        const int count = VaScanf(format, list);
        va_end(list);
        return count;
    }
};

class EngineBuffer : public FixtureBuffer {
public:
    using FixtureBuffer::FixtureBuffer;
    int VaScanf(const char*, va_list) override;
};
class VguiBuffer : public FixtureBuffer {
public:
    using FixtureBuffer::FixtureBuffer;
    int VaScanf(const char*, va_list) override;
};
class CstrikeBuffer : public FixtureBuffer {
public:
    using FixtureBuffer::FixtureBuffer;
    int VaScanf(const char*, va_list) override;
};

std::int32_t CseFixtureStrtol(const char* text, char** end, int base) {
    Require(base == 10, "actual retained signed consumer is decimal only");
    return CseLegacyStrtol10(text, end);
}
std::uint32_t CseFixtureStrtoul(const char* text, char** end, int base) {
    Require(base == 10, "actual retained unsigned consumer is decimal only");
    return CseLegacyStrtoul10(text, end);
}
#define strtol CseFixtureStrtol
#define strtoul CseFixtureStrtoul
#define ASSERT_AT(expression, line) Require(static_cast<bool>(expression), "actual Windows scanner assertion")
#include "actual_scanf.h"
#undef ASSERT_AT
#undef strtoul
#undef strtol

namespace {
template<typename Buffer>
void CheckConsumer() {
    constexpr int kSentinelError = 12345;
    for (const char* text : {"", "+", " \t-", "nondigit"}) {
        Buffer buffer(text);
        int value = 303;
        errno = kSentinelError;
        Require(buffer.Scan("%d", &value) == 0 && value == 0 && buffer.m_Get == 0,
            "no conversion still assigns zero before returning unchanged count and cursor");
        Require(errno == kSentinelError, "no conversion preserves incoming errno");
    }
    {
        Buffer buffer("42,+");
        int first = 303;
        unsigned int second = 707;
        errno = kSentinelError;
        Require(buffer.Scan("%d,%u", &first, &second) == 1 && first == 42 && second == 0 && buffer.m_Get == 3,
            "failed later conversion preserves prior count and current input position but assigns zero");
        Require(errno == kSentinelError, "success and later no conversion preserve errno");
    }
    {
        Buffer buffer("2147483648X");
        int value = 0;
        errno = 0;
        Require(buffer.Scan("%i", &value) == 1 && value == (std::numeric_limits<int>::max)()
            && buffer.m_Get == 10 && errno == ERANGE, "signed overflow remains a completed decimal conversion");
    }
    {
        Buffer buffer("-4294967296X");
        unsigned int value = 0;
        errno = kSentinelError;
        Require(buffer.Scan("%u", &value) == 1 && value == (std::numeric_limits<unsigned int>::max)()
            && buffer.m_Get == 11 && errno == ERANGE, "unsigned negative overflow consumes the digit run");
    }
    {
        Buffer buffer("0x17");
        int value = 303;
        errno = kSentinelError;
        Require(buffer.Scan("%i", &value) == 1 && value == 0 && buffer.m_Get == 1 && errno == kSentinelError,
            "percent-i is explicitly base10 in these retained consumers");
    }
    for (bool error : {false, true}) {
        Buffer buffer("17");
        if (error) {
            buffer.m_Error = Buffer::GET_OVERFLOW;
        } else {
            buffer.m_Flags = 0;
        }
        int value = 303;
        Require(buffer.Scan("%d", &value) == 0 && value == 303 && buffer.m_Get == 0,
            "invalid or binary buffer returns before conversion and assignment");
    }
}
}

int main(int argc, char** argv) {
    Require(argc == 2, "one actual consumer family required");
    const std::string family(argv[1]);
    if (family == "engine") {
        CheckConsumer<EngineBuffer>();
    } else if (family == "vgui") {
        CheckConsumer<VguiBuffer>();
    } else {
        Require(family == "cstrike", "known actual consumer family");
        CheckConsumer<CstrikeBuffer>();
    }
    std::printf("PASS: actual %s VaScanf decimal method body\n", family.c_str());
    return 0;
}
