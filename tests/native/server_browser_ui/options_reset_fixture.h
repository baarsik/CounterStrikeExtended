#pragma once

#include <iterator>

#include "PropertyDialog.h"
#include "PropertyPage.h"
#include "ComboBox.h"
#include "../../../src/engine/modes.h"
#include "../../../src/GameUI/CvarToggleCheckButton.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "check.h"

namespace options_reset_fixture {
// The renderer is an external dependency of the Content handler. Populate its complete result.
struct RendererState {
    vmode_s modes[4]{{640, 480, 32}, {1280, 720, 32}, {1920, 1080, 32}, {3840, 2160, 32}};
    void GetVideoModes(vmode_s** list, int* count) {
        *list = modes;
        *count = static_cast<int>(std::size(modes));
    }
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
    void PrepareResolutionList(bool disallowBadResolutions);
    struct Settings {
        int windowed = 1;
        int w = 1920;
        int h = 1080;
    };
    Settings m_CurrentSettings;
    vgui2::CheckButton* m_pWindowed = nullptr;
    CCvarToggleCheckButton* m_pVsync = nullptr;
    vgui2::ComboBox* m_pMode = nullptr;
    vgui2::ComboBox* m_pRenderer = nullptr;
    vgui2::ComboBox* m_pAspectRatio = nullptr;
    bool m_bStartWidescreen = true;
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
    video->m_pMode = new vgui2::ComboBox(video, "Mode", 4, false);
    video->m_pRenderer = new vgui2::ComboBox(video, "Renderer", 2, false);
    video->m_pAspectRatio = new vgui2::ComboBox(video, "Aspect", 2, false);
    video->m_pRenderer->AddItem("Software", nullptr);
    video->m_pRenderer->ActivateItemByRow(0);
    vgui2::ivgui()->RunFrame();
    video->PrepareResolutionList(true);
    vgui2::ivgui()->RunFrame();
    char resolution[32]{};
    video->m_pMode->GetText(resolution, sizeof(resolution));
#if defined(__APPLE__)
    Require(std::strcmp(resolution, "1920 x 1080") == 0 && video->m_pMode->GetItemCount() == 2,
        "native software keeps the active 1080p mode and filters modes above its framebuffer limit");
#else
    Require(std::strcmp(resolution, "1280 x 720") == 0 && video->m_pMode->GetItemCount() == 1,
        "Windows software keeps its legacy resolution limit");
#endif
    auto* content = new COptionsSubAdvanced(&dialog);
    content->m_pDetailTextures = new CCvarToggleCheckButton(content, "ResetDetails", "Details", "fixture_cvar");
    verify(*content, *content->m_pDetailTextures);
}
} // namespace options_reset_fixture
