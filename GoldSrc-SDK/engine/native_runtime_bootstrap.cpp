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
    return static_cast<int>(cse_vgui1_native::CloseEmbedded());
}
