#include "NativeClipboardOwner.h"
#include "NativeClipboardMac.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace owner = cse_vgui2_clipboard_owner;
namespace core = cse_vgui2_clipboard;

namespace {
std::uint64_t NextEpoch = 0;
// Only external host/allocator/codec/lease services are controlled. All owner
// packing, validation, snapshots, publication and retirement are production code.
struct Host {
    bool thread = true;
    bool retain = true;
    bool release = true;
    bool retire = true;
    bool allocation = true;
    bool throwCodec = false;
    bool malformedPublic = false;
    int failure = 0;
    int pins = 0;
    int deferred = 0;
    int opens = 0;
    int mutations = 0;
    int reads = 0;
    int readReleases = 0;
    int closes = 0;
    int outstanding = 0;
    std::int64_t change = 7;
    std::int64_t readAfter = 7;
    std::vector<unsigned char> bytes;
    std::vector<unsigned char> publicBytes;

    static bool Thread(void* context) { return static_cast<Host*>(context)->thread; }
    static bool Pin(void* context, std::uint64_t* token) {
        auto& host = *static_cast<Host*>(context);
        if (!host.retain) {
            return false;
        }
        ++host.pins;
        *token = 42;
        return true;
    }
    static bool Deferred(void* context, std::uint64_t token) {
        auto& host = *static_cast<Host*>(context);
        assert(token == 42);
        if (!host.release) {
            return false;
        }
        ++host.deferred;
        return true;
    }
    static void* Allocate(void* context, std::size_t bytes) {
        assert(bytes != 0);
        auto& host = *static_cast<Host*>(context);
        if (!host.allocation) {
            return nullptr;
        }
        void* data = std::malloc(bytes);
        if (data) {
            ++host.outstanding;
        }
        return data;
    }
    static void Free(void* context, void* data) {
        assert(data != nullptr);
        --static_cast<Host*>(context)->outstanding;
        std::free(data);
    }
    static owner::Status Encode(void* context, core::ByteSpan units, core::ByteBuffer output,
        std::uint64_t* written) {
        auto& host = *static_cast<Host*>(context);
        if (host.throwCodec) {
            throw std::runtime_error("controlled codec failure");
        }
        if (host.malformedPublic) {
            return owner::Status::Unrepresentable;
        }
        // This injected authored-data codec only admits ASCII UTF16LE. It is
        // deliberately not an external-text converter or product charset.
        assert(units.storageBytes % 2 == 0);
        const std::uint64_t required = units.storageBytes / 2;
        for (std::uint64_t index = 0; index < required; ++index) {
            if (units.data[index * 2 + 1] != 0 || units.data[index * 2] > 0x7f) {
                return owner::Status::Unrepresentable;
            }
        }
        if (!output.data) {
            *written = required;
            return owner::Status::Ok;
        }
        assert(output.storageBytes == required);
        for (std::uint64_t index = 0; index < required; ++index) {
            output.data[index] = units.data[index * 2];
        }
        *written = required;
        return owner::Status::Ok;
    }
    static owner::Status Open(void* context) {
        ++static_cast<Host*>(context)->opens;
        return owner::Status::Ok;
    }
    static owner::Status Read(void* context, owner::HostRead* output) {
        auto& host = *static_cast<Host*>(context);
        ++host.reads;
        auto* copy = new std::vector<unsigned char>(host.bytes);
        *output = {{copy->data(), copy->size()}, copy, host.change, host.readAfter};
        return owner::Status::Ok;
    }
    static bool ReleaseRead(void* context, void* token) {
        auto& host = *static_cast<Host*>(context);
        ++host.readReleases;
        if (!host.retire) {
            return false;
        }
        delete static_cast<std::vector<unsigned char>*>(token);
        return true;
    }
    static owner::Status Prepare(void* context, std::int64_t* change) {
        auto& host = *static_cast<Host*>(context);
        ++host.mutations;
        host.bytes.clear();
        host.publicBytes.clear();
        *change = ++host.change;
        host.readAfter = host.change;
        return host.failure == 1 ? owner::Status::StateUncertain : owner::Status::Ok;
    }
    static owner::Status Stage(void* context, core::ByteSpan, core::ByteSpan, bool) {
        return static_cast<Host*>(context)->failure == 6 ? owner::Status::NoMemory : owner::Status::Ok;
    }
    static owner::Status Types(void* context, bool, std::int64_t*) {
        return static_cast<Host*>(context)->failure == 2 ? owner::Status::HostFailure : owner::Status::Ok;
    }
    static owner::Status SetPrivate(void* context, core::ByteSpan bytes, std::int64_t) {
        auto& host = *static_cast<Host*>(context);
        if (host.failure == 3) {
            return owner::Status::HostFailure;
        }
        host.bytes.assign(bytes.data, bytes.data + bytes.storageBytes);
        return owner::Status::Ok;
    }
    static owner::Status SetPublic(void* context, core::ByteSpan bytes, std::int64_t) {
        auto& host = *static_cast<Host*>(context);
        if (host.failure == 4) {
            return owner::Status::HostFailure;
        }
        if (host.failure == 5) {
            return owner::Status::OwnershipChanged;
        }
        host.publicBytes.assign(bytes.data, bytes.data + bytes.storageBytes);
        return owner::Status::Ok;
    }
    static owner::Status Close(void* context) {
        auto& host = *static_cast<Host*>(context);
        ++host.closes;
        return host.failure == 7 ? owner::Status::StateUncertain : owner::Status::Ok;
    }

