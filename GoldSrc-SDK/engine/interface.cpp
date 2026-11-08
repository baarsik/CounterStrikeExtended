#include "interface.h"
#include "../public/native_module.h"

#ifdef GetModuleHandle
#undef GetModuleHandle
#endif

InterfaceReg* InterfaceReg::s_pInterfaceRegs;

extern "C" void* CreateInterface(const char* name, int* returnCode) {
    return InterfaceReg::Create(name, returnCode);
}

CreateInterfaceFn Sys_GetFactoryThis() {
    return CreateInterface;
}

CreateInterfaceFn Sys_GetFactory(CSysModule* module) {
    return reinterpret_cast<CreateInterfaceFn>(cse_platform::GetSymbol(module, cse_platform::InterfaceFactorySymbol));
}

CSysModule* Sys_LoadModule(const char* name) {
    return static_cast<CSysModule*>(cse_platform::LoadModule(name));
}

void Sys_UnloadModule(CSysModule* module) {
    cse_platform::UnloadModule(module);
}

void* GetModuleHandle(const char* name) {
    return cse_platform::FindLoadedModule(name);
}

void* Sys_GetProcAddress(void* module, const char* name) {
    return cse_platform::GetSymbol(module, name, cse_platform::ModuleNullPolicy::MainExecutable);
}

CreateInterfaceFn Sys_GetFactory(const char* name) {
    // Preserve the engine's Win32 null-name lookup of the main executable.
    void* module = cse_platform::FindLoadedModule(name, cse_platform::ModuleNullPolicy::MainExecutable);
    return reinterpret_cast<CreateInterfaceFn>(cse_platform::GetSymbol(module, cse_platform::InterfaceFactorySymbol));
}
