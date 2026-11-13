#pragma once
#include "../../public/FileSystem.h"
#include "../../public/dedicated_api.h"
#include "../../public/game_server_data.h"
#include "../../public/native_dedicated_lifetime.h"
#include "../../launcher/native/SoftwareImageOwner.h"
#include "../../launcher/native/SocketImageOwner.h"
#include <string>
namespace cse::dedicated {
class DedicatedModuleSet {
public:
    DedicatedModuleSet() = default;
    ~DedicatedModuleSet() noexcept;
    DedicatedModuleSet(const DedicatedModuleSet&) = delete;
    DedicatedModuleSet& operator=(const DedicatedModuleSet&) = delete;
    bool Open(const char* runtimeRoot, bool gui, std::string& error);
    void Close() noexcept;
    bool Initialize(char* base, char* command, CreateInterfaceFn host);
    bool Frame();
    void AddText(char* text);
    void Status(float* fps, int* active, int* maximum, char* map);
    int Shutdown() noexcept;
    bool Retire() noexcept;
    IDedicatedServerAPI* Engine() const noexcept {
        return engine_;
    }
    IFileSystem* Filesystem() const noexcept {
        return filesystem_;
    }
    IGameServerData* Admin() const noexcept {
        return admin_;
    }
    CreateInterfaceFn EngineFactory() const noexcept {
        return engineFactory_;
    }
    CreateInterfaceFn FilesystemFactory() const noexcept {
        return filesystemFactory_;
    }
    CreateInterfaceFn VguiFactory() const noexcept {
        return vguiFactory_;
    }
    bool IsClosed() const noexcept {
        return !engineModule_ && !filesystemModule_ && !vguiModule_ && !fontModule_ && !software_.Active() && !sockets_.Active();
    }

private:
    software_lifetime::ImageOwner software_;
    socket_lifetime::SocketImageOwner sockets_;
    void* fontModule_ = nullptr;
    void* engineModule_ = nullptr;
    void* filesystemModule_ = nullptr;
    void* vguiModule_ = nullptr;
    IDedicatedServerAPI* engine_ = nullptr;
    IFileSystem* filesystem_ = nullptr;
    IGameServerData* admin_ = nullptr;
    CreateInterfaceFn engineFactory_ = nullptr;
    CreateInterfaceFn filesystemFactory_ = nullptr;
    CreateInterfaceFn vguiFactory_ = nullptr;
    CseNativeDedicatedRetireFn retire_ = nullptr;
    bool shutdown_ = false;
    bool retired_ = false;
    int shutdownResult_ = 0;
};
} // namespace cse::dedicated
