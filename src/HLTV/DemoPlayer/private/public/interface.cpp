#include "public/tier1/interface.h"
#include "../../../../public/native_module.h"

#include <cstring>

#if !defined( _WIN32 )
#include <cstdio>
#include <dlfcn.h>
#include <unistd.h>

// Linux doesn't have this function so this emulates its functionality
//
//
void* GetModuleHandle(const char* name) {
    return cse_platform::FindLoadedModule(name);
}
#endif

InterfaceReg* InterfaceReg::s_pInterfaceRegs = NULL;

extern "C" DLL_EXPORT void* CreateInterface( const char* pName, int* pReturnCode )
{
	return InterfaceReg::Create( pName, pReturnCode );
}

#if !defined( _WIN32 )
//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a function, given a module
// Input  : pModuleName - module name
//			*pName - proc name
//-----------------------------------------------------------------------------
//static hlds_run wants to use this function 
static void* Sys_GetProcAddress(const char* moduleName, const char* name) {
    return cse_platform::GetSymbol(cse_platform::FindLoadedModule(moduleName), name);
}

void* Sys_GetProcAddress(void* module, const char* name) {
    return cse_platform::GetSymbol(module, name);
}

//-----------------------------------------------------------------------------
// Purpose: Loads a DLL/component from disk and returns a handle to it
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
CSysModule* Sys_LoadModule(const char* name) {
    return static_cast<CSysModule*>(cse_platform::LoadModule(name));
}

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
#endif
