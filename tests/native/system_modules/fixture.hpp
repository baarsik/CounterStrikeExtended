#pragma once
#include "common/native/BaseSystemModule.h"
#include "common/native/IEngineWrapper.h"
#include "common/native/IDemoPlayer.h"
#include "common/native/SystemModuleRegistry.h"
#include "HLTV/common/IServer.h"
#include "HLTV/common/IWorld.h"
#include "HLTV/common/INetwork.h"
#include "HLTV/common/IDirector.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <stdexcept>
#include <string>
#include <set>
#if defined(_WIN32)
#include <windows.h>
constexpr int RTLD_NOW = 0;
constexpr int RTLD_LOCAL = 0;
inline void* dlopen(const char* path, int) { return LoadLibraryA(path); }
inline void* dlsym(void* handle, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
}
inline int dlclose(void* handle) { return FreeLibrary(static_cast<HMODULE>(handle)) ? 0 : -1; }
inline const char* dlerror() { return "LoadLibrary fixture failed"; }
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <dlfcn.h>
#endif

inline void Require(bool value, const char* what) {
    if (!value) { throw std::runtime_error(what); }
}
extern "C" void* Mem_ZeroMalloc(std::size_t size);
inline void Mem_Free(void* data) { std::free(data); }
inline void CopyLimited(char* out, const char* in, int size) { std::strncpy(out, in, size); }
inline int Q_strcasecmp(const char* a, const char* b) { return strcasecmp(a, b); }
inline std::set<std::string> engineCommands;
inline void Cmd_RemoveWrapperCmds() { engineCommands.clear(); }
inline void SystemWrapperCommandForwarder() {}
inline void Cmd_AddWrapperCommand(char* name, void (*)()) { engineCommands.insert(name); }
void DemoPlayer_Reset();
int DemoPlayer_Init();
int DemoPlayer_IsActive();
void DemoPlayer_SetActive(int);
inline int COM_CheckParm(const char*) { return 0; }
inline void Con_DPrintf(const char*, ...) {}
constexpr int src_command = 0;
inline void Cmd_ExecuteString(char*, int) {}
inline int Q_strnicmp(const char* a, const char* b, int count) { return strncasecmp(a, b, count); }
inline void Sys_UnloadModule(void* handle) { dlclose(handle); }

// Engine operations are controlled services; interfaces, BaseSystemModule, loader and providers are real.
class EngineWrapper : public IEngineWrapper, public BaseSystemModule {
public:
    void* QueryInterface(const char* name) override {
        if (cse::modules::Is(name, cse::modules::EngineName)) { return static_cast<IEngineWrapper*>(this); }
        return BaseSystemModule::QueryInterface(name);
    }
    char* GetType() override { return const_cast<char*>(cse::modules::EngineName); }
    bool GetViewOrigin(float*) override { return true; }
    bool GetViewAngles(float*) override { return true; }
    int GetTraceEntity() override { return 42; }
    float GetCvarFloat(char*) override { return 0; }
    char* GetCvarString(char*) override { return const_cast<char*>(""); }
    void SetCvar(char*, char*) override {}
    void Cbuf_AddText(char*) override {}
    void DemoUpdateClientData(client_data_s*) override {}
    void CL_QueueEvent(int, int, float, event_args_s*) override {}
    void HudWeaponAnim(int, int) override {}
    void CL_DemoPlaySound(int, char*, float, float, int, int) override {}
    void ClientDLL_ReadDemoBuffer(int, unsigned char*) override {}
    bool ValidStuffText(const char*) override { return true; }
    void Cbuf_AddFilteredText(char*) override {}
};

