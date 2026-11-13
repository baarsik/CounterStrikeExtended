#pragma once

#include "Label.h"
#include "vgui/KeyValues.h"

namespace cse {
namespace gameui {

// Stock resources still define obsolete sound credits. Constructor-owned controls
// consume their settings so resource and scheme reloads cannot recreate the labels.
class ObsoleteSoundCredit final : public vgui2::Label {
public:
    ObsoleteSoundCredit(vgui2::Panel* parent, const char* panelName) : vgui2::Label(parent, panelName, "") {
        SetVisible(false);
    }

    void ApplySettings(KeyValues* settings) override {
        vgui2::Label::ApplySettings(settings);
        SetText("");
        SetVisible(false);
    }
};

}
}
