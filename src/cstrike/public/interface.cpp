//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Main class factory methods
//=============================================================================

#include "interface.h"
#include "../../public/native_module.h"
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

// ------------------------------------------------------------------------------------ //
// InterfaceReg.
// ------------------------------------------------------------------------------------ //
InterfaceReg* InterfaceReg::s_pInterfaceRegs = NULL;

extern "C" void* CreateInterface( const char* name, int* returnCode )
{
	return InterfaceReg::Create( name, returnCode );
}

void* GetModuleHandle(const char* name) {
    return cse_platform::FindLoadedModule(name);
}

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

CreateInterfaceFn Sys_GetFactory(CSysModule* module) {
    return reinterpret_cast<CreateInterfaceFn>(cse_platform::GetSymbol(module, cse_platform::InterfaceFactorySymbol));
}

CreateInterfaceFn Sys_GetFactory(const char* name) {
    void* module = cse_platform::FindLoadedModule(name);
    return reinterpret_cast<CreateInterfaceFn>(Sys_GetProcAddress(module, cse_platform::InterfaceFactorySymbol));
}

CreateInterfaceFn Sys_GetFactoryThis()
{
	return CreateInterface;
}
