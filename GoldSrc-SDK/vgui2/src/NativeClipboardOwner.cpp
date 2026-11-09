#include "NativeClipboardOwner.h"
#include "NativeClipboardMac.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

namespace cse_vgui2_clipboard_owner {
namespace core = cse_vgui2_clipboard;
namespace {
constexpr unsigned char BundleMagic[] = {'C', 'S', 'E', 'C', 'L', 'P', '1', 0};
constexpr std::size_t WordBytes = sizeof(std::uint32_t);
constexpr std::size_t WideWordBytes = sizeof(std::uint64_t);
constexpr std::size_t VersionOffset = sizeof(BundleMagic);
constexpr std::size_t FlagsOffset = VersionOffset + WordBytes;
constexpr std::size_t HeaderSizeOffset = FlagsOffset + WordBytes;
constexpr std::size_t Raw8SizeOffset = HeaderSizeOffset + WideWordBytes;
constexpr std::size_t UnicodeSizeOffset = Raw8SizeOffset + WideWordBytes;
constexpr std::size_t UnitExtentOffset = UnicodeSizeOffset + WideWordBytes;
constexpr std::size_t EncodingOffset = UnitExtentOffset + WideWordBytes;
constexpr std::size_t ReservedOffset = EncodingOffset + WordBytes;
constexpr std::size_t TotalSizeOffset = ReservedOffset + WordBytes;
constexpr std::uint64_t HeaderBytes = TotalSizeOffset + WideWordBytes;
static_assert(HeaderBytes == 64, "Version1 serializes a fixed LE header, never native structure padding");
constexpr std::uint32_t Raw8Flag = 1;
constexpr std::uint32_t UnicodeFlag = 2;
constexpr std::uint64_t UnitBytes = 2;
constexpr unsigned int ByteBits = 8;
constexpr std::uint64_t ByteMask = 0xff;
constexpr std::uint64_t HighSurrogateFirst = 0xd800;
constexpr std::uint64_t HighSurrogateLast = 0xdbff;
constexpr std::uint64_t LowSurrogateFirst = 0xdc00;
constexpr std::uint64_t LowSurrogateLast = 0xdfff;

bool Span(core::ByteSpan span) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(span.data);
    return span.storageBytes == 0 || (span.data && span.storageBytes <= SIZE_MAX
        && span.storageBytes <= UINTPTR_MAX - address);
}

bool Add(std::uint64_t first, std::uint64_t second, std::uint64_t& sum) noexcept {
    if (second > UINT64_MAX - first) {
        return false;
    }
    sum = first + second;
    return true;
}

void Put(unsigned char* destination, std::uint64_t value, std::size_t bytes) noexcept {
    for (std::size_t index = 0; index < bytes; ++index) {
        destination[index] = static_cast<unsigned char>(value & ByteMask);
        value >>= ByteBits;
    }
}

std::uint64_t Get(const unsigned char* source, std::size_t bytes) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < bytes; ++index) {
        value |= static_cast<std::uint64_t>(source[index]) << (index * ByteBits);
    }
    return value;
}

bool StrictUnits(core::ByteSpan source) noexcept {
    bool high = false;
    for (std::uint64_t offset = 0; offset < source.storageBytes; offset += UnitBytes) {
        const auto unit = Get(source.data + static_cast<std::size_t>(offset), UnitBytes);
        if (high) {
            if (unit < LowSurrogateFirst || unit > LowSurrogateLast) {
                return false;
            }
            high = false;
        } else if (unit >= HighSurrogateFirst && unit <= HighSurrogateLast) {
            high = true;
        } else if (unit >= LowSurrogateFirst && unit <= LowSurrogateLast) {
            return false;
        }
    }
    return !high;
}

