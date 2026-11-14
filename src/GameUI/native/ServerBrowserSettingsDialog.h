#pragma once

#include "ServerBrowserMasterservers.h"
#include "Frame.h"
#include <functional>

namespace vgui2 {
class ListPanel;
class TextEntry;
class Button;
class Label;
}
namespace cse::server_browser {
struct BrowserSession;
class BrowserSettingsDialog final : public vgui2::Frame {
    DECLARE_CLASS_SIMPLE_NAMED(BrowserSettingsDialog, vgui2::Frame, "Frame");
public:
    BrowserSettingsDialog(vgui2::Panel* parent, BrowserSession& session,
        std::function<void(const MasterserverSettings&)> submit);
    void Retire();
    void OnMessage(const KeyValues* message, vgui2::VPANEL sender) override;
protected:
    void OnCommand(const char* command) override;
    void OnClose() override;
private:
    void Populate();
    std::string Selection() const;
    BrowserSession& session_;
    MasterserverSettings draft_;
    std::function<void(const MasterserverSettings&)> submit_;
    vgui2::ListPanel* list_;
    vgui2::TextEntry* name_;
    vgui2::TextEntry* addresses_;
    vgui2::Button* remove_;
    vgui2::Label* error_;
    bool retired_ = false;
};
}
