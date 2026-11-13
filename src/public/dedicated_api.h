#pragma once
#include "interface_registry.h"
class IDedicatedServerAPI : public IBaseInterface {
public:
    virtual bool Init(char* basedir, char* command, CreateInterfaceFn launcherFactory,
                      CreateInterfaceFn filesystemFactory) = 0;
    virtual int Shutdown() = 0;
    virtual bool RunFrame() = 0;
    virtual void AddConsoleText(char* text) = 0;
    virtual void UpdateStatus(float* fps, int* activePlayers, int* maxPlayers, char* map) = 0;
};
