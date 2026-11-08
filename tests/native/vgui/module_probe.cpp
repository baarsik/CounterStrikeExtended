#include <cstdio>
#include <windows.h>
#undef GetClassName
#undef SendMessage
#include "vgui/IVGui.h"
#include "vgui/IPanel.h"
#include "client_spy.h"

// Explicit invocation against owned rebuilt modules. No renderer, browser or game is started.
int main(int argc, char **argv) {
    if (argc != 3) {
        return 2;
    }
    const auto filesystem = LoadLibraryA(argv[2]);
    const auto module = LoadLibraryA(argv[1]);
    if (!filesystem || !module) {
        std::fprintf(stderr, "Cannot load owned VGUI/filesystem modules: %lu\n", GetLastError());
        return 1;
    }
    const auto createVgui = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(module, "CreateInterface"));
    const auto createFilesystem = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(filesystem, "CreateInterface"));
    if (!createVgui || !createFilesystem) {
        return 1;
    }
    auto *gui = static_cast<vgui2::IVGui *>(createVgui("VGUI_ivgui006", nullptr));
    auto *panels = static_cast<vgui2::IPanel *>(createVgui("VGUI_Panel007", nullptr));
    CreateInterfaceFn factories[] = {createVgui, createFilesystem};
    constexpr int FactoryCount = sizeof(factories) / sizeof(factories[0]);
    if (!gui || !panels || !gui->Init(factories, FactoryCount)) {
        std::fprintf(stderr, "Owned VGUI factory initialization failed\n");
        return 1;
    }
    constexpr unsigned int Cycles = 24;
    for (unsigned int cycle = 0; cycle < Cycles; ++cycle) {
        ClientSpy parentClient;
        ClientSpy childClient;
        const auto parent = gui->AllocPanel();
        const auto child = gui->AllocPanel();
        panels->Init(parent, &parentClient);
        panels->Init(child, &childClient);
        panels->SetParent(child, parent);
        const auto handle = gui->PanelToHandle(child);
        if (!parent || !child || parentClient.childAdded != child || panels->GetParent(child) != parent
            || panels->GetChildCount(parent) != 1 || panels->GetChild(parent, 0) != child
            || gui->HandleToPanel(handle) != child) {
            std::fprintf(stderr, "Owned VGUI parenting/registry failed at cycle %u\n", cycle);
            return 1;
        }
        panels->SendMessage(parent, nullptr, child);
        if (parentClient.messageFrom != child) {
            return 1;
        }
        panels->SetParent(child, 0);
        gui->FreePanel(child);
        if (gui->HandleToPanel(handle) != 0 || panels->GetChildCount(parent) != 0) {
            return 1;
        }
        const auto replacement = gui->AllocPanel();
        panels->Init(replacement, &childClient);
        if (gui->PanelToHandle(replacement) == handle || gui->HandleToPanel(handle) != 0) {
            return 1;
        }
        gui->FreePanel(replacement);
        gui->FreePanel(parent);
    }
    FreeLibrary(module);
    FreeLibrary(filesystem);
    std::printf("Production VGUI parenting, messages and stale registry handles passed %u cycles: %s\n", Cycles,
                argv[1]);
    return 0;
}