struct Allocation {
    const DependenciesV1& dependencies;
    unsigned char* data = nullptr;
    ~Allocation() noexcept {
        if (data) {
            dependencies.deallocate(dependencies.context, data);
        }
    }
    bool Make(std::uint64_t size) {
        if (size > SIZE_MAX) {
            return false;
        }
        if (size != 0) {
            data = static_cast<unsigned char*>(dependencies.allocate(dependencies.context,
                static_cast<std::size_t>(size)));
        }
        return size == 0 || data;
    }
};

bool Complete(const DependenciesV1& dependencies, const BackendV1& backend) noexcept {
    return dependencies.structBytes == sizeof(dependencies) && dependencies.version == ContractVersion
        && dependencies.context && dependencies.ownerEpoch != 0 && dependencies.ownerThread
        && dependencies.retainRoot && dependencies.deferRootRelease && dependencies.allocate
        && dependencies.deallocate && backend.structBytes == sizeof(backend) && backend.version == ContractVersion
        && backend.context && backend.open && backend.read && backend.releaseRead && backend.stage && backend.prepare
        && backend.addTypes && backend.setPrivate && backend.setPublic && backend.close;
}

Status PostMutation(Status status) noexcept {
    return status == Status::OwnershipChanged || status == Status::StateUncertain
        ? status : Status::FailedAfterClear;
}
}

struct Owner::Impl {
    struct Node {
        Node* next = nullptr;
        HostRead read{};
        unsigned char* storage = nullptr;
        std::uint64_t serial = 0;
        bool ended = false;
    };
    DependenciesV1 dependencies{};
    BackendV1 backend{};
    Node* nodes = nullptr;
    std::uint64_t root = 0;
    std::uint64_t serial = 0;
    bool held = false;
    bool closing = false;
    bool busy = false;

    Status Enter(bool allowClosing = false) noexcept {
        if (busy) {
            return Status::Busy;
        }
        if (!held || (closing && !allowClosing)) {
            return Status::Closed;
        }
        return dependencies.ownerThread(dependencies.context) ? Status::Ok : Status::WrongThread;
    }
    bool Retire(Node* node) noexcept {
        if (node->read.token && !backend.releaseRead(backend.context, node->read.token)) {
            return false;
        }
        if (node->storage) {
            dependencies.deallocate(dependencies.context, node->storage);
        }
        node->~Node();
        dependencies.deallocate(dependencies.context, node);
        return true;
    }
    Status Parse(Node* node, Snapshot& output) noexcept {
        const auto size = node->read.bytes.storageBytes;
        const auto* bytes = node->storage;
        if (size < HeaderBytes || std::memcmp(bytes, BundleMagic, sizeof(BundleMagic)) != 0
            || Get(bytes + VersionOffset, WordBytes) != ContractVersion
            || Get(bytes + HeaderSizeOffset, WideWordBytes) != HeaderBytes
            || Get(bytes + ReservedOffset, WordBytes) != 0 || Get(bytes + TotalSizeOffset, WideWordBytes) != size) {
            return Status::MalformedBundle;
        }
        const auto flags = Get(bytes + FlagsOffset, WordBytes);
        const auto raw8Bytes = Get(bytes + Raw8SizeOffset, WideWordBytes);
        const auto unicodeBytes = Get(bytes + UnicodeSizeOffset, WideWordBytes);
        const auto extent = Get(bytes + UnitExtentOffset, WideWordBytes);
        std::uint64_t total = 0;
        if ((flags & ~(Raw8Flag | UnicodeFlag)) != 0 || ((flags & Raw8Flag) == 0 && raw8Bytes != 0)
            || ((flags & UnicodeFlag) == 0 && (unicodeBytes != 0 || extent != 0))
            || !Add(HeaderBytes, raw8Bytes, total) || !Add(total, unicodeBytes, total) || total != size) {
            return Status::MalformedBundle;
        }
        Snapshot parsed{};
        parsed.epoch = dependencies.ownerEpoch;
        parsed.ticket = node->serial;
        parsed.raw8 = {(flags & Raw8Flag) ? bytes + HeaderBytes : nullptr, raw8Bytes};
        parsed.raw8EncodingTag = static_cast<std::uint32_t>(Get(bytes + EncodingOffset, WordBytes));
        parsed.unicode = {node->serial, (flags & Raw8Flag) != 0, raw8Bytes, (flags & UnicodeFlag) != 0,
            {(flags & UnicodeFlag) ? bytes + HeaderBytes + raw8Bytes : nullptr, unicodeBytes}, extent};
        std::int32_t count = 0;
        parsed.raw8Status = core::CountLegacyText(parsed.unicode, count);
        unsigned char byte = 0;
        parsed.unicodeBytesStatus = core::ReadUnicodeBytes(parsed.unicode, 0, {&byte, sizeof(byte)}, count);
        wchar_t cell = 0;
        parsed.unicodeCellsStatus = core::ReadUnicodeCells(parsed.unicode, 0, {&cell, sizeof(cell)}, count);
        output = parsed;
        return Status::Ok;
    }
};

