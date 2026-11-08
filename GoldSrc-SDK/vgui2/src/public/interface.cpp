#include "interface.h"
#include "../../../public/native_module.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef GetModuleHandle
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

// Linux doesn't have this function so this emulates its functionality
//
//
void* GetModuleHandle(const char* name) {
    return cse_platform::FindLoadedModule(name);
}

InterfaceReg* InterfaceReg::s_pInterfaceRegs = NULL;

// CreateInterface.
extern "C" void* CreateInterface( const char* pName, int* pReturnCode )
{
	return InterfaceReg::Create( pName, pReturnCode );
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

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a function, given a module
// Input  : pModuleName - module name
//			*pName - proc name
//-----------------------------------------------------------------------------
//static hlds_run wants to use this function 
static void* Sys_GetProcAddress(const char* moduleName, const char* name) {
    return Sys_GetProcAddress(cse_platform::FindLoadedModule(moduleName), name);
}

//-----------------------------------------------------------------------------
// Purpose: Loads a DLL/component from disk and returns a handle to it
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
CSysModule* Sys_LoadModule(const char* name) {
#if defined(_WIN32)
    constexpr auto fallback = cse_platform::ModuleFallback::None;
#else
    constexpr auto fallback = cse_platform::ModuleFallback::NativeExtension;
#endif
    return static_cast<CSysModule*>(cse_platform::LoadModule(name, fallback));
}

//-----------------------------------------------------------------------------
// Purpose: Unloads a DLL/component from
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
void Sys_UnloadModule(CSysModule* module) {
    cse_platform::UnloadModule(module);
}

CreateInterfaceFn Sys_GetFactory(CSysModule* module) {
    return reinterpret_cast<CreateInterfaceFn>(cse_platform::GetSymbol(module, cse_platform::InterfaceFactorySymbol));
}

CreateInterfaceFn Sys_GetFactoryThis()
{
	return CreateInterface;
}

CreateInterfaceFn Sys_GetFactory(const char* name) {
    return reinterpret_cast<CreateInterfaceFn>(Sys_GetProcAddress(name, cse_platform::InterfaceFactorySymbol));
}
