#pragma once

#include "vgui/IClientPanel.h"

class ClientSpy : public vgui2::IClientPanel {
public:
    vgui2::VPANEL childAdded = 0;
    vgui2::VPANEL messageFrom = 0;
    KeyValues *message = nullptr;
    vgui2::VPANEL GetVPanel() override {
        return 0;
    }
    void Think() override {}
    void PerformApplySchemeSettings() override {}
    void PaintTraverse(bool, bool) override {}
    void Repaint() override {}
    vgui2::VPANEL IsWithinTraverse(int, int, bool) override {
        return 0;
    }
    void GetInset(int &, int &, int &, int &) override {}
    void GetClipRect(int &, int &, int &, int &) override {}
    void OnChildAdded(vgui2::VPANEL child) override {
        childAdded = child;
    }
    void OnSizeChanged(int, int) override {}
    void InternalFocusChanged(bool) override {}
    bool RequestInfo(KeyValues *) override {
        return false;
    }
    void RequestFocus(int) override {}
    bool RequestFocusPrev(vgui2::VPANEL) override {
        return false;
    }
    bool RequestFocusNext(vgui2::VPANEL) override {
        return false;
    }
    void OnMessage(const KeyValues *params, vgui2::VPANEL from) override {
        message = const_cast<KeyValues *>(params);
        messageFrom = from;
    }
    vgui2::VPANEL GetCurrentKeyFocus() override {
        return 0;
    }
    int GetTabPosition() override {
        return 0;
    }
    const char *GetName() override {
        return "owned-client";
    }
    const char *GetClassName() override {
        return "ClientSpy";
    }
    vgui2::HScheme GetScheme() override {
        return 0;
    }
    bool IsProportional() override {
        return false;
    }
    bool IsAutoDeleteSet() override {
        return false;
    }
    void DeletePanel() override {}
    void *QueryInterface(vgui2::EInterfaceID) override {
        return nullptr;
    }
    vgui2::Panel *GetPanel() override {
        return nullptr;
    }
    const char *GetModuleName() override {
        return "owned-fixture";
    }
};
