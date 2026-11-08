#include "basefilesystem.h"
#include "../public/native_module.h"

#include <cstdio>
#include <cstring>

#ifndef _WIN32
#define FS_MODULE_PATH_CAPACITY	((size_t)1024)
#define FS_MODULE_SUFFIX_LIMIT	((size_t)512)
#endif // LINUX

CSysModule* Sys_LoadModule(const char* name) {
#if defined(_WIN32)
    constexpr auto fallback = cse_platform::ModuleFallback::None;
#else
    constexpr auto fallback = cse_platform::ModuleFallback::NativeExtension;
#endif
    return static_cast<CSysModule*>(cse_platform::LoadModule(name, fallback));
}

void Sys_UnloadModule(CSysModule* module) {
    cse_platform::UnloadModule(module);
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a function, given a module
// Input  : pModuleName - module name
//			*pName - proc name
//-----------------------------------------------------------------------------
// hlds_run wants to use this function 
void* Sys_GetProcAddress(void* module, const char* name) {
    return cse_platform::GetSymbol(module, name, cse_platform::ModuleNullPolicy::MainExecutable);
}

CreateInterfaceFn Sys_GetFactory(CSysModule* module) {
    return reinterpret_cast<CreateInterfaceFn>(cse_platform::GetSymbol(module, cse_platform::InterfaceFactorySymbol));
}

CreateInterfaceFn Sys_GetFactoryThis() {
    return CreateInterface;
}

CreateInterfaceFn Sys_GetFactory(const char* name) {
    void* module = cse_platform::FindLoadedModule(name, cse_platform::ModuleNullPolicy::MainExecutable);
    return reinterpret_cast<CreateInterfaceFn>(cse_platform::GetSymbol(module, cse_platform::InterfaceFactorySymbol));
}

InterfaceReg* InterfaceReg::s_pInterfaceRegs = NULL;
