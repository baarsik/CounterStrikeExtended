#pragma once

#include "VGui1HostImageOwner.h"
#include "SoftwareImageOwner.h"
#include "SocketImageOwner.h"
#include "../../public/native_runtime_host.h"

namespace cse_mac_runtime {

enum class EngineImage { Hardware, Software };

class Session {
public:
    ~Session() noexcept;
    static bool Available(EngineImage) noexcept;
    static bool Admit(const char* enginePath, const char* fontPath, EngineImage image) noexcept;
    // Cold admission precedes the font preload and software image constructors.
    static void* LoadAdmitted(const char* enginePath, const char* fontPath, EngineImage image,
                              void*& ownedFont) noexcept;
    void* Load(const char* enginePath, const char* fontPath, EngineImage image) noexcept;
    cse::software_lifetime::ImageOwner* SoftwareOwner() noexcept {
        return softwareSelected_ ? &software_ : nullptr;
    }
    // adopted is set only after the main-image owner takes the caller's real
    // load reference. The caller must then never release that reference again.
    bool Open(void* ownedEngine, const char* enginePath, const char* fontPath, bool& adopted) noexcept;
    bool Close() noexcept;

private:
    cse_vgui1_host::ImageOwner owner_;
    cse::software_lifetime::ImageOwner software_;
    cse::socket_lifetime::SocketImageOwner sockets_;
    void* fontPreload_ = nullptr;
    bool softwareSelected_ = false;
    cse::native_runtime::Close close_ = nullptr;
    bool entered_ = false;
    bool active_ = false;
};

} // namespace cse_mac_runtime
