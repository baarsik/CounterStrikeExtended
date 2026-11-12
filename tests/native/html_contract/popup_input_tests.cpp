#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace vgui2 {
enum MouseCode { MOUSE_LEFT };
using VPANEL = unsigned int;
struct Input {
    VPANEL mouseOver = 2;
    VPANEL GetMouseOver() const {
        return mouseOver;
    }
};
Input* input() {
    static Input value;
    return &value;
}
}

struct Serializer {
    int x = 0;
    int y = 0;
    int moves = 0;
    int clicks = 0;
    void MouseMove(int nextX, int nextY) {
        x = nextX;
        y = nextY;
        ++moves;
    }
};

class HTMLComboBoxHost;
class HTML {
public:
    Serializer* m_Serializer = nullptr;
    HTMLComboBoxHost* m_pComboBoxHost = nullptr;
    int screenX = 50;
    int screenY = 70;
    vgui2::VPANEL GetVPanel() const {
        return 1;
    }
    void ScreenToLocal(int& x, int& y) const {
        x -= screenX;
        y -= screenY;
    }
    void BrowserSizePopup(int x, int y, int wide, int tall);
    void OnMousePressed(vgui2::MouseCode) {
        ++m_Serializer->clicks;
    }
    void OnMouseReleased(vgui2::MouseCode) {
        ++m_Serializer->clicks;
    }
    void OnMouseDoublePressed(vgui2::MouseCode) {
        ++m_Serializer->clicks;
    }
};

class HTMLComboBoxHost {
public:
    HTML* m_pParent;
    int screenX = 0;
    int screenY = 0;
    int width = 0;
    int height = 0;
    vgui2::VPANEL GetVPanel() const {
        return 2;
    }
    void LocalToScreen(int& x, int& y) const {
        x += screenX;
        y += screenY;
    }
    void SetBounds(int x, int y, int wide, int tall) {
        screenX = x;
        screenY = y;
        width = wide;
        height = tall;
    }
    void OnCursorMoved(int x, int y);
    void OnMousePressed(vgui2::MouseCode code);
    void OnMouseReleased(vgui2::MouseCode code);
    void OnMouseDoublePressed(vgui2::MouseCode code);
};

HTML* activeView = nullptr;
struct PanelCoordinates {
    void GetAbsPos(vgui2::VPANEL, int& x, int& y) {
        x = activeView->screenX;
        y = activeView->screenY;
    }
};
PanelCoordinates* ipanel() {
    static PanelCoordinates value;
    return &value;
}

#include "popup_input.inc"

int main() {
    try {
        Serializer serializer;
        HTML view;
        HTMLComboBoxHost popup{&view};
        view.m_Serializer = &serializer;
        view.m_pComboBoxHost = &popup;
        activeView = &view;
        view.BrowserSizePopup(300, 200, 120, 80);
        popup.OnCursorMoved(10, 10);
#if INTPTR_MAX > INT32_MAX
        if (serializer.x != 310 || serializer.y != 210) {
            throw std::runtime_error("popup input did not reach browser-view coordinates");
        }
#else
        if (serializer.x != 10 || serializer.y != 10) {
            throw std::runtime_error("legacy popup input coordinates changed");
        }
#endif
        popup.OnMousePressed(vgui2::MOUSE_LEFT);
        popup.OnMouseReleased(vgui2::MOUSE_LEFT);
        popup.OnMouseDoublePressed(vgui2::MOUSE_LEFT);
        if (serializer.clicks != 3 || serializer.moves != 1) {
            throw std::runtime_error("popup clicks did not preserve the translated cursor");
        }
        vgui2::input()->mouseOver = 0;
        popup.OnCursorMoved(90, 90);
        if (serializer.moves != 1) {
            throw std::runtime_error("popup forwarded movement while another panel owned the mouse");
        }
#if INTPTR_MAX > INT32_MAX
        popup.m_pParent = nullptr;
        popup.OnCursorMoved(1, 1);
#endif
        std::puts("popup input fixtures passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
