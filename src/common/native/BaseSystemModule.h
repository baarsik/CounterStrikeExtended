#pragma once

#include "../../public/native_system_module_abi.h"
#include "../ObjectList.h"
#include "BorrowedListeners.h"
#include "ISystemModule.h"

class IBaseSystem;

class BaseSystemModule : public virtual ISystemModule {
public:
    BaseSystemModule();
    ~BaseSystemModule() override;

    bool Init(IBaseSystem *system, int serial, char *name) override;
    void RunFrame(double time) override;
    void ReceiveSignal(ISystemModule *module, unsigned int signal, void *data) override;
    void ExecuteCommand(int commandID, char *commandLine) override;
    void RegisterListener(ISystemModule *module) override;
    void RemoveListener(ISystemModule *module) override;

    IBaseSystem *GetSystem() override;
    int GetSerial() override;
    char *GetStatusLine() override;
    char *GetType() override;
    char *GetName() override;
    int GetState() override;
    int GetVersion() override;

    void ShutDown() override;
    void *QueryInterface(const char *name) override;
    virtual char *COM_GetBaseDir() {
        return const_cast<char *>("");
    }

protected:
    bool m_NativeShuttingDown = false;
    enum : unsigned int { MODULE_UNDEFINED = 0, MODULE_INITIALIZING = 1, MODULE_RUNNING = 3, MODULE_DISCONNECTED = 4 };

    void FireSignal(unsigned int signal, void *data);

    IBaseSystem *m_System = nullptr;
    BorrowedListeners m_Listener;
    bool TryRegisterListener(ISystemModule *module);
    cse::modules::Subscription m_Subscription{this, [](void *context, ISystemModule *listener) {
                                                  return static_cast<BaseSystemModule *>(context)->TryRegisterListener(
                                                      listener);
                                              }};
    char m_Name[255]{};
    unsigned int m_State = MODULE_UNDEFINED;
    unsigned int m_Serial = 0;
    double m_SystemTime = 0;
};
