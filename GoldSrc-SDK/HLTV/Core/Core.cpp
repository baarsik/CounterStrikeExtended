#include "HLTV/Core/Server.h"
#include "public/tier1/interface.h"

IServer* CreateServer()
{
	return new Server();
}

static InterfaceReg __g_CreateIServer_reg( (InstantiateInterfaceFn)CreateServer, "server001" );

#if CSE_NATIVE_SYSTEM_MODULE
#include "HLTV/Core/World.h"
#include "HLTV/Core/Network.h"
#include <new>

static ISystemModule* CSE_ABI_CALL CreateNativeModule(const char* name) {
    if (cse::modules::Is(name, cse::modules::ServerName)) {
        return static_cast<ISystemModule*>(new (std::nothrow) Server());
    }
    if (cse::modules::Is(name, cse::modules::WorldName)) {
        return static_cast<ISystemModule*>(new (std::nothrow) World());
    }
    if (cse::modules::Is(name, cse::modules::NetworkName)) {
        return static_cast<ISystemModule*>(new (std::nothrow) Network());
    }
    return nullptr;
}

static void CSE_ABI_CALL ReleaseNativeModule(ISystemModule* module) {
    delete module;
}

static void* NativeFactory() {
    static cse::modules::Factory factory{cse::modules::Contract, sizeof(cse::modules::Factory),
        cse::modules::ContractRevision, CreateNativeModule, ReleaseNativeModule};
    return &factory;
}

static InterfaceReg nativeFactoryRegistration(NativeFactory, cse::modules::FactoryName);
#endif
