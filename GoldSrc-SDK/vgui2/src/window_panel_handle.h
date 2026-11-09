#pragma once

#include "cse_vgui.h"

namespace vgui2 {

// Window storage uses LONG_PTR, while the VGUI panel generation/index remains a fixed 32-bit handle.
inline HPanel ReadWindowPanelHandle(HWND window) {
    return static_cast<HPanel>(GetWindowLongPtrA(window, GWLP_USERDATA));
}

inline void WriteWindowPanelHandle(HWND window, HPanel handle) {
    SetWindowLongPtrA(window, GWLP_USERDATA, static_cast<LONG_PTR>(handle));
}

}
