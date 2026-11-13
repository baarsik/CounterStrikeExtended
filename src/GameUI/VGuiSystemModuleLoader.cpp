//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "VGuiSystemModuleLoader.h"
#include "GameUI_Interface.h"
#include "Taskbar.h"
#include "native/ServerBrowserModule.h"
#include <vector>

#include "common/IVGuiModule.h"
#include "filesystem.h"
#include "vgui/ILocalize.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#include <strings.h>
#define stricmp strcasecmp
#endif
#include <string.h>

#if INTPTR_MAX > INT32_MAX
namespace {
class NativeModuleCallScope final {
public:
    explicit NativeModuleCallScope(size_t& depth) : depth_(depth) {
        ++depth_;
    }
    ~NativeModuleCallScope() {
        --depth_;
    }
private:
    size_t& depth_;
};
}
#endif

CVGuiSystemModuleLoader g_VModuleLoader;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVGuiSystemModuleLoader, IVGuiModuleLoader,
	"VGuiModuleLoader003", g_VModuleLoader );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVGuiSystemModuleLoader::CVGuiSystemModuleLoader()
{
	m_bModulesInitialized = false;
	m_bPlatformShouldRestartAfterExit = false;
	m_pPlatformModuleData = NULL;
}

CVGuiSystemModuleLoader::~CVGuiSystemModuleLoader()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns number of modules loaded
//-----------------------------------------------------------------------------
int CVGuiSystemModuleLoader::GetModuleCount()
{
	return m_Modules.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns the string menu name (unlocalized) of a module
//			moduleIndex is of the range [0, GetModuleCount())
//-----------------------------------------------------------------------------
const char* CVGuiSystemModuleLoader::GetModuleLabel( int moduleIndex )
{
	return m_Modules[moduleIndex].data->GetString( "MenuName", "< unknown >" );
}

//-----------------------------------------------------------------------------
// Purpose: returns a modules interface factory
//-----------------------------------------------------------------------------
CreateInterfaceFn CVGuiSystemModuleLoader::GetModuleFactory( int moduleIndex )
{
#if INTPTR_MAX > INT32_MAX
    return m_Modules.IsValidIndex(moduleIndex) ? m_Modules[moduleIndex].factory : nullptr;
#else
    return Sys_GetFactory(m_Modules[moduleIndex].module);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: brings the specified module to the foreground
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ActivateModule( int moduleIndex )
{
	if ( !m_Modules.IsValidIndex( moduleIndex ) )
		return false;

#if INTPTR_MAX > INT32_MAX
    if (m_nativeCallDepth || m_Modules[moduleIndex].phase != ModulePhase::Attached) {
        return false;
    }
    NativeModuleCallScope nativeCall(m_nativeCallDepth);
#endif
    m_Modules[moduleIndex].moduleInterface->Activate();
	if ( g_pTaskbar )
	{
		wchar_t titleBuffer[1024];
		wchar_t* title = vgui2::localize()->Find( m_Modules[moduleIndex].data->GetName() );
		if ( !title )
		{
			vgui2::localize()->ConvertANSIToUnicode( m_Modules[moduleIndex].data->GetName(),
				titleBuffer, sizeof( titleBuffer ) );
			title = titleBuffer;
		}

		g_pTaskbar->SetTitle( m_Modules[moduleIndex].moduleInterface->GetPanel(), title );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: activates a module by name
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ActivateModule( const char* moduleName )
{
	for ( int i = 0; i < GetModuleCount(); i++ )
	{
		if ( !stricmp( GetModuleLabel( i ), moduleName ) ||
			!stricmp( m_Modules[i].data->GetName(), moduleName ) )
		{
#if INTPTR_MAX > INT32_MAX
            return ActivateModule(i);
#else
			ActivateModule( i );
			return true;
#endif
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: sets the the platform should update and restart when it quits
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::SetPlatformToRestart()
{
	m_bPlatformShouldRestartAfterExit = true;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the module loader has acquired the platform mutex and loaded the modules
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::IsPlatformReady()
{
	return m_bModulesInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ShouldPlatformRestart()
{
	return m_bPlatformShouldRestartAfterExit;
}

bool CVGuiSystemModuleLoader::InitializeAllModules( CreateInterfaceFn* factories, int count )
{
#if INTPTR_MAX > INT32_MAX
    if (m_nativeCallDepth) {
        return false;
    }
    NativeModuleCallScope nativeCall(m_nativeCallDepth);
    bool success = true;
    std::vector<CreateInterfaceFn> moduleFactories;
    for (int index = 0; index < count; ++index) {
        moduleFactories.push_back(factories[index]);
    }
    for (int index = 0; index < m_Modules.Count(); ++index) {
        auto& record = m_Modules[index];
        moduleFactories.push_back(record.factory);
        if (record.initializeEntered) {
            success = success && record.phase == ModulePhase::Attached;
            continue;
        }
        record.initializeEntered = true;
        if (!record.moduleInterface->Initialize(factories, count)) {
            success = false;
            continue;
        }
        record.initializeSucceeded = true;
        record.phase = ModulePhase::Initialized;
    }
    for (int index = 0; index < m_Modules.Count(); ++index) {
        auto& record = m_Modules[index];
        if (record.phase != ModulePhase::Initialized || record.postInitializeEntered) {
            continue;
        }
        record.postInitializeEntered = true;
        if (!record.moduleInterface->PostInitialize(moduleFactories.data(),
            static_cast<int>(moduleFactories.size()))) {
            success = false;
            continue;
        }
        record.phase = ModulePhase::PostInitialized;
        if (!g_pTaskbar) {
            success = false;
            continue;
        }
        record.moduleInterface->SetParent(g_pTaskbar->GetVPanel());
        g_pTaskbar->AddTask(record.moduleInterface->GetPanel());
        record.phase = ModulePhase::Attached;
    }
    m_bModulesInitialized = success;
    return success;
#else
	bool success = true;
	// Init vgui in the modules
	for ( int i = 0; i < m_Modules.Count(); i++ )
	{
		if ( !m_Modules[i].moduleInterface->Initialize( factories, count ) )
		{
			vgui2::ivgui()->DPrintf2( "Platform Error: module failed to initialize\n" );
			success = false;
		}
	}

#ifdef _MSC_VER
#pragma warning( suppress: 6255 )
#endif
	CreateInterfaceFn* moduleFactories = (CreateInterfaceFn*)alloca(
		( count + m_Modules.Count() ) * sizeof( CreateInterfaceFn ) );
	for ( int i = 0; i < count; i++ )
		moduleFactories[i] = factories[i];
	for ( int i = 0; i < m_Modules.Count(); i++ )
		moduleFactories[count + i] = Sys_GetFactory( m_Modules[i].module );

	// give the modules a chance to link themselves together
	for ( int i = 0; i < m_Modules.Count(); i++ )
	{
		if ( !m_Modules[i].moduleInterface->PostInitialize( moduleFactories, count + m_Modules.Count() ) )
		{
			vgui2::ivgui()->DPrintf2( "Platform Error: module failed to initialize\n" );
			success = false;
		}

		m_Modules[i].moduleInterface->SetParent( g_pTaskbar->GetVPanel() );
		g_pTaskbar->AddTask( m_Modules[i].moduleInterface->GetPanel() );
	}

	m_bModulesInitialized = true;
	return success;
#endif
}

bool CVGuiSystemModuleLoader::LoadPlatformModules( CreateInterfaceFn* factories, int count, bool useSteamModules ) {
#if INTPTR_MAX > INT32_MAX
    if (m_nativeCallDepth || m_pPlatformModuleData) {
        return !m_nativeCallDepth && m_bModulesInitialized;
    }
#endif
    // load platform menu
    KeyValues* platformData = new KeyValues( "Platform" );
    if ( !platformData->LoadFromFile( vgui2::filesystem(), "steam/games/PlatformMenu.vdf", "PLATFORM" ) ) {
        platformData->deleteThis();
        return false;
    }

    bool success = true;
    // walk the platform menu loading all the interfaces
    KeyValues* menu = platformData->FindKey( "Menu", true );
    {
#if INTPTR_MAX > INT32_MAX
        NativeModuleCallScope nativeCall(m_nativeCallDepth);
#endif
        for ( KeyValues* data = menu->GetFirstSubKey(); data; data = data->GetNextKey() ) {
            // see if we should skip steam modules
            if (!useSteamModules && data->GetInt("SteamApp", 0)) {
                continue;
            }

            const char* interfaceName = data->GetString( "interface" );
            if (!stricmp(interfaceName, "VGuiModuleTracker001")) {
                continue;
            }

#if INTPTR_MAX > INT32_MAX
            if (std::strcmp(data->GetName(), "Servers") == 0 &&
                std::strcmp(interfaceName, cse::server_browser::ModuleInterface) == 0) {
                const auto factory = cse::server_browser::ServerBrowserFactory();
                auto* moduleInterface = factory ? static_cast<IVGuiModule*>(factory(interfaceName, nullptr)) : nullptr;
                if (!moduleInterface) {
                    success = false;
                    continue;
                }
                const int index = m_Modules.AddToTail();
                auto& record = m_Modules[index];
                record.module = nullptr;
                record.ownership = ModuleOwnership::InternalGameUI;
                record.factory = factory;
                record.moduleInterface = moduleInterface;
                record.data = data;
                record.tryRetire = cse::server_browser::TryRetireServerBrowser;
                record.isRetired = cse::server_browser::IsServerBrowserRetired;
                continue;
            }
#endif
            const char* moduleName = data->GetString( "dll" );
#ifndef _WIN32
            char dllPath[512];
            strncpy( dllPath, moduleName, sizeof( dllPath ) );
            char* extension = strrchr( dllPath, '.' );
            if (extension) {
                *extension = 0;
            }
            strcat( dllPath, "_linux.so" );
            moduleName = dllPath;
#endif

            // load the dll
            char localPath[512];
            if ( !vgui2::filesystem()->GetLocalPath( moduleName, localPath, sizeof( localPath ) ) ) {
                vgui2::ivgui()->DPrintf2( "Platform Error: couldn't find %s, not loading\n", data->GetString( "dll" ) );
                success = false;
                continue;
            }

            vgui2::filesystem()->GetLocalCopy( localPath );
            CSysModule* module = Sys_LoadModule( localPath );
            if ( !module ) {
                vgui2::ivgui()->DPrintf2( "Platform Error: bad module '%s', not loading\n", data->GetString( "dll" ) );
                success = false;
                continue;
            }

            CreateInterfaceFn factory = Sys_GetFactory( module );
            IVGuiModule* moduleInterface = factory ? static_cast<IVGuiModule*>(factory(interfaceName, nullptr)) : nullptr;
            if ( !moduleInterface ) {
                vgui2::ivgui()->DPrintf2( "Platform Error: module version ('%s, %s) invalid, not loading\n",
                    data->GetString( "dll" ), data->GetString( "interface" ) );
                success = false;
#if INTPTR_MAX > INT32_MAX
                Sys_UnloadModule(module);
#endif
                continue;
            }

            // store off the module
            int index = m_Modules.AddToTail();
            m_Modules[index].module = module;
            m_Modules[index].factory = factory;
            m_Modules[index].moduleInterface = moduleInterface;
            m_Modules[index].data = data;
        }
    }

    m_pPlatformModuleData = platformData;
    bool initialized = InitializeAllModules( factories, count );
    return success && initialized;
}

//-----------------------------------------------------------------------------
// Purpose: gives all platform modules a chance to Shutdown gracefully
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::TryRetireNativeModules() {
#if INTPTR_MAX > INT32_MAX
    if (m_nativeCallDepth) {
        return false;
    }
    NativeModuleCallScope nativeCall(m_nativeCallDepth);
    for (int index = 0; index < m_Modules.Count(); ++index) {
        auto& record = m_Modules[index];
        if (record.ownership != ModuleOwnership::InternalGameUI || record.phase == ModulePhase::Retired ||
            record.phase == ModulePhase::Shutdown) {
            continue;
        }
        record.phase = ModulePhase::Retiring;
        if (!record.tryRetire || !record.isRetired || !record.tryRetire() || !record.isRetired()) {
            return false;
        }
        record.phase = ModulePhase::Retired;
    }
#endif
    return true;
}

void CVGuiSystemModuleLoader::ShutdownPlatformModules()
{
#if INTPTR_MAX > INT32_MAX
    if (!TryRetireNativeModules()) {
        return;
    }
    NativeModuleCallScope nativeCall(m_nativeCallDepth);
    for (int index = 0; index < m_Modules.Count(); ++index) {
        auto& record = m_Modules[index];
        if (record.phase == ModulePhase::Shutdown) {
            continue;
        }
        // Publish first so a synchronous module shutdown callback cannot run this phase twice.
        record.phase = ModulePhase::Shutdown;
        if (!record.initializeEntered) {
            continue;
        }
        if (record.initializeSucceeded) {
            record.moduleInterface->Deactivate();
        }
        record.moduleInterface->Shutdown();
    }
    m_bModulesInitialized = false;
    return;
#else
	// static include guard to prevent recursive calls
	static bool runningFunction = false;
	if ( runningFunction )
		return;

	runningFunction = true;
	for ( int i = 0; i < m_Modules.Count(); i++ )
		m_Modules[i].moduleInterface->Deactivate();

	// give all the modules notice of quit
	for ( int i = 0; i < m_Modules.Count(); i++ )
	{
		vgui2::ivgui()->PostMessage( m_Modules[i].moduleInterface->GetPanel(),
			new KeyValues( "Command", "command", "Quit" ), 0 );
	}

	for ( int i = 0; i < m_Modules.Count(); i++ )
		m_Modules[i].moduleInterface->Shutdown();
	runningFunction = false;
#endif
}

void CVGuiSystemModuleLoader::DeactivatePlatformModules() {
#if INTPTR_MAX > INT32_MAX
    if (m_nativeCallDepth) {
        return;
    }
    NativeModuleCallScope nativeCall(m_nativeCallDepth);
#endif
    for (int index = 0; index < m_Modules.Count(); ++index) {
#if INTPTR_MAX > INT32_MAX
        if (m_Modules[index].phase != ModulePhase::Attached) {
            continue;
        }
#endif
        m_Modules[index].moduleInterface->Deactivate();
    }
}

//-----------------------------------------------------------------------------
// Purpose: Reenables all the deactivated platform modules
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::ReactivatePlatformModules() {
#if INTPTR_MAX > INT32_MAX
    if (m_nativeCallDepth) {
        return;
    }
    NativeModuleCallScope nativeCall(m_nativeCallDepth);
#endif
    for (int index = 0; index < m_Modules.Count(); ++index) {
#if INTPTR_MAX > INT32_MAX
        if (m_Modules[index].phase != ModulePhase::Attached) {
            continue;
        }
#endif
        m_Modules[index].moduleInterface->Reactivate();
    }
}

//-----------------------------------------------------------------------------
// Purpose: Disables and unloads platform
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::UnloadPlatformModules()
{
#if INTPTR_MAX > INT32_MAX
    if (!TryRetireNativeModules()) {
        return;
    }
    NativeModuleCallScope nativeCall(m_nativeCallDepth);
#endif
    for (int index = 0; index < m_Modules.Count(); ++index) {
        if (m_Modules[index].ownership == ModuleOwnership::ExternalImage && m_Modules[index].module) {
            Sys_UnloadModule(m_Modules[index].module);
        }
    }
	m_Modules.RemoveAll();

	if ( m_pPlatformModuleData )
	{
		m_pPlatformModuleData->deleteThis();
		m_pPlatformModuleData = NULL;
	}
}

void CVGuiSystemModuleLoader::RunFrame()
{
#if INTPTR_MAX > INT32_MAX
    if (m_nativeCallDepth) {
        return;
    }
    NativeModuleCallScope nativeCall(m_nativeCallDepth);
#endif
	for ( int i = 0; i < m_Modules.Count(); i++ )
    {
#if INTPTR_MAX > INT32_MAX
        if (m_Modules[i].phase != ModulePhase::Attached) {
            continue;
        }
#endif
        m_Modules[i].moduleInterface->RunFrame();
    }
}

//-----------------------------------------------------------------------------
// Purpose: brings the specified module to the foreground
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::IsModuleHidden( int moduleIndex )
{
	return m_Modules[moduleIndex].data->GetInt( "Hidden", 0 ) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::PostMessageToAllModules( KeyValues* message )
{
#if INTPTR_MAX > INT32_MAX
    if (m_nativeCallDepth) {
        message->deleteThis();
        return;
    }
    NativeModuleCallScope nativeCall(m_nativeCallDepth);
#endif
	for ( int i = 0; i < m_Modules.Count(); i++ )
	{
#if INTPTR_MAX > INT32_MAX
        if (m_Modules[i].phase != ModulePhase::Attached) {
            continue;
        }
#endif
		vgui2::ivgui()->PostMessage( m_Modules[i].moduleInterface->GetPanel(), message->MakeCopy(), 0 );
	}
	message->deleteThis();
}
