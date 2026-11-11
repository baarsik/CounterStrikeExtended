#pragma once

#include "interface.h"
#include "filesystem.h"
#include "vgui/VGUI.h"
#include <cstdint>

struct ControlledVGuiServices {
    void (*event)(const char* name);
    bool (*schemeAvailable)();
};

namespace vgui2 {

inline ControlledVGuiServices* controlledServices = nullptr;

class Panel {
public:
    Panel(Panel*, const char*) {
        controlledServices->event("panel-acquired");
    }
    ~Panel() {
        controlledServices->event("panel-destroyed");
    }
    VPANEL GetVPanel() {
        return static_cast<VPANEL>(reinterpret_cast<std::uintptr_t>(this));
    }
};

class ControlledSurface {
public:
    void SetEmbeddedPanel(VPANEL panel) {
        controlledServices->event(panel ? "panel-embedded" : "panel-detached");
    }
    void Shutdown() {
        controlledServices->event("surface-shutdown");
    }
};

class ControlledScheme {
public:
    bool LoadSchemeFromFile(const char*, const char*) {
        controlledServices->event("scheme-load");
        return controlledServices->schemeAvailable();
    }
};

class ControlledVGui {
public:
    void Start() {
        controlledServices->event("controls-start");
    }
    void Stop() {
        controlledServices->event("controls-stop");
    }
    void RunFrame() {}
};

class ControlledLocalize {
public:
    void AddFile(IFileSystem*, const char*) {}
};

inline ControlledSurface* surface() {
    static ControlledSurface service;
    return &service;
}
inline ControlledScheme* scheme() {
    static ControlledScheme service;
    return &service;
}
inline ControlledVGui* ivgui() {
    static ControlledVGui service;
    return &service;
}
inline ControlledLocalize* localize() {
    static ControlledLocalize service;
    return &service;
}
inline IFileSystem* controlledFileSystem = nullptr;
inline IFileSystem* filesystem() {
    return controlledFileSystem;
}
inline bool VGuiControls_Init(const char*, CreateInterfaceFn* factories, int) {
    controlledServices = static_cast<ControlledVGuiServices*>(factories[1]("ControlledVGuiServices", nullptr));
    controlledFileSystem = static_cast<IFileSystem*>(factories[2]("VFileSystem009", nullptr));
    controlledServices->event("controls-ready");
    return true;
}

}