namespace {
struct BusyGuard {
    bool& busy;
    explicit BusyGuard(bool& value) noexcept : busy(value) { busy = true; }
    ~BusyGuard() noexcept { busy = false; }
};
}

Owner::~Owner() noexcept {
    if (HoldsRoot()) {
        std::fputs("Native clipboard owner destroyed before successful retirement\n", stderr);
        std::abort();
    }
    delete impl_;
}

bool Owner::HoldsRoot() const noexcept {
    return impl_ && impl_->held;
}

Status Owner::Open(const DependenciesV1& dependencies, const BackendV1& backend) noexcept {
    if (HoldsRoot()) {
        return Status::Busy;
    }
    if (!Complete(dependencies, backend)) {
        return Status::InvalidArgument;
    }
    auto* state = new (std::nothrow) Impl;
    if (!state) {
        return Status::NoMemory;
    }
    if (!dependencies.retainRoot(dependencies.context, &state->root)) {
        delete state;
        return Status::Unavailable;
    }
    state->held = true;
    state->dependencies = dependencies;
    state->backend = backend;
    delete impl_;
    impl_ = state;
    const auto admitted = state->Enter();
    if (admitted != Status::Ok) {
        state->closing = true;
        return admitted;
    }
    BusyGuard guard(state->busy);
    const auto result = backend.open(backend.context);
    state->closing = result != Status::Ok;
    return result;
}

