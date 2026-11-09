#pragma once
#include "common/IBaseSystem.h"
#include <cstring>

class ControlledSystem : public IBaseSystem {
public:
    bool Init(IBaseSystem*, int, char*) override {
        return true;
    }
    void RunFrame(double) override {}
    void ReceiveSignal(ISystemModule*, unsigned int, void*) override {}
    void ExecuteCommand(int, char*) override {}
    void RegisterListener(ISystemModule*) override {}
    void RemoveListener(ISystemModule*) override {}
    IBaseSystem* GetSystem() override {
        return this;
    }
    int GetSerial() override {
        return 1;
    }
    char* GetStatusLine() override {
        return name;
    }
    char* GetType() override {
        return name;
    }
    char* GetName() override {
        return name;
    }
    int GetState() override {
        return 3;
    }
    int GetVersion() override {
        return 1;
    }
    void ShutDown() override {}
    double GetTime() override {
        return 12.5;
    }
    unsigned int GetTick() override {
        return 0;
    }
    void SetFPS(float) override {}
    void Printf(char*, ...) override {
        ++warnings;
    }
    void DPrintf(char*, ...) override {
        ++warnings;
    }
    void RedirectOutput(char*, int) override {}
    IFileSystem* GetFileSystem() override {
        return nullptr;
    }
    unsigned char* LoadFile(const char*, int*) override {
        return nullptr;
    }
    void FreeFile(unsigned char*) override {}
    void SetTitle(char*) override {}
    void SetStatusLine(char*) override {}
    void ShowConsole(bool) override {}
    void LogConsole(char*) override {}
    bool InitVGUI(IVGuiModule*) override {
        return false;
    }
#ifdef _WIN32
    vgui::Panel* GetPanel() override {
        return nullptr;
    }
#endif
    bool RegisterCommand(char*, ISystemModule*, int) override {
        return true;
    }
    void GetCommandMatches(char*, ObjectList*) override {}
    void ExecuteString(char*) override {}
    void ExecuteFile(char*) override {}
    void Errorf(char*, ...) override {
        ++errors;
    }
    char* CheckParam(char* parameter) override {
        return std::strcmp(parameter, "-multicastttl") == 0 ? ttl : nullptr;
    }
    bool AddModule(ISystemModule*, char*) override {
        return false;
    }
    ISystemModule* GetModule(char*, char*, char*) override {
        return nullptr;
    }
    bool RemoveModule(ISystemModule*) override {
        return false;
    }
    void Stop() override {}
    char* COM_GetBaseDir() override {
        return name;
    }

    char name[8] = "fixture";
    char* ttl = nullptr;
    int warnings = 0;
    int errors = 0;
};
