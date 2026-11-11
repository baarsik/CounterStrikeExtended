#include "public/native_software_lifetime.h"
#include "public/native_vgui_completion.h"
#include <cstdlib>
using namespace cse::software_lifetime;
static unsigned int bindCalls = 0;
static Result Bind(const Binding*) noexcept {
    ++bindCalls;
    return Result::Ready;
}
static Result Retire() noexcept {
    return Result::Retired;
}
extern "C" __attribute__((visibility("default"))) unsigned int BindCalls() {
    return bindCalls;
}
extern "C" __attribute__((visibility("default"))) void* CreateInterface(const char*, int*) {
    return nullptr;
}
extern "C" __attribute__((visibility("default"))) const Api CseNativeSoftwareLifetimeV1 = {
#if CSE_BAD_CONTRACT == 1
    sizeof(Api) - sizeof(void*), Version, Contract, Bind, Retire
#elif CSE_BAD_CONTRACT == 2
    sizeof(Api), Version + 1, Contract, Bind, Retire
#elif CSE_BAD_CONTRACT == 3
    sizeof(Api), Version, Contract + 1, Bind, Retire
#else
    sizeof(Api), Version, Contract, Bind, reinterpret_cast<Result (*)() noexcept>(&std::abort)
#endif
};

static cse::vgui_completion::Result FinishVgui(const void*) noexcept {
    return cse::vgui_completion::Result::Complete;
}
extern "C" __attribute__((visibility("default"))) const cse::vgui_completion::Api CseNativeVguiCompletionV1 = {
#if CSE_BAD_CONTRACT == 1
    sizeof(cse::vgui_completion::Api) - sizeof(void*), cse::vgui_completion::Version, FinishVgui
#elif CSE_BAD_CONTRACT == 2
    sizeof(cse::vgui_completion::Api), cse::vgui_completion::Version + 1, FinishVgui
#elif CSE_BAD_CONTRACT == 3
    sizeof(cse::vgui_completion::Api), cse::vgui_completion::Version, nullptr
#else
    sizeof(cse::vgui_completion::Api), cse::vgui_completion::Version,
    reinterpret_cast<cse::vgui_completion::Result (*)(const void*) noexcept>(&std::abort)
#endif
};
