#pragma once
#include "DedicatedModuleSet.h"
#include <string>
class CMainPanel;
class IAdminServer;
class IVGuiModule;
namespace cse::dedicated {
// Main-thread owner of the retained dedicated controls. Task2 supplies the
// admitted standalone factory; Task3 supplies revocable admin bindings.
class NativeGui {
public:
    ~NativeGui();
    bool Open(DedicatedModuleSet& modules, std::string& error);
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
};
} // namespace cse::dedicated
