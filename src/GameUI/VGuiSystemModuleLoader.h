//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Handles loading/unloading of different vgui modules into a shared context
//
//=============================================================================

#pragma once

#include "common/IVGuiModuleLoader.h"
#include "utlvector.h"

class IVGuiModule;
class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: Handles loading/unloading of different vgui modules into a shared context
//-----------------------------------------------------------------------------
class CVGuiSystemModuleLoader : public IVGuiModuleLoader
{
public:
	CVGuiSystemModuleLoader();
	~CVGuiSystemModuleLoader() override;

	// returns number of modules loaded
	int GetModuleCount() override;
	const char* GetModuleLabel( int moduleIndex ) override;
	// returns a modules interface factory
	CreateInterfaceFn GetModuleFactory( int moduleIndex ) override;
	// brings the specified module to the foreground
	bool ActivateModule( int moduleIndex ) override;
	bool ActivateModule( const char* moduleName ) override;
	void SetPlatformToRestart() override;

	bool LoadPlatformModules( CreateInterfaceFn* factories, int count, bool useSteamModules );
	bool InitializeAllModules( CreateInterfaceFn* factories, int count );
	void ShutdownPlatformModules();
    bool TryRetireNativeModules();
	void DeactivatePlatformModules();
	// Reenables all the deactivated platform modules
	void ReactivatePlatformModules();
	void UnloadPlatformModules();
	void RunFrame();
	bool IsModuleHidden( int moduleIndex );
	void PostMessageToAllModules( KeyValues* message );
	bool IsPlatformReady();
	bool ShouldPlatformRestart();

private:
    enum class ModuleOwnership { ExternalImage, InternalGameUI };
    enum class ModulePhase { Loaded, Initialized, PostInitialized, Attached, Retiring, Retired, Shutdown };
    struct module_t {
        CSysModule* module = nullptr;
        IVGuiModule* moduleInterface = nullptr;
        KeyValues* data = nullptr;
        ModuleOwnership ownership = ModuleOwnership::ExternalImage;
        CreateInterfaceFn factory = nullptr;
        ModulePhase phase = ModulePhase::Loaded;
        bool (*tryRetire)() = nullptr;
        bool (*isRetired)() = nullptr;
        bool initializeEntered = false;
        bool initializeSucceeded = false;
        bool postInitializeEntered = false;
    };

    size_t m_nativeCallDepth = 0;
	bool					m_bModulesInitialized;
	bool					m_bPlatformShouldRestartAfterExit;
	CUtlVector<module_t>	m_Modules;
	KeyValues*				m_pPlatformModuleData;
};

extern CVGuiSystemModuleLoader g_VModuleLoader;
