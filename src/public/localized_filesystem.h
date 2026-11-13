#ifndef CSE_PUBLIC_LOCALIZED_FILESYSTEM_H
#define CSE_PUBLIC_LOCALIZED_FILESYSTEM_H

#include "interface_registry.h"
#include <cstddef>
#include <cstdint>

class IFileSystem;

namespace cse {
namespace localized_filesystem {

constexpr char InterfaceName[] = "CSELocalizedFileSystem001";
constexpr std::uint32_t Version = 1;

struct Replacement {
    std::uint32_t group;
    const char* anchor;
    const char* pathId;
    const char* const* roots;
    std::size_t count;
    bool writable;
};

// Private extension: the public filesystem vtable stays unchanged. Views are
// read-only preparation borrows and expire at Commit/Discard/Reset.
struct Api {
    std::uint32_t size;
    std::uint32_t version;
    void (*BeginGroup)(std::uint32_t group);
    void (*EndGroup)();
    IFileSystem* (*Prepare)(IFileSystem* owner, const Replacement* groups, std::size_t count);
    IFileSystem* (*Baseline)();
    void (*Commit)();
    void (*Discard)();
    void (*Reset)();
};

inline const Api* Query(CreateInterfaceFn factory) {
    const auto* api = factory ? static_cast<const Api*>(factory(InterfaceName, nullptr)) : nullptr;
    return api && api->size == sizeof(Api) && api->version == Version && api->BeginGroup && api->EndGroup
        && api->Prepare && api->Baseline && api->Commit && api->Discard && api->Reset ? api : nullptr;
}

} // namespace localized_filesystem
} // namespace cse

#endif
