#include <string.h>
#include "native_module.h"
#include <stdlib.h>
#include <stdio.h>
#include "interface.h"

#if !defined ( _WIN32 )
// Linux doesn't have this function so this emulates its functionality
//
//
void* GetModuleHandle(const char* name) {
    return cse_platform::FindLoadedModule(name);
}
#endif

// ------------------------------------------------------------------------------------ //
// InterfaceReg.
// ------------------------------------------------------------------------------------ //
InterfaceReg *InterfaceReg::s_pInterfaceRegs = NULL;




// ------------------------------------------------------------------------------------ //
// CreateInterface.
// ------------------------------------------------------------------------------------ //
EXPORT_FUNCTION void *CreateInterface( const char *pName, int *pReturnCode )
{
	return InterfaceReg::Create( pName, pReturnCode );
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a function, given a module
// Input  : pModuleName - module name
//			*pName - proc name
//-----------------------------------------------------------------------------
//static hlds_run wants to use this function 
static void* Sys_GetProcAddress(const char* moduleName, const char* name) {
    return cse_platform::GetSymbol(cse_platform::FindLoadedModule(moduleName, cse_platform::ModuleNullPolicy::MainExecutable), name);
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
// Purpose: Loads a DLL/component from disk and returns a handle to it
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
CSysModule* Sys_LoadModule(const char* name) {
    return static_cast<CSysModule*>(cse_platform::LoadModule(name));
}

//-----------------------------------------------------------------------------
// Purpose: Unloads a DLL/component from
// Input  : *pModuleName - filename of the component
// Output : opaque handle to the module (hides system dependency)
//-----------------------------------------------------------------------------
void Sys_UnloadModule(CSysModule* module) {
    cse_platform::UnloadModule(module);
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to a function, given a module
// Input  : module - windows HMODULE from Sys_LoadModule() 
//			*pName - proc name
// Output : factory for this module
//-----------------------------------------------------------------------------
CreateInterfaceFn Sys_GetFactory(CSysModule* module) {
    return reinterpret_cast<CreateInterfaceFn>(cse_platform::GetSymbol(module, cse_platform::InterfaceFactorySymbol));
}



//-----------------------------------------------------------------------------
// Purpose: returns the instance of this module
// Output : interface_instance_t
//-----------------------------------------------------------------------------
CreateInterfaceFn Sys_GetFactoryThis( void )
{
	// BEN-NOTE: unifying this on all platforms
//#ifdef LINUX
#if 0
	return CreateInterfaceLocal;
#else
	return CreateInterface;
#endif // LINUX

}

//-----------------------------------------------------------------------------
// Purpose: returns the instance of the named module
// Input  : *pModuleName - name of the module
// Output : interface_instance_t - instance of that module
//-----------------------------------------------------------------------------
CreateInterfaceFn Sys_GetFactory(const char* name) {
    return reinterpret_cast<CreateInterfaceFn>(Sys_GetProcAddress(name, cse_platform::InterfaceFactorySymbol));
}



