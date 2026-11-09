#include "VGUI_EngineSurfaceWrap.h"
#include "VGui1NativeRuntime.h"
#include "engine_apiproxy.h"
#include <cstdlib>
#include "ivideomode.h"
#include "vgui_int.h"
#include "vgui_intwrap2.h"
#include "cdll_int.h"
#include "interface.h"
#include <SDL3/SDL.h>
#include "abort_bindings.inc"

cl_enginefunc_dst_t g_engdstAddrs{};
extern "C" void ClientDLL_ActivateMouse() {
    std::abort();
}
extern "C" void ClientDLL_DeactivateMouse() {
    std::abort();
}
extern "C" void SetMouseEnable(qboolean) {
    std::abort();
}
extern "C" bool SDLCALL SDL_GetWindowPosition(SDL_Window*, int*, int*) {
    std::abort();
}
extern "C" bool SDLCALL SDL_GetWindowSizeInPixels(SDL_Window*, int*, int*) {
    std::abort();
}
extern "C" int VideoMode_IsWindowed() {
    std::abort();
}
extern "C" void VideoMode_GetCurrentVideoMode(int*, int*, int*) {
    std::abort();
}
CreateInterfaceFn Sys_GetFactoryThis() {
    std::abort();
}
int VGuiWrap2_UseVGUI1() {
    std::abort();
}
void VGui_ViewportPaintBackground(int*) {
    std::abort();
}
namespace cse_vgui1_native {
Result DetachEmbedded(EngineSurfaceWrap*) noexcept {
    std::abort();
}
Runtime& EmbeddedRuntime() noexcept {
    std::abort();
}
[[noreturn]] void InvalidEmbeddedUse(const char*) noexcept {
    std::abort();
}
Scope::Scope(Runtime& runtime) noexcept : runtime_(runtime), status_(Result::Unavailable) {
    (void) runtime_;
    std::abort();
}
Scope::~Scope() noexcept {
    std::abort();
}
}
