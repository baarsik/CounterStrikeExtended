#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <type_traits>

// Access changes only for observations; all declarations, types, data fields and virtual methods remain actual SDK ones.
#define protected public
#include "HLTV/Core/World.h"
#undef protected

namespace {

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", description);
        std::exit(EXIT_FAILURE);
    }
}

std::map<void*, unsigned int> allocations;
int allocationCalls = 0;
int failedAllocation = 0;
int freeCalls = 0;

bool HasVirtualTable(const void* object) {
    std::uintptr_t word = 0;
    std::memcpy(&word, object, sizeof(word));
    return word != 0;
}

void EmptyBuffer(const BitBuffer& buffer) {
    Require(HasVirtualTable(&buffer), "actual BitBuffer has a valid constructed virtual table word");
    Require(!buffer.sizeError && buffer.data == nullptr && buffer.currentByte == nullptr
        && buffer.currentBit == 0 && buffer.maxSize == 0 && !buffer.littleEndian && !buffer.ownData,
        "reset retains all original empty semantic field goldens");
}

class ControlledSystem {
public:
    void Errorf(const char*, ...) {
        ++errors;
    }
    int errors = 0;
};

}

extern "C" void* Mem_ZeroMalloc(unsigned int size) {
    ++allocationCalls;
    if (failedAllocation < 0 || allocationCalls == failedAllocation) {
        return nullptr;
    }
    void* pointer = std::calloc(1, size);
    if (pointer != nullptr) {
        Require(allocations.emplace(pointer, size).second, "allocation is uniquely owned");
    }
    return pointer;
}

extern "C" void CseObjectResetFree(void* pointer) {
    if (pointer != nullptr) {
        Require(allocations.erase(pointer) == 1, "free only live owned allocations, never external storage");
        ++freeCalls;
        std::free(pointer);
    }
}

#include "generated_regions.h"

static_assert(std::is_nothrow_destructible<deltaCache_t>::value, "cache destruction cannot propagate exceptions");

void DestroyAddressVirtually(NetAddress* address);
void DestroyBufferVirtually(BitBuffer* buffer);

