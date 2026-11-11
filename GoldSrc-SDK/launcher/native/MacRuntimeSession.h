#pragma once

#include "VGui1HostImageOwner.h"
#include "../../public/native_runtime_host.h"

namespace cse_mac_runtime {

enum class EngineImage { Hardware, Software };

class Session {
public:
    ~Session() noexcept;
    static bool Available(EngineImage) noexcept;
    static bool Admit(const char* enginePath, const char* fontPath, EngineImage image) noexcept;
    // adopted is set only after the main-image owner takes the caller's real
    // load reference. The caller must then never release that reference again.
    bool Open(void* ownedEngine, const char* enginePath, const char* fontPath, bool& adopted) noexcept;
    bool Close() noexcept;

private:
    cse_vgui1_host::ImageOwner owner_;
    cse::native_runtime::Close close_ = nullptr;
    bool entered_ = false;
    bool active_ = false;
};

} // namespace cse_mac_runtime
