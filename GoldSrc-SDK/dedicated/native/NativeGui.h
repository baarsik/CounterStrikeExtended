#pragma once
#include "DedicatedModuleSet.h"
#include <string>
class CMainPanel;
class IAdminServer;
class IVGuiModule;
namespace cse::dedicated {
// Main-thread owner of retained controls, admitted standalone interfaces and revocable admin bindings.
class NativeGui {
public:
    ~NativeGui();
    bool Open(DedicatedModuleSet& modules, std::string& error, std::uint64_t generation, const std::string& runtimeRoot);
    bool Pump();
    bool ReadyToStart() const;
    bool Started(std::string& error);
    const char* Commands() const;
    void Stop() noexcept;
    void Close() noexcept;
    void Print(char* text);

private:
    CMainPanel* panel_ = nullptr;
    IAdminServer* admin_ = nullptr;
    IVGuiModule* adminModule_ = nullptr;
    bool initialized_ = false;
    bool bound_ = false;
    unsigned int manageHandle_ = 0;
    std::string configPath_;
    bool configAttached_ = false;
    IFileSystem* filesystem_ = nullptr;
};
} // namespace cse::dedicated