namespace {

void Constructor() {
    NetAddress address;
    std::printf("NetAddress sizeof=%zu align=%zu virtual_word=%d\n", sizeof(address), alignof(NetAddress),
        HasVirtualTable(&address));
    const std::array<unsigned char, 4> empty = {};
    Require(std::memcmp(address.m_IP, empty.data(), empty.size()) == 0 && address.m_Port == 0,
        "constructor retains zero address payload");
    for (char character : address.m_String) {
        Require(character == 0, "constructor retains empty cached address string");
    }
    Require(HasVirtualTable(&address), "actual NetAddress constructor preserves its virtual table");
    DestroyAddressVirtually(&address);
    new (&address) NetAddress();
}

void Address() {
    NetAddress address;
    Require(HasVirtualTable(&address), "address starts with constructed dispatch");
    address.m_IP[0] = 192;
    address.m_IP[1] = 0;
    address.m_IP[2] = 2;
    address.m_IP[3] = 7;
    address.m_Port = 0x1234;
    std::memset(address.m_String, 'X', sizeof(address.m_String));
    ResetAddress(address);
    Require(HasVirtualTable(&address), "Network reset preserves actual NetAddress virtual dispatch");
    const std::array<unsigned char, 4> empty = {};
    Require(std::memcmp(address.m_IP, empty.data(), empty.size()) == 0 && address.m_Port == 0,
        "Network reset clears only address payload");
    for (char character : address.m_String) {
        Require(character == 0, "Network reset clears cached address bytes");
    }
    BitBuffer stream(16);
    address.ToStream(&stream);
    constexpr std::array<unsigned char, 6> EncodedEmpty = {{0, 0, 0, 0, 0, 0}};
    Require(stream.CurrentSize() == static_cast<int>(EncodedEmpty.size())
        && std::memcmp(stream.data, EncodedEmpty.data(), EncodedEmpty.size()) == 0,
        "empty address keeps the independent six-byte wire golden");
    DestroyAddressVirtually(&address);
    new (&address) NetAddress();
}

void Allocation(int failed) {
    constexpr int CacheEntries = 3;
    const unsigned int frameBytes = static_cast<unsigned int>(sizeof(frameCache_t) * CacheEntries);
    frameCache_t* frameCache = nullptr;
    deltaCache_t* deltaCache = nullptr;
    ControlledSystem system;
    failedAllocation = failed;
    const bool result = AllocateCaches(CacheEntries, frameBytes, frameCache, deltaCache, &system);
    Require(result == (failed == 0) && system.errors == (failed == 0 ? 0 : 1),
        "actual allocation region retains success/failure reporting");
    if (deltaCache != nullptr) {
        for (int index = 0; index < CacheEntries; ++index) {
            Require(deltaCache[index].seqNr == 0 && deltaCache[index].deltaNr == 0,
                "actual allocated cache entry retains zero scalar IDs");
            EmptyBuffer(deltaCache[index].buffer);
            DestroyBufferVirtually(&deltaCache[index].buffer);
            new (&deltaCache[index].buffer) BitBuffer();
        }
    }
    int hits = 7;
    int faults = 9;
    ClearCaches(CacheEntries, frameCache, deltaCache, hits, faults);
    Require(hits == 1 && faults == 1, "cache statistics retain original reset goldens");
    ReleaseCaches(CacheEntries, frameCache, deltaCache);
    Require(frameCache == nullptr && deltaCache == nullptr && allocations.empty(),
        "partial initialization and normal release leave no live owned storage");
}

void Lifecycle() {
    constexpr int CacheEntries = 3;
    constexpr int ResetCycles = 64;
    constexpr unsigned int BufferBytes = 32;
    // Retained WriteBits ORs into supplied storage: initial 0x5a with written 0x3c becomes 0x7e.
    constexpr unsigned char ExpectedWrittenExternalByte = 0x7e;
    const unsigned int frameBytes = static_cast<unsigned int>(sizeof(frameCache_t) * CacheEntries);
    auto* frameCache = static_cast<frameCache_t*>(Mem_ZeroMalloc(frameBytes));
    auto* deltaCache = static_cast<deltaCache_t*>(Mem_ZeroMalloc(sizeof(deltaCache_t) * CacheEntries));
    Require(frameCache != nullptr && deltaCache != nullptr, "owned fixture cache storage");
    for (int index = 0; index < CacheEntries; ++index) {
        new (&deltaCache[index]) deltaCache_t();
    }
    std::array<unsigned char, BufferBytes> external;
    external.fill(0x5a);
    int hits = 7;
    int faults = 9;
    for (int cycle = 0; cycle < ResetCycles; ++cycle) {
        for (int index = 0; index < CacheEntries; ++index) {
            deltaCache[index].seqNr = 100 + static_cast<unsigned int>(cycle);
            deltaCache[index].deltaNr = 3;
            if (index == 1) {
                deltaCache[index].buffer.SetBuffer(external.data(), static_cast<int>(external.size()));
            } else {
                Require(deltaCache[index].buffer.Resize(BufferBytes), "allocate actual owned BitBuffer storage");
            }
            deltaCache[index].buffer.WriteByte(0x3c);
            deltaCache[index].buffer.sizeError = true;
        }
        std::memset(frameCache, 0xa5, frameBytes);
        const std::size_t live = allocations.size();
        const int released = freeCalls;
        ClearCaches(CacheEntries, frameCache, deltaCache, hits, faults);
        Require(allocations.size() + 2 == live && freeCalls == released + 2,
            "clear destroys each owned buffer exactly once and retains external storage");
        for (int index = 0; index < CacheEntries; ++index) {
            Require(deltaCache[index].seqNr == 0 && deltaCache[index].deltaNr == 0,
                "repeated clear restores scalar cache IDs");
            EmptyBuffer(deltaCache[index].buffer);
            DestroyBufferVirtually(&deltaCache[index].buffer);
            new (&deltaCache[index].buffer) BitBuffer();
        }
        const auto* bytes = reinterpret_cast<const unsigned char*>(frameCache);
        for (unsigned int index = 0; index < frameBytes; ++index) {
            Require(bytes[index] == 0, "POD frame-cache byte clearing stays unchanged");
        }
        Require(external[0] == ExpectedWrittenExternalByte && external[1] == 0x5a && hits == 1 && faults == 1,
            "external bytes and original statistics are preserved");
    }
    // Release also owns destruction when no earlier Clear removed data.
    Require(deltaCache[0].buffer.Resize(BufferBytes), "live buffer at release");
    ReleaseCaches(CacheEntries, frameCache, deltaCache);
    Require(frameCache == nullptr && deltaCache == nullptr && allocations.empty(),
        "release destroys live entries and frees the existing allocator storage");
    ReleaseCaches(CacheEntries, frameCache, deltaCache);
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "select controlled component case");
    const std::string mode = argv[1];
    if (mode == "constructor") {
        Constructor();
    } else if (mode == "address") {
        Address();
    } else if (mode == "allocation") {
        Allocation(0);
    } else if (mode == "frame-failure") {
        Allocation(1);
    } else if (mode == "delta-failure") {
        Allocation(2);
    } else if (mode == "both-failure") {
        Allocation(-1);
    } else if (mode == "lifecycle") {
        Lifecycle();
    } else {
        Require(false, "known controlled case");
    }
    Require(allocations.empty(), "all actual component owned allocations released");
    std::printf("PASS %s alloc=%d free=%d cache_size=%zu\n", argv[1], allocationCalls, freeCalls, sizeof(deltaCache_t));
    return EXIT_SUCCESS;
}