Status Owner::Publish(const PreparedViews& views) noexcept {
    if (!impl_) {
        return Status::Closed;
    }
    auto& state = *impl_;
    const auto admitted = state.Enter();
    if (admitted != Status::Ok) {
        return admitted;
    }
    BusyGuard guard(state.busy);
    const auto& dependencies = state.dependencies;
    if ((!views.hasRaw8 && views.raw8.storageBytes != 0)
        || (!views.hasUnicode && (views.unicodeStorage.storageBytes != 0 || views.unicodeUnitExtentBytes != 0))
        || !Span(views.raw8) || !Span(views.unicodeStorage)
        || (views.publishPublic && (!views.hasUnicode || views.publicUnitExtentBytes % UnitBytes != 0
            || views.publicUnitExtentBytes > views.unicodeUnitExtentBytes
            || views.publicUnitExtentBytes > views.unicodeStorage.storageBytes))) {
        return Status::InvalidArgument;
    }
    if (views.publishPublic && !dependencies.encodePublic) {
        return Status::CodecUnavailable;
    }
    std::uint64_t total = 0;
    if (!Add(HeaderBytes, views.raw8.storageBytes, total)
        || !Add(total, views.unicodeStorage.storageBytes, total) || total > SIZE_MAX) {
        return Status::InvalidArgument;
    }
    bool mutated = false;
    try {
        Allocation bundle{dependencies};
        Allocation publicData{dependencies};
        if (!bundle.Make(total)) {
            return Status::NoMemory;
        }
        std::memcpy(bundle.data, BundleMagic, sizeof(BundleMagic));
        Put(bundle.data + VersionOffset, ContractVersion, WordBytes);
        const auto flags = (views.hasRaw8 ? Raw8Flag : 0) | (views.hasUnicode ? UnicodeFlag : 0);
        Put(bundle.data + FlagsOffset, flags, WordBytes);
        Put(bundle.data + HeaderSizeOffset, HeaderBytes, WideWordBytes);
        Put(bundle.data + Raw8SizeOffset, views.raw8.storageBytes, WideWordBytes);
        Put(bundle.data + UnicodeSizeOffset, views.unicodeStorage.storageBytes, WideWordBytes);
        Put(bundle.data + UnitExtentOffset, views.unicodeUnitExtentBytes, WideWordBytes);
        Put(bundle.data + EncodingOffset, views.raw8EncodingTag, WordBytes);
        Put(bundle.data + ReservedOffset, 0, WordBytes);
        Put(bundle.data + TotalSizeOffset, total, WideWordBytes);
        if (views.raw8.storageBytes != 0) {
            std::memcpy(bundle.data + HeaderBytes, views.raw8.data, static_cast<std::size_t>(views.raw8.storageBytes));
        }
        if (views.unicodeStorage.storageBytes != 0) {
            std::memcpy(bundle.data + HeaderBytes + views.raw8.storageBytes, views.unicodeStorage.data,
                static_cast<std::size_t>(views.unicodeStorage.storageBytes));
        }
        bool publishPublic = views.publishPublic;
        std::uint64_t publicBytes = 0;
        if (publishPublic) {
            const core::ByteSpan units{bundle.data + HeaderBytes + views.raw8.storageBytes,
                views.publicUnitExtentBytes};
            Status encoded = StrictUnits(units) ? dependencies.encodePublic(dependencies.context,
                units, {nullptr, 0}, &publicBytes) : Status::Unrepresentable;
            if (encoded == Status::Unrepresentable) {
                publishPublic = false;
            } else {
                if (encoded != Status::Ok) {
                    return encoded;
                }
                if (!publicData.Make(publicBytes)) {
                    return Status::NoMemory;
                }
                std::uint64_t written = 0;
                encoded = dependencies.encodePublic(dependencies.context, units,
                    {publicData.data, publicBytes}, &written);
                if (encoded != Status::Ok || written != publicBytes) {
                    return encoded == Status::Ok ? Status::Unavailable : encoded;
                }
            }
        }
        std::int64_t change = 0;
        auto result = state.backend.stage(state.backend.context, {bundle.data, total},
            {publicData.data, publicBytes}, publishPublic);
        if (result != Status::Ok) {
            return result;
        }
        mutated = true; // A throwing/failed prepare may already have cleared.
        result = state.backend.prepare(state.backend.context, &change);
        if (result != Status::Ok) {
            return PostMutation(result);
        }
        result = state.backend.addTypes(state.backend.context, publishPublic, &change);
        if (result != Status::Ok) {
            return PostMutation(result);
        }
        result = state.backend.setPrivate(state.backend.context, {bundle.data, total}, change);
        if (result != Status::Ok) {
            return PostMutation(result);
        }
        if (publishPublic) {
            result = state.backend.setPublic(state.backend.context, {publicData.data, publicBytes}, change);
            if (result != Status::Ok) {
                return PostMutation(result);
            }
        }
        return publishPublic ? Status::Ok : Status::PrivateOnly;
    } catch (...) {
        return mutated ? Status::StateUncertain : Status::Unavailable;
    }
}