class SystemWrapper : public IBaseSystem, public BaseSystemModule {
public:
    struct command_t { char name[32]; union { int commandID; int id; }; ISystemModule* module; };
    struct library_t {
        char name[4096];
        union { void* handle; void* module; };
        union { CreateInterfaceFn createInterfaceFn; CreateInterfaceFn factory; };
    };
    char m_BaseDir[1]{};
    ObjectList m_Modules;
    ObjectList m_Libraries;
    ObjectList m_Commands;
    cse::modules::Registry m_NativeModules;
    IEngineWrapper* m_EngineWrapper = nullptr;
    unsigned int m_Tick = 0;
    double m_LastTime = 0;
    std::string corePath;
    std::string demoPath;
    bool failEngine = false;
    bool failServer = false;
    bool failWorld = false;
    bool failCommand = false;
    bool failFileSystem = false;
    bool failWorldInit = false;
    void* QueryInterface(const char* name) override {
        if (cse::modules::Is(name, cse::modules::LifetimeName)) { return &m_NativeModules.lifetime; }
        if (cse::modules::Is(name, cse::modules::SystemName)) { return static_cast<IBaseSystem*>(this); }
        return BaseSystemModule::QueryInterface(name);
    }
    bool Init(IBaseSystem*, int, char*) override;
    void ShutDown() override;
    void RunFrame(double) override;
    bool DispatchCommand(char*);
    bool AddModule(ISystemModule*, char*) override;
    bool RemoveModule(ISystemModule*) override;
    ISystemModule* FindModule(char*, char*);
    ISystemModule* GetModule(char*, char*, char*) override;
    bool RegisterCommand(char*, ISystemModule*, int) override;
    void RemoveNativeCommands(ISystemModule*);
    library_t* GetLibrary(char* name) {
        for (auto* lib = static_cast<library_t*>(m_Libraries.GetFirst()); lib;
             lib = static_cast<library_t*>(m_Libraries.GetNext())) {
            if (!std::strcmp(lib->name, name)) { return lib; }
        }
        const char* path = std::strcmp(name, "core") == 0 ? corePath.c_str() : demoPath.c_str();
        void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) { throw std::runtime_error(dlerror()); }
        auto* lib = static_cast<library_t*>(std::calloc(1, sizeof(library_t)));
        std::strncpy(lib->name, name, sizeof(lib->name) - 1);
        lib->handle = handle;
        lib->createInterfaceFn = reinterpret_cast<CreateInterfaceFn>(dlsym(handle, "CreateInterface"));
        Require(lib->createInterfaceFn != nullptr, "missing factory export");
        m_Libraries.Add(lib);
        return lib;
    }
    double GetTime() override { return m_SystemTime; }
    unsigned int GetTick() override { return m_Tick; }
    void SetFPS(float) override {}
    std::string printed;
    void Printf(char* format, ...) override {
        char text[1024];
        va_list args;
        va_start(args, format);
        std::vsnprintf(text, sizeof(text), format, args);
        va_end(args);
        printed += text;
    }
    void DPrintf(char*, ...) override {}
    void RedirectOutput(char*, int) override {}
    IFileSystem* GetFileSystem() override {
        if (failFileSystem) { throw std::bad_alloc(); }
        return nullptr;
    }
    unsigned char* LoadFile(const char*, int*) override { return nullptr; }
    void FreeFile(unsigned char*) override {}
    void SetTitle(char*) override {}
    void SetStatusLine(char*) override {}
    void ShowConsole(bool) override {}
    void LogConsole(char*) override {}
    bool InitVGUI(IVGuiModule*) override { return false; }
#if defined(_WIN32)
    void* GetPanel() override { return nullptr; }
#endif
    void GetCommandMatches(char*, ObjectList*) override {}
    void ExecuteString(char*) override {}
    void ExecuteFile(char*) override {}
    void Errorf(char*, ...) override {}
    char* CheckParam(char* name) override {
        if (failWorldInit && !std::strcmp(name, "-cachesize")) { throw std::bad_alloc(); }
        return nullptr;
    }
    void Stop() override {}
    char* COM_GetBaseDir() override { return const_cast<char*>(""); }
};

extern SystemWrapper gSystemWrapper;

extern SystemWrapper* currentModuleSystem;
inline void* Sys_LoadModule(const char* name) {
    const char* path = std::strstr(name, "core") ? currentModuleSystem->corePath.c_str()
                                                : currentModuleSystem->demoPath.c_str();
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}
inline CreateInterfaceFn Sys_GetFactory(void* handle) {
    return reinterpret_cast<CreateInterfaceFn>(dlsym(handle, "CreateInterface"));
}
namespace hltv_posix {
constexpr std::size_t PathCapacity = 4096;
template<class Loader> void* LoadRuntimeModule(const char* name, const char*, Loader loader) {
    return loader(name);
}
}
#if !defined(_WIN32)
#define _stricmp strcasecmp
#define _snprintf snprintf
#endif
