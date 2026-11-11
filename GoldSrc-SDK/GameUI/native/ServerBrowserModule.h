#pragma once

#include "../../public/interface_registry.h"
#include "../../public/native_gameui_retirement.h"
#include <cstdint>

namespace cse::server_browser {
constexpr char ModuleInterface[] = "VGuiModuleServerBrowser001";
CreateInterfaceFn ServerBrowserFactory();
bool BindServerBrowserHost(CreateInterfaceFn *factories, int count);
bool TryRetireServerBrowser();
bool IsServerBrowserRetired();
bool TryRetireGameUIPanels();
bool AreGameUIPanelsRetired();
void ServerBrowserAcceptedConnection(const char *game, uint32_t rawIP, uint16_t connectionPort);
void ServerBrowserDisconnected();
}
