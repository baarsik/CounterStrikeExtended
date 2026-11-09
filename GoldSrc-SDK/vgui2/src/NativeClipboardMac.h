#ifndef CSE_VGUI2_NATIVE_CLIPBOARD_MAC_H
#define CSE_VGUI2_NATIVE_CLIPBOARD_MAC_H

#include "NativeClipboardOwner.h"

namespace cse_vgui2_clipboard_owner {

// One copied, immutable private bundle. before/after counts are opaque equality
// observations, not transaction IDs. A nonnull token must be retired even on
// failed read. releaseRead false retains the entire token/storage unchanged.
struct HostRead {
    cse_vgui2_clipboard::ByteSpan bytes;
    void* token;
    std::int64_t before;
    std::int64_t after;
};

// Owned backend boundary; callbacks nonthrowing/nonreentrant. open must establish
// a fresh unique named board on the main thread, reject standard boards, and use
// exactly one item. No general binding, promised-data provider or host observer.
// prepare is the first mutation; all following failures are post-clear/uncertain.
// close success disposes context and promises no callbacks after return. Failed
// close retains all ownership for retry, including failed local retirement.
struct BackendV1 {
    std::uint32_t structBytes;
    std::uint32_t version;
    void* context;
    Status (*open)(void*);
    Status (*read)(void*, HostRead*);
    bool (*releaseRead)(void*, void*);
    // Copy/allocate ALL private/public host data before prepare mutates the
    // board. Staged copies remain backend-owned until replaced or close succeeds.
    Status (*stage)(void*, cse_vgui2_clipboard::ByteSpan, cse_vgui2_clipboard::ByteSpan, bool);
    Status (*prepare)(void*, std::int64_t*);
    Status (*addTypes)(void*, bool, std::int64_t*);
    Status (*setPrivate)(void*, cse_vgui2_clipboard::ByteSpan, std::int64_t);
    Status (*setPublic)(void*, cse_vgui2_clipboard::ByteSpan, std::int64_t);
    Status (*close)(void*);
};

// NEW UNSELECTED named-fixture backend only. No App/System/interface/manifest
// integration or General factory exists. The caller must honor the genuine
// host-root/deferred-release contract before any component call, including this
// factory. On failure use owner.HoldsRoot()/Close to retire retained ownership.
#if !defined(_WIN32) && (defined(__clang__) || defined(__GNUC__))
__attribute__((visibility("hidden")))
#endif
Status CreateNamedFixtureOwner(const DependenciesV1&, Owner&) noexcept;

} // namespace cse_vgui2_clipboard_owner
#endif
