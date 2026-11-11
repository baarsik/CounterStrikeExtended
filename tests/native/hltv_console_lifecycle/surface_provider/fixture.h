#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using VPANEL = std::uintptr_t;
using HPanel = std::uint32_t;
using HWND = void*;
using DWORD = std::uint32_t;
static_assert(sizeof(DWORD) == 4, "controlled Win32 DWORD remains 32 bits");

constexpr DWORD WS_POPUP = 1;
constexpr DWORD WS_CLIPCHILDREN = 2;
constexpr DWORD WS_SYSMENU = 4;
constexpr DWORD WS_MINIMIZEBOX = 8;
constexpr DWORD WS_EX_TOOLWINDOW = 16;
constexpr DWORD WS_VISIBLE = 32;
constexpr DWORD WS_DISABLED = 64;
constexpr DWORD WS_MINIMIZE = 128;
constexpr DWORD SWP_NOREDRAW = 256;
constexpr DWORD SWP_HIDEWINDOW = 512;
constexpr int SW_HIDE = 0;
constexpr int SW_SHOWNA = 1;
constexpr int TRANSPARENT = 1;
constexpr int TA_UPDATECP = 1;
constexpr int SND_FILENAME = 1;
constexpr int SND_NOWAIT = 2;
constexpr int SND_NOSTOP = 4;
constexpr int SND_NODEFAULT = 8;
constexpr int SND_ASYNC = 16;
inline HWND HWND_BOTTOM = nullptr;

inline int osCalls = 0;
inline int liveWindows = 0;
inline int liveDrawingContexts = 0;
inline int liveRegions = 0;
inline int timers = 0;
inline int dragDropTargets = 0;
inline int staticSurfaceAvailable = 0;
inline bool staticPrimeSoundSystem = false;
inline int g_SurfaceDragDropTarget = 0;

struct SurfacePlat {
    HWND hwnd = nullptr;
    void* clipRgn = nullptr;
    void* hdc = nullptr;
    void* hwndDC = nullptr;
    void* bitmap = nullptr;
    int bitmapSize[2] = {};
    bool reservedFlag = false;
    VPANEL panel = 0;
    bool disabled = false;
    void* notifyIcon = nullptr;
    void* textureDC = nullptr;
};

class VPanel {
public:
    bool IsPopup();
    SurfacePlat* Plat();
    void SetPlat(SurfacePlat* plat);
    VPanel* GetParent();
    void SetParent(VPanel* parent) {
        _parent = parent;
    }

    void GetPos(int& x, int& y) {
        x = 0;
        y = 0;
    }

    void GetSize(int& width, int& height) {
        width = 1;
        height = 1;
    }

    void SetPopup(bool popup) {
        _popup = popup;
    }

    void SetKeyBoardInputEnabled(bool) {}
    void SetMouseInputEnabled(bool) {}
    bool IsVisible() {
        return false;
    }

private:
    bool _popup = false;
    SurfacePlat* _plat = nullptr;
    VPanel* _parent = nullptr;
};

struct PopupList {
    std::vector<VPANEL> panels;
    void RemoveElement(VPANEL panel) {
        panels.erase(std::remove(panels.begin(), panels.end(), panel), panels.end());
    }
    int Find(VPANEL panel) {
        constexpr int MissingPopup = -1;
        auto found = std::find(panels.begin(), panels.end(), panel);
        return found == panels.end() ? MissingPopup : static_cast<int>(found - panels.begin());
    }

    void AddElement(VPANEL panel) {
        panels.push_back(panel);
    }
};

class CWin32Surface {
public:
    void SetEmbeddedPanel(VPANEL panel);
    void SetContextPanel(VPANEL panel);
    SurfacePlat* ContextPlat();
    void CreatePopup(VPANEL panel, bool minimized, bool taskbar, bool disabled, bool mouse, bool keyboard);
    void ReleasePanel(VPANEL panel);
    void SetPanelVisible(VPANEL panel, bool visible);
    void SetNotifyIcon(VPANEL, int, void*, void*) {}
    void RecreateContext(VPANEL) {
        ++osCalls;
    }
    VPANEL _embeddedPanel = 0;
    VPANEL _currentContextPanel = 0;
    PopupList _popupList;
    char _language[1] = {};
};

struct ControlledSystem {
    bool GetRegistryString(const char*, char*, int) {
        ++osCalls;
        return false;
    }
};
struct ControlledFontManager {
    void SetLanguage(const char*) {
        ++osCalls;
    }
};
struct ControlledVGui {
    HPanel PanelToHandle(VPANEL) {
        return 1;
    }
};
inline ControlledSystem controlledSystem;
inline ControlledSystem* g_pSystem = &controlledSystem;
inline ControlledFontManager controlledFontManager;
inline ControlledFontManager& FontManager() {
    return controlledFontManager;
}

inline ControlledVGui controlledVGui;
inline ControlledVGui* g_pIVgui = &controlledVGui;

inline HWND CreateWindowExA(DWORD, const char*, const char*, DWORD, int, int, int, int,
                           HWND, void*, void*, void*) {
    ++osCalls;
    ++liveWindows;
    return new bool(false);
}

inline void* CreateRectRgn(int, int, int, int) {
    ++osCalls;
    ++liveRegions;
    return new char;
}

inline void* CreateCompatibleDC(void*) {
    ++osCalls;
    ++liveDrawingContexts;
    return new char;
}

inline void* GetModuleHandleA(void*) {
    ++osCalls;
    return nullptr;
}

inline void SetBkMode(void*, int) {
    ++osCalls;
}

inline void WriteWindowPanelHandle(HWND, HPanel) {
    ++osCalls;
}

inline void SetTextAlign(void*, int) {
    ++osCalls;
}

inline void ShowWindow(HWND, int) {
    ++osCalls;
}

inline void RegisterDragDrop(HWND, int*) {
    ++osCalls;
    ++dragDropTargets;
}

inline void RevokeDragDrop(HWND) {
    ++osCalls;
    --dragDropTargets;
}

inline void PlaySoundA(const char*, void*, int) {
    ++osCalls;
}

inline void __debugbreak() {
    std::abort();
}

inline void SetTimer(HWND window, int, int, void*) {
    ++osCalls;
    if (!*static_cast<bool*>(window)) {
        ++timers;
        *static_cast<bool*>(window) = true;
    }
}

inline void SetWindowPos(HWND, HWND, int, int, int, int, DWORD) {
    ++osCalls;
}

inline void DeleteObject(void* object) {
    ++osCalls;
    --liveRegions;
    delete static_cast<char*>(object);
}

inline void DeleteDC(void* context) {
    ++osCalls;
    --liveDrawingContexts;
    delete static_cast<char*>(context);
}

inline void ReleaseDC(HWND, void*) {
    ++osCalls;
}

inline void DestroyWindow(HWND window) {
    ++osCalls;
    --liveWindows;
    if (*static_cast<bool*>(window)) {
        --timers;
    }
    delete static_cast<bool*>(window);
}
