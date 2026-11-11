#include "BaseSystemModule.h"
#include "../../public/native_system_module_abi.h"
#include "IBaseSystem.h"
#include <algorithm>
#include <cstring>
#include <new>

int COM_BuildNumber();

BaseSystemModule::BaseSystemModule() = default;
BaseSystemModule::~BaseSystemModule() = default;

char *BaseSystemModule::GetName() {
    return m_Name;
}
char *BaseSystemModule::GetType() {
    return const_cast<char *>("GenericModule");
}
char *BaseSystemModule::GetStatusLine() {
    return const_cast<char *>("No status available.\n");
}
int BaseSystemModule::GetVersion() {
    return COM_BuildNumber();
}
int BaseSystemModule::GetSerial() {
    return static_cast<int>(m_Serial);
}
int BaseSystemModule::GetState() {
    return static_cast<int>(m_State);
}
IBaseSystem *BaseSystemModule::GetSystem() {
    return m_System;
}

bool BaseSystemModule::Init(IBaseSystem *system, int serial, char *name) {
    if (!system) {
        return false;
    }
    m_NativeShuttingDown = false;
    m_System = system;
    m_Serial = static_cast<unsigned int>(serial);
    m_State = MODULE_INITIALIZING;
    m_SystemTime = 0;
    constexpr std::size_t MaxNameLength = sizeof(m_Name) - 1;
    std::size_t nameLength = 0;
    if (name) {
        while (nameLength < MaxNameLength && name[nameLength] != '\0') {
            m_Name[nameLength] = name[nameLength];
            ++nameLength;
        }
    }
    std::fill(m_Name + nameLength, m_Name + sizeof(m_Name), '\0');
    m_Listener.clear();
    return true;
}

void BaseSystemModule::RunFrame(double time) {
    m_SystemTime = time;
}

void BaseSystemModule::ShutDown() {
    if (m_State == MODULE_DISCONNECTED) {
        return;
    }
    m_State = MODULE_DISCONNECTED;
    m_Listener.clear();
    if (m_System) {
        m_System->RemoveModule(this);
    }
}

void *BaseSystemModule::QueryInterface(const char *name) {
    if (cse::modules::Is(name, cse::modules::ModuleName)) {
        return static_cast<ISystemModule *>(this);
    }
    if (cse::modules::Is(name, cse::modules::SubscriptionName)) {
        return &m_Subscription;
    }
    return nullptr;
}

bool BaseSystemModule::TryRegisterListener(ISystemModule *module) {
    if (!module || m_State == MODULE_DISCONNECTED || m_NativeShuttingDown) {
        return false;
    }
    return m_Listener.Add(module);
}

void BaseSystemModule::RegisterListener(ISystemModule *module) {
    TryRegisterListener(module);
}

void BaseSystemModule::RemoveListener(ISystemModule *module) {
    m_Listener.Remove(module);
}

void BaseSystemModule::FireSignal(unsigned int signal, void *data) {
    cse::modules::CallbackScope callback(m_System);
    const auto listeners = m_Listener;
    for (ISystemModule *listener : listeners) {
        if (m_Listener.Contains(listener)) {
            listener->ReceiveSignal(this, signal, data);
        }
    }
}

void BaseSystemModule::ExecuteCommand(int, char *) {
    if (m_System) {
        m_System->DPrintf(const_cast<char *>("WARNING! Undeclared ExecuteCommand().\n"));
    }
}

void BaseSystemModule::ReceiveSignal(ISystemModule *module, unsigned int signal, void *) {
    if (m_System && module) {
        m_System->DPrintf(const_cast<char *>("WARNING! Unhandled signal (%i) from module %s.\n"), signal,
                          module->GetName());
    }
}
