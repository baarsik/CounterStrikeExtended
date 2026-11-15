#pragma once

#include "PropertyDialog.h"
#include "PropertyPage.h"
#include "../../../src/GameUI/CvarToggleCheckButton.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "check.h"

namespace options_reset_fixture {
// The renderer is an external dependency of the Content handler. Populate its complete result.
struct RendererState {
    void GetCurrentRenderer(char* name, int, int* windowed, int* hdModels, int* addons) {
        name[0] = '\0';
        *windowed = 1;
        *hdModels = 0;
        *addons = 0;
    }
};
inline RendererState renderer;
inline RendererState* gameuifuncs = &renderer;

class COptionsSubVideo : public vgui2::PropertyPage {
public:
    explicit COptionsSubVideo(vgui2::Panel* parent) : PropertyPage(parent, "VideoResetOwner", true) {
        AddActionSignalTarget(parent);
    }
    void OnDataChanged();
    void OnButtonChecked(KeyValues* data);
    struct Settings {
        int windowed = 1;
    };
    Settings m_CurrentSettings;
    vgui2::CheckButton* m_pWindowed = nullptr;
    CCvarToggleCheckButton* m_pVsync = nullptr;
};

class COptionsSubAdvanced : public vgui2::PropertyPage {
public:
    explicit COptionsSubAdvanced(vgui2::Panel* parent) : PropertyPage(parent, "ContentResetOwner", true) {
        AddActionSignalTarget(parent);
    }
    void OnDataChanged();
    void OnButtonChecked(KeyValues* data);
    vgui2::CheckButton* m_pOriginalModels = nullptr;
    vgui2::CheckButton* m_pAddonsFolder = nullptr;
    CCvarToggleCheckButton* m_pDetailTextures = nullptr;
};

// Complete production handlers are copied by the builder with a source receipt.
#include "options_button_reset_owner.inc"

inline void Run(vgui2::Panel* root) {
    vgui2::PropertyDialog dialog(root, "ResetNotificationDialog");
    dialog.SetApplyButtonVisible(true);
    const auto verify = [&](auto& page, CCvarToggleCheckButton& checkbox) {
        KeyValues* data = new KeyValues("CheckButtonChecked");
        data->SetPtr("panel", &checkbox);
        const auto notify = [&] {
            data->SetInt("state", checkbox.IsSelected());
            page.OnButtonChecked(data);
            vgui2::ivgui()->RunFrame();
        };
        dialog.EnableApplyButton(false);
        checkbox.Reset();
        notify();
        Require(!dialog.IsApplyButtonEnabled(), "reset notifications keep Apply disabled for unchanged settings");
        checkbox.SetSelected(!checkbox.IsSelected());
        notify();
        Require(dialog.IsApplyButtonEnabled(), "a real checkbox change still enables Apply");
        dialog.EnableApplyButton(false);
        checkbox.Reset();
        notify();
        Require(!dialog.IsApplyButtonEnabled(), "restoring saved settings does not create a new pending edit");
        data->deleteThis();
    };
    auto* video = new COptionsSubVideo(&dialog);
    video->m_pVsync = new CCvarToggleCheckButton(video, "ResetVsync", "VSync", "fixture_cvar");
    verify(*video, *video->m_pVsync);
    auto* content = new COptionsSubAdvanced(&dialog);
    content->m_pDetailTextures = new CCvarToggleCheckButton(content, "ResetDetails", "Details", "fixture_cvar");
    verify(*content, *content->m_pDetailTextures);
}
} // namespace options_reset_fixture
