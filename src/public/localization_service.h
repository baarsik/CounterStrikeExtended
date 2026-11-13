#ifndef CSE_PUBLIC_LOCALIZATION_SERVICE_H
#define CSE_PUBLIC_LOCALIZATION_SERVICE_H

#include <cstdint>
#include "interface.h"
#include "cse_module_abi.h"

class IFileSystem;
struct client_textmessage_s;

namespace cse {
namespace localization {

constexpr char EngineInterface[] = "CSEEngineLocalization001";
constexpr char RuntimeInterface[] = "CSEVGuiLocalization001";
constexpr std::uint32_t InterfaceVersion = 1;
constexpr std::uint64_t InvalidRequest = 0;
using FontResetCallback = void (CSE_ABI_CALL *)(std::uint32_t font);

enum class RequestState : std::uint32_t {
    Pending,
    Applied,
    Failed,
    Superseded
};

struct RequestResult {
    std::uint32_t size;
    RequestState state;
    std::uint64_t revision;
};

// Owned by the engine. Borrow only while the owning engine module is alive.
// RequestLanguage queues work; all mutation occurs at an accepted engine frame.
struct EngineApi {
    std::uint32_t size;
    std::uint32_t version;
    const char* (CSE_ABI_CALL *Language)();
    const char* (CSE_ABI_CALL *Preference)();
    std::uint64_t (CSE_ABI_CALL *Revision)();
    std::uint64_t (CSE_ABI_CALL *RequestLanguage)(const char* preference, bool persist);
    bool (CSE_ABI_CALL *GetRequestResult)(std::uint64_t request, RequestResult* result);
    const char* (CSE_ABI_CALL *LastError)();
    bool (CSE_ABI_CALL *IsCatalogueMessage)(const client_textmessage_s* message);
    bool (CSE_ABI_CALL *ReplaceCenterText)(const char* expectedText, const char* replacementText);
};

// Font handles belong to the selected surface owner. Preparing may cache new
// faces, but active ranges/metrics change only at Commit.
struct FontBackend {
    std::uint32_t size;
    std::uint32_t version;
    bool (CSE_ABI_CALL *Begin)(const char* language);
    bool (CSE_ABI_CALL *Stage)(std::uint32_t font, const char* name, int tall, int weight,
        int blur, int scanlines, int flags);
    void (CSE_ABI_CALL *Finish)();
    void (CSE_ABI_CALL *Commit)();
    void (CSE_ABI_CALL *Discard)();
};

// Owned by vgui2. Prepare is non-destructive; Commit cannot fail after Prepare.
// The engine updates mounts/surface language before committing this provider.
struct RuntimeApi {
    std::uint32_t size;
    std::uint32_t version;
    bool (CSE_ABI_CALL *PrepareLanguage)(IFileSystem* filesystem, const char* language);
    void (CSE_ABI_CALL *CommitLanguage)(std::uint64_t revision);
    void (CSE_ABI_CALL *DiscardPrepared)();
    std::uint64_t (CSE_ABI_CALL *Revision)();
    void (CSE_ABI_CALL *BindFontReset)(FontResetCallback reset);
    const FontBackend* (CSE_ABI_CALL *BindFontBackend)(const FontBackend* backend);
};

inline const EngineApi* QueryEngine(CreateInterfaceFn factory) {
    const auto* api = factory ? static_cast<const EngineApi*>(factory(EngineInterface, nullptr)) : nullptr;
    if (!api || api->size < sizeof(EngineApi) || api->version != InterfaceVersion || !api->Language
        || !api->Preference || !api->Revision || !api->RequestLanguage || !api->GetRequestResult
        || !api->LastError || !api->IsCatalogueMessage || !api->ReplaceCenterText) {
        return nullptr;
    }
    return api;
}

inline const RuntimeApi* QueryRuntime(CreateInterfaceFn factory) {
    const auto* api = factory ? static_cast<const RuntimeApi*>(factory(RuntimeInterface, nullptr)) : nullptr;
    if (!api || api->size < sizeof(RuntimeApi) || api->version != InterfaceVersion || !api->PrepareLanguage
        || !api->CommitLanguage || !api->DiscardPrepared || !api->Revision || !api->BindFontReset || !api->BindFontBackend) {
        return nullptr;
    }
    return api;
}

} // namespace localization
} // namespace cse

#endif
