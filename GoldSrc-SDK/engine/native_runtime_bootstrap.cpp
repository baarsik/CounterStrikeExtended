#if defined(__APPLE__)
#include "native_socket_lifetime.h"
#endif
#if defined(__APPLE__) && defined(SW_RENDERER)
#include "native_software_lifetime.h"
#endif
#include "native_gameui_retirement.h"
#include "VGui1NativeRuntime.h"
#include "native_mouse_window.h"
#include <pthread.h>
#include "../public/native_runtime_host.h"
#include "../public/cse_export.h"

extern "C" CSE_PUBLIC_EXPORT int CseNativeRuntimeOpenV1(const cse::native_runtime::BindingV1* binding) noexcept {
    if (!binding || binding->structBytes != sizeof(*binding)
        || binding->version != cse::native_runtime::BindingVersion) {
        return static_cast<int>(cse_vgui1_native::Result::InvalidArgument);
    }
#if defined(__APPLE__) && defined(SW_RENDERER)
    cse::software_lifetime::Entry entry(cse::software_lifetime::Mode::Client,
        cse::software_lifetime::EntryKind::Outermost);
    if (!entry) {
        return static_cast<int>(cse_vgui1_native::Result::Busy);
    }
#endif
#if defined(__APPLE__)
    if (!cse::socket_lifetime::Admitted()) {
        return static_cast<int>(cse_vgui1_native::Result::Busy);
    }
#endif
    // Explicit admitted Open owns this main-thread identity even when Run is never entered.
    // The helper rejects conflicting use/retirement and does not initialize platform resources.
    if (!cse::InitializeNativeGameUIHost()) {
        return static_cast<int>(cse_vgui1_native::Result::Busy);
    }
    const cse_vgui1_native::BindingV2 runtime{binding->epoch, binding->enginePath, binding->fontPath, binding->host};
    return static_cast<int>(cse_vgui1_native::ConfigureEmbedded(runtime));
}

extern "C" CSE_PUBLIC_EXPORT int CseNativeRuntimeCloseV1() noexcept {
    // SDL has no thread identity before initialization; guard the true OS main
    // thread before reading the engine-private deferred cleanup or window owner.
    if (!pthread_main_np() || !cse::FinishNativeListenShutdown() ||
        !cse::NativeMouseWindowState().Invalidate(nullptr)) {
        return static_cast<int>(cse_vgui1_native::Result::Busy);
    }
    const auto result = cse_vgui1_native::CloseEmbedded();
#if defined(__APPLE__)
    if (result == cse_vgui1_native::Result::Closed &&
        cse::socket_lifetime::Retire() != cse::socket_lifetime::Status::Retired) {
        return static_cast<int>(cse_vgui1_native::Result::Busy);
    }
#endif
    return static_cast<int>(result);
}