    owner::DependenciesV1 Dependencies() {
        return {sizeof(owner::DependenciesV1), 1, this, ++NextEpoch, Thread, Pin, Deferred,
            Allocate, Free, Encode, nullptr};
    }
    owner::BackendV1 Backend() {
        return {sizeof(owner::BackendV1), 1, this, Open, Read, ReleaseRead, Stage, Prepare, Types, SetPrivate,
            SetPublic, Close};
    }
};

constexpr unsigned char Raw8[] = {'A', 0, 'B', 0};
constexpr unsigned char Raw16[] = {'A', 0, 0, 0, 'B', 0, 0, 0, 0xa5};
owner::PreparedViews Views() {
    // Raw8 four bytes, raw16 nine-byte allocation/eight-byte cell extent.
    // Public explicit six-byte unit prefix includes the embedded NUL and tail B.
    return {true, {Raw8, sizeof(Raw8)}, 101, true, {Raw16, sizeof(Raw16)}, 8, true, 6};
}

void RoundTrip() {
    Host host;
    owner::Owner clipboard;
    assert(clipboard.Open(host.Dependencies(), host.Backend()) == owner::Status::Ok);
    assert(clipboard.Publish(Views()) == owner::Status::Ok);
    assert((host.publicBytes == std::vector<unsigned char>{'A', 0, 'B'}));
    // Independent fixed-format oracle: 64-byte LE header, then 4 raw8 and 9
    // raw16 allocation bytes. No production encoder computes this expected data.
    constexpr unsigned char Expected[] = {
        'C', 'S', 'E', 'C', 'L', 'P', '1', 0, 1, 0, 0, 0, 3, 0, 0, 0,
        64, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0,
        9, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0,
        101, 0, 0, 0, 0, 0, 0, 0, 77, 0, 0, 0, 0, 0, 0, 0,
        'A', 0, 'B', 0, 'A', 0, 0, 0, 'B', 0, 0, 0, 0xa5
    };
    assert(host.bytes.size() == sizeof(Expected) && std::memcmp(host.bytes.data(), Expected, sizeof(Expected)) == 0);
    owner::Snapshot snapshot{};
    assert(clipboard.Acquire(snapshot) == owner::Status::Ok);
    std::int32_t count = -1;
    assert(core::CountLegacyText(snapshot.unicode, count) == core::Status::Ok && count == 4);
    assert(snapshot.raw8.storageBytes == 4 && std::memcmp(snapshot.raw8.data, Raw8, 4) == 0);
    if (sizeof(void*) > 4) {
        assert(reinterpret_cast<std::uintptr_t>(snapshot.raw8.data) > UINT32_MAX);
    }
    wchar_t cells[4]{};
    assert(core::ReadUnicodeCells(snapshot.unicode, 0, {cells, sizeof(cells)}, count) == core::Status::Ok);
    assert(count == 4 && cells[0] == 'A' && cells[1] == 0 && cells[2] == 'B' && cells[3] == 0);
    unsigned char bytes[9]{};
    assert(core::ReadUnicodeBytes(snapshot.unicode, 0, {bytes, sizeof(bytes)}, count) == core::Status::Ok);
    assert(count == 9 && std::memcmp(bytes, Raw16, 9) == 0);
    host.bytes.assign(1, 0); // Same owner/count: retained ticket cannot change.
    assert(std::memcmp(snapshot.raw8.data, Raw8, 4) == 0);
    assert(clipboard.Close() == owner::Status::Busy);
    auto duplicate = snapshot;
    assert(clipboard.EndBorrow(snapshot) == owner::Status::Ok);
    assert(clipboard.EndBorrow(duplicate) == owner::Status::InvalidArgument);
    assert(snapshot.ticket == 0 && snapshot.raw8.data == nullptr);
    assert(clipboard.Close() == owner::Status::Ok);
    assert(host.deferred == 1 && host.outstanding == 0);
}

void IndependentViewsAndMalformedEnvelope() {
    Host host;
    owner::Owner clipboard;
    assert(clipboard.Open(host.Dependencies(), host.Backend()) == owner::Status::Ok);
    auto views = Views();
    views.publishPublic = false;
    views.unicodeUnitExtentBytes = 7; // malformed wide extent, valid independent raw views
    assert(clipboard.Publish(views) == owner::Status::PrivateOnly);
    owner::Snapshot snapshot{};
    assert(clipboard.Acquire(snapshot) == owner::Status::Ok);
    assert(snapshot.raw8Status == core::Status::Ok);
    assert(snapshot.unicodeBytesStatus == core::Status::Ok);
    assert(snapshot.unicodeCellsStatus == core::Status::MalformedUnicode);
    assert(clipboard.EndBorrow(snapshot) == owner::Status::Ok);
    const auto valid = host.bytes;
    constexpr std::size_t InvalidFields[] = {0, 8, 12, 16, 24, 32, 52, 56};
    for (const auto field : InvalidFields) {
        host.bytes = valid;
        host.bytes[field] ^= 0x80;
        snapshot.ticket = 99;
        assert(clipboard.Acquire(snapshot) == owner::Status::MalformedBundle && snapshot.ticket == 99);
    }
    host.bytes = valid;
    ++host.readAfter;
    snapshot.ticket = 99;
    host.retire = false;
    assert(clipboard.Acquire(snapshot) == owner::Status::ReleaseFailed && snapshot.ticket == 99);
    assert(clipboard.Close() == owner::Status::ReleaseFailed);
    host.retire = true;
    assert(clipboard.RetryRetirements() == owner::Status::Ok);
    assert(clipboard.Close() == owner::Status::Ok && host.outstanding == 0);
}

void CloseFailureAndCodecAdmission() {
    Host host;
    owner::Owner clipboard;
    auto dependencies = host.Dependencies();
    dependencies.encodePublic = nullptr;
    assert(clipboard.Open(dependencies, host.Backend()) == owner::Status::Ok);
    assert(clipboard.Publish(Views()) == owner::Status::CodecUnavailable && host.mutations == 0);
    host.failure = 7;
    assert(clipboard.Close() == owner::Status::StateUncertain && clipboard.HoldsRoot() && host.deferred == 0);
    assert(clipboard.Publish(Views()) == owner::Status::Closed);
    host.failure = 0;
    assert(clipboard.Close() == owner::Status::Ok && host.closes == 2 && host.deferred == 1);
}

void EmptyPrivateBundle() {
    Host host;
    owner::Owner clipboard;
    assert(clipboard.Open(host.Dependencies(), host.Backend()) == owner::Status::Ok);
    owner::Snapshot snapshot{};
    snapshot.ticket = 99;
    assert(clipboard.Acquire(snapshot) == owner::Status::MalformedBundle && snapshot.ticket == 99);
    assert(clipboard.Close() == owner::Status::Ok && host.outstanding == 0);
}

void RejectionsAndRetirement() {
    Host host;
    owner::Owner clipboard;
    host.retain = false;
    assert(clipboard.Open(host.Dependencies(), host.Backend()) == owner::Status::Unavailable);
    assert(host.opens == 0);
    host.retain = true;
    assert(clipboard.Open(host.Dependencies(), host.Backend()) == owner::Status::Ok);
    auto invalid = Views();
    invalid.raw8 = {reinterpret_cast<const unsigned char*>(~std::uintptr_t(0) - 1), 4};
    assert(clipboard.Publish(invalid) == owner::Status::InvalidArgument);
    assert(host.mutations == 0);
    host.allocation = false;
    assert(clipboard.Publish(Views()) == owner::Status::NoMemory && host.mutations == 0);
    host.allocation = true;
    host.throwCodec = true;
    assert(clipboard.Publish(Views()) == owner::Status::Unavailable && host.mutations == 0);
    host.throwCodec = false;
    host.failure = 6;
    assert(clipboard.Publish(Views()) == owner::Status::NoMemory && host.mutations == 0);
    host.failure = 0;
    assert(clipboard.Publish(Views()) == owner::Status::Ok);
    host.thread = false;
    owner::Snapshot snapshot{};
    snapshot.ticket = 99;
    assert(clipboard.Acquire(snapshot) == owner::Status::WrongThread && snapshot.ticket == 99);
    host.thread = true;
    assert(clipboard.Acquire(snapshot) == owner::Status::Ok);
    host.retire = false;
    assert(clipboard.EndBorrow(snapshot) == owner::Status::ReleaseFailed);
    assert(snapshot.ticket == 0 && clipboard.Close() == owner::Status::ReleaseFailed);
    host.retire = true;
    assert(clipboard.RetryRetirements() == owner::Status::Ok);
    host.release = false;
    assert(clipboard.Close() == owner::Status::ReleaseFailed);
    assert(host.closes == 1 && host.deferred == 0);
    host.release = true;
    assert(clipboard.Close() == owner::Status::Ok);
    assert(host.closes == 1 && host.deferred == 1 && host.outstanding == 0);
}

void PartialAndMalformed() {
    for (int failure = 1; failure <= 5; ++failure) {
        Host host;
        owner::Owner clipboard;
        assert(clipboard.Open(host.Dependencies(), host.Backend()) == owner::Status::Ok);
        host.failure = failure;
        const auto expected = failure == 1 ? owner::Status::StateUncertain
            : (failure == 5 ? owner::Status::OwnershipChanged : owner::Status::FailedAfterClear);
        assert(clipboard.Publish(Views()) == expected);
        assert(host.mutations == 1);
        assert(clipboard.Close() == owner::Status::Ok && host.outstanding == 0);
    }
    Host host;
    owner::Owner clipboard;
    assert(clipboard.Open(host.Dependencies(), host.Backend()) == owner::Status::Ok);
    host.malformedPublic = true;
    assert(clipboard.Publish(Views()) == owner::Status::PrivateOnly);
    assert(host.publicBytes.empty());
    owner::Snapshot snapshot{};
    assert(clipboard.Acquire(snapshot) == owner::Status::Ok);
    assert(clipboard.EndBorrow(snapshot) == owner::Status::Ok);
    host.bytes[0] ^= 1;
    snapshot.ticket = 99;
    assert(clipboard.Acquire(snapshot) == owner::Status::MalformedBundle && snapshot.ticket == 99);
    assert(host.readReleases == 2);
    assert(clipboard.Close() == owner::Status::Ok && host.outstanding == 0);
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "empty") == 0) {
        EmptyPrivateBundle();
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "null-cleanup") == 0) {
        IndependentViewsAndMalformedEnvelope();
        return 0;
    }
    RoundTrip();
    RejectionsAndRetirement();
    PartialAndMalformed();
    IndependentViewsAndMalformedEnvelope();
    CloseFailureAndCodecAdmission();
    EmptyPrivateBundle();
    std::cout << "owner: round-trip, independent views, failure/retirement and malformed gates PASS\n";
}