Status Owner::Acquire(Snapshot& output) noexcept {
    if (!impl_) {
        return Status::Closed;
    }
    auto& state = *impl_;
    const auto admitted = state.Enter();
    if (admitted != Status::Ok) {
        return admitted;
    }
    if (state.serial == UINT64_MAX) {
        return Status::Exhausted;
    }
    BusyGuard guard(state.busy);
    void* memory = state.dependencies.allocate(state.dependencies.context, sizeof(Impl::Node));
    if (!memory) {
        return Status::NoMemory;
    }
    auto* node = new (memory) Impl::Node;
    node->serial = ++state.serial;
    node->next = state.nodes;
    state.nodes = node;
    auto result = state.backend.read(state.backend.context, &node->read);
    if (result == Status::Ok && (!node->read.token || !Span(node->read.bytes))) {
        result = Status::HostFailure;
    }
    if (result == Status::Ok && node->read.before != node->read.after) {
        result = Status::OwnershipChanged;
    }
    if (result == Status::Ok && node->read.bytes.storageBytes < HeaderBytes) {
        result = Status::MalformedBundle;
    }
    if (result == Status::Ok) {
        node->storage = static_cast<unsigned char*>(state.dependencies.allocate(state.dependencies.context,
            static_cast<std::size_t>(node->read.bytes.storageBytes)));
        if (!node->storage) {
            result = Status::NoMemory;
        } else {
            std::memcpy(node->storage, node->read.bytes.data, static_cast<std::size_t>(node->read.bytes.storageBytes));
            result = state.Parse(node, output);
        }
    }
    if (result != Status::Ok) {
        node->ended = true;
        auto* next = node->next;
        if (!state.Retire(node)) {
            return Status::ReleaseFailed;
        }
        state.nodes = next;
    }
    return result;
}

Status Owner::EndBorrow(Snapshot& snapshot) noexcept {
    if (!impl_) {
        return Status::Closed;
    }
    auto& state = *impl_;
    const auto admitted = state.Enter(true);
    if (admitted != Status::Ok) {
        return admitted;
    }
    if (snapshot.epoch != state.dependencies.ownerEpoch || snapshot.ticket == 0) {
        return Status::InvalidArgument;
    }
    BusyGuard guard(state.busy);
    auto** link = &state.nodes;
    while (*link && (*link)->serial != snapshot.ticket) {
        link = &(*link)->next;
    }
    if (!*link || (*link)->ended) {
        return Status::InvalidArgument;
    }
    auto* node = *link;
    node->ended = true;
    snapshot = {};
    auto* next = node->next;
    if (!state.Retire(node)) {
        return Status::ReleaseFailed;
    }
    *link = next;
    return Status::Ok;
}

Status Owner::RetryRetirements() noexcept {
    if (!impl_) {
        return Status::Closed;
    }
    auto& state = *impl_;
    const auto admitted = state.Enter(true);
    if (admitted != Status::Ok) {
        return admitted;
    }
    BusyGuard guard(state.busy);
    Status result = Status::Ok;
    auto** link = &state.nodes;
    while (*link) {
        auto* node = *link;
        auto* next = node->next;
        if (node->ended && state.Retire(node)) {
            *link = next;
        } else {
            if (node->ended) {
                result = Status::ReleaseFailed;
            }
            link = &node->next;
        }
    }
    return result;
}

Status Owner::Close() noexcept {
    if (!impl_ || !impl_->held) {
        return Status::Ok;
    }
    auto& state = *impl_;
    const auto admitted = state.Enter(true);
    if (admitted != Status::Ok) {
        return admitted;
    }
    BusyGuard guard(state.busy);
    state.closing = true;
    for (auto* node = state.nodes; node; node = node->next) {
        if (!node->ended) {
            return Status::Busy;
        }
    }
    if (state.nodes) {
        return Status::ReleaseFailed;
    }
    if (state.backend.context) {
        const auto closed = state.backend.close(state.backend.context);
        if (closed != Status::Ok) {
            return closed;
        }
        state.backend = {};
    }
    if (!state.dependencies.deferRootRelease(state.dependencies.context, state.root)) {
        return Status::ReleaseFailed;
    }
    state.held = false;
    return Status::Ok;
}

} // namespace cse_vgui2_clipboard_owner
