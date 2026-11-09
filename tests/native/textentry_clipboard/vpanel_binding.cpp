// A separate TU avoids combining the private VPanel Dar with the controls Dar.
// The owned fixture compiles the actual VPanel implementation; no UI is run.
#include "VPanel.h"
#include "vgui/IClientPanel.h"
#include "vgui/ISurface.h"

namespace vgui2 {
ISurface* g_pSurface = nullptr;
}

vgui2::VPANEL CreateActualVPanel(vgui2::IClientPanel* client) {
    auto* panel = new vgui2::VPanel();
    panel->Init(client);
    return reinterpret_cast<vgui2::VPANEL>(panel);
}

void DestroyActualVPanel(vgui2::VPANEL handle) {
    delete reinterpret_cast<vgui2::VPanel*>(handle);
}

void SendViaActualVPanel(vgui2::VPANEL target, KeyValues* message, vgui2::VPANEL sender) {
    reinterpret_cast<vgui2::VPanel*>(target)->SendMessage(message, sender);
}
