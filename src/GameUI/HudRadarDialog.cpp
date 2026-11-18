#include "HudRadarDialog.h"

#include "GameUI_Interface.h"
#include "wrect.h"
#include "cl_dll.h"
#include "Button.h"
#include "CheckButton.h"
#include "ComboBox.h"
#include "Label.h"
#include "Slider.h"
#include "Tooltip.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "../vgui2/controls/SmokedGlass.h"
#include "../cstrike/cl_dll/nextclient/radar_cvars.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>

namespace {

constexpr float SliderPrecision = 100.0f;
constexpr int DesignWidth = 640;
constexpr int DesignHeight = 580;
constexpr int ScreenMargin = 12;
constexpr char MapModeKey[] = "mode";

struct Setting {
    const char* name;
    const char* label;
};

constexpr Setting SliderSettings[] = {
    {"hud_radar_scale", "#GameUI_HudRadar_RadarScale"},
    {"hud_radar_map_scale", "#GameUI_HudRadar_MapScale"},
    {"hud_radar_opacity", "#GameUI_HudRadar_Opacity"},
    {"hud_radar_name_size", "#GameUI_HudRadar_NameSize"},
};

constexpr Setting CheckSettings[] = {
    {"hud_radar_round", "#GameUI_HudRadar_Round"},
    {"hud_radar_fullmap", "#GameUI_HudRadar_FullMap"},
    {"hud_radar_rotate", "#GameUI_HudRadar_Rotation"},
    {"hud_radar_bg", "#GameUI_HudRadar_BlurBackground"},
    {"hud_radar_map_smoothing", "#GameUI_HudRadar_MapSmoothing"},
    {"hud_radar_map_shift", "#GameUI_HudRadar_MapShift"},
    {"hud_radar_names", "#GameUI_HudRadar_Names"},
    {"hud_radar_health", "#GameUI_HudRadar_Health"},
    {"hud_radar_tracks", "#GameUI_HudRadar_Tracks"},
    {"hud_radar_shift_scoreboard", "#GameUI_HudRadar_ShiftScoreboard"},
};

float SettingValue(const char* name, bool defaults) {
    return defaults ? static_cast<float>(std::atof(radar::FindCvar(name).default_value))
                    : engine->pfnGetCvarFloat(const_cast<char*>(name));
}

float SliderSettingValue(const char* name, bool defaults) {
    const auto definition = radar::FindCvar(name);
    const float value = SettingValue(name, defaults);
    return std::isfinite(value) ? (std::clamp)(value, definition.min_value, definition.max_value)
                                : static_cast<float>(std::atof(definition.default_value));
}

} // namespace

CHudRadarDialog::CHudRadarDialog(vgui2::Panel* parent) : BaseClass(parent, "HudRadarDialog", false, false) {
    static_assert(std::size(SliderSettings) == SliderCount);
    static_assert(std::size(CheckSettings) == CheckCount);
    SetTitle("#GameUI_HudRadar_Title", true);
    SetSizeable(false);
    SetDeleteSelfOnClose(true);
    int workspaceX = 0;
    int workspaceY = 0;
    int workspaceWide = 0;
    int workspaceTall = 0;
    vgui2::surface()->GetWorkspaceBounds(workspaceX, workspaceY, workspaceWide, workspaceTall);
    const float scale =
        (std::min)({vgui2::smoked::PixelScale(), static_cast<float>(workspaceWide - ScreenMargin * 2) / DesignWidth,
                  static_cast<float>(workspaceTall - ScreenMargin * 2) / DesignHeight});
    SetSize(static_cast<int>(DesignWidth * scale), static_cast<int>(DesignHeight * scale));

    m_modeLabel = new vgui2::Label(this, "ModeLabel", "#GameUI_HudRadar_Mode");
    m_mode = new vgui2::ComboBox(this, "RadarMode", 2, false);
    m_mode->AddItem("#GameUI_HudRadar_Classic", nullptr);
    m_mode->AddItem("#GameUI_HudRadar_Advanced", nullptr);
    m_mode->AddActionSignalTarget(this);
    m_modeLabel->SetAssociatedControl(m_mode);
    m_mapModeLabel = new vgui2::Label(this, "MapModeLabel", "#GameUI_HudRadar_MapMode");
    m_mapMode = new vgui2::ComboBox(this, "MapMode", static_cast<int>(std::size(kRadarMapModeDefs)), false);
    for (const auto& definition : kRadarMapModeDefs) {
        KeyValues* data = new KeyValues("MapMode", MapModeKey, static_cast<int>(definition.mode));
        m_mapMode->AddItem(definition.label, data);
        data->deleteThis();
    }
    m_mapMode->AddActionSignalTarget(this);
    m_mapModeLabel->SetAssociatedControl(m_mapMode);

    for (int index = 0; index < SliderCount; ++index) {
        const auto& setting = SliderSettings[index];
        const auto definition = radar::FindCvar(setting.name);
        const std::string labelName = std::string(setting.name) + "Label";
        m_sliderLabels[index] = new vgui2::Label(this, labelName.c_str(), setting.label);
        m_sliders[index] = new vgui2::Slider(this, setting.name);
        m_sliders[index]->SetRange(static_cast<int>(definition.min_value * SliderPrecision),
                                   static_cast<int>(definition.max_value * SliderPrecision));
        m_sliders[index]->AddActionSignalTarget(this);
        m_sliderLabels[index]->SetAssociatedControl(m_sliders[index]);
        const std::string valueName = std::string(setting.name) + "Value";
        m_values[index] = new vgui2::Label(this, valueName.c_str(), "");
    }
    for (int index = 0; index < CheckCount; ++index) {
        m_checks[index] = new vgui2::CheckButton(this, CheckSettings[index].name, CheckSettings[index].label);
        m_checks[index]->AddActionSignalTarget(this);
    }
    for (int index = 0; index < CheckCount; ++index) {
        if (std::strcmp(CheckSettings[index].name, "hud_radar_map_shift") == 0) {
            m_checks[index]->GetTooltip()->SetText("#GameUI_HudRadar_MapShiftTooltip");
        }
    }
    m_defaults = new vgui2::Button(this, "Defaults", "#GameUI_HudRadar_Defaults");
    m_defaults->SetCommand("Defaults");
    m_defaults->AddActionSignalTarget(this);
    m_ok = new vgui2::Button(this, "OK", "#GameUI_OK");
    m_ok->SetCommand("OK");
    m_ok->AddActionSignalTarget(this);
    m_cancel = new vgui2::Button(this, "Cancel", "#GameUI_Cancel");
    m_cancel->SetCommand("Close");
    m_cancel->AddActionSignalTarget(this);
    m_apply = new vgui2::Button(this, "Apply", "#GameUI_Apply");
    m_apply->SetCommand("Apply");
    m_apply->AddActionSignalTarget(this);
    m_ok->SetAsDefaultButton(true);
    LoadValues(false);
}

float CHudRadarDialog::SliderValue(int index) const {
    return static_cast<float>(m_sliders[index]->GetValue()) / SliderPrecision;
}

float CHudRadarDialog::MapModeValue() const {
    KeyValues* data = m_mapMode->GetActiveItemUserData();
    return data ? static_cast<float>(data->GetInt(MapModeKey)) : static_cast<float>(RadarMapMode::SliceTextured);
}

void CHudRadarDialog::LoadValues(bool defaults) {
    m_defaultsRequested = defaults;
    constexpr float AdvancedModeThreshold = radar::FindCvar("hud_radar_mode").max_value;
    const float mode = SettingValue("hud_radar_mode", defaults) >= AdvancedModeThreshold ? 1.0f : 0.0f;
    m_mode->ActivateItemByRow(static_cast<int>(mode));
    const auto mapMode = RadarMapMode_FromCvar(SettingValue("hud_radar_map_mode", defaults));
    for (int row = 0; row < static_cast<int>(std::size(kRadarMapModeDefs)); ++row) {
        if (kRadarMapModeDefs[row].mode == mapMode) {
            m_mapMode->ActivateItemByRow(row);
            break;
        }
    }
    for (int index = 0; index < SliderCount; ++index) {
        m_sliders[index]->SetValue(
            static_cast<int>(std::lround(SliderSettingValue(SliderSettings[index].name, defaults) * SliderPrecision)),
            false);
    }
    for (int index = 0; index < CheckCount; ++index) {
        m_checks[index]->SetSelected(SettingValue(CheckSettings[index].name, defaults) != 0.0f);
    }
    if (!defaults) {
        m_baseline[0] = mode;
        m_baseline[1] = MapModeValue();
        for (int index = 0; index < SliderCount; ++index) {
            m_baseline[2 + index] = SliderValue(index);
        }
        for (int index = 0; index < CheckCount; ++index) {
            m_baseline[2 + SliderCount + index] = m_checks[index]->IsSelected() ? 1.0f : 0.0f;
        }
    }
    UpdateControls();
}

void CHudRadarDialog::ApplyValues() {
    const auto apply = [this](int index, const char* name, float value) {
        if (m_defaultsRequested || value != m_baseline[index]) {
            engine->Cvar_SetValue(const_cast<char*>(name), value);
        }
    };
    apply(0, "hud_radar_mode", m_mode->GetActiveItem() != 0 ? 1.0f : 0.0f);
    apply(1, "hud_radar_map_mode", MapModeValue());
    for (int index = 0; index < SliderCount; ++index) {
        apply(2 + index, SliderSettings[index].name, SliderValue(index));
    }
    for (int index = 0; index < CheckCount; ++index) {
        apply(2 + SliderCount + index, CheckSettings[index].name, m_checks[index]->IsSelected() ? 1.0f : 0.0f);
    }
    LoadValues(false);
}

void CHudRadarDialog::UpdateModified() {
    bool modified = m_defaultsRequested || m_baseline[0] != (m_mode->GetActiveItem() != 0 ? 1.0f : 0.0f) ||
        m_baseline[1] != MapModeValue();
    for (int index = 0; index < SliderCount; ++index) {
        modified = modified || m_baseline[2 + index] != SliderValue(index);
    }
    for (int index = 0; index < CheckCount; ++index) {
        modified = modified || m_baseline[2 + SliderCount + index] != (m_checks[index]->IsSelected() ? 1.0f : 0.0f);
    }
    m_apply->SetEnabled(modified);
}

void CHudRadarDialog::UpdateControls() {
    for (int index = 0; index < SliderCount; ++index) {
        char text[32]{};
        const bool percent = std::strcmp(SliderSettings[index].name, "hud_radar_opacity") == 0;
        std::snprintf(text, sizeof(text), percent ? "%.0f%%" : "%.2f", SliderValue(index));
        m_values[index]->SetText(text);
    }
    const bool generated = MapModeValue() != static_cast<float>(RadarMapMode::OverviewTextured);
    bool names = false;
    for (int index = 0; index < CheckCount; ++index) {
        if (std::strcmp(CheckSettings[index].name, "hud_radar_map_smoothing") == 0) {
            m_checks[index]->SetEnabled(generated);
        }
        if (std::strcmp(CheckSettings[index].name, "hud_radar_names") == 0) {
            names = m_checks[index]->IsSelected();
        }
    }
    for (int index = 0; index < SliderCount; ++index) {
        if (std::strcmp(SliderSettings[index].name, "hud_radar_name_size") == 0) {
            m_sliders[index]->SetEnabled(names);
            m_sliderLabels[index]->SetEnabled(names);
            m_values[index]->SetEnabled(names);
        }
    }
    UpdateModified();
}

void CHudRadarDialog::OnControlChanged() {
    UpdateControls();
}

void CHudRadarDialog::OnCheckChanged() {
    UpdateControls();
}

void CHudRadarDialog::OnTextChanged() {
    UpdateControls();
}

void CHudRadarDialog::OnCommand(const char* command) {
    if (std::strcmp(command, "Defaults") == 0) {
        LoadValues(true);
        return;
    }
    if (std::strcmp(command, "OK") == 0 || std::strcmp(command, "Apply") == 0) {
        ApplyValues();
        if (std::strcmp(command, "OK") == 0) {
            Close();
        }
        return;
    }
    BaseClass::OnCommand(command);
}

void CHudRadarDialog::PerformLayout() {
    BaseClass::PerformLayout();
    const float scale = static_cast<float>(GetWide()) / DesignWidth;
    const auto pixels = [scale](int value) { return static_cast<int>(std::lround(value * scale)); };
    constexpr int Inset = 20;
    constexpr int LabelWidth = 180;
    constexpr int FieldX = Inset + LabelWidth;
    constexpr int RowHeight = 32;
    constexpr int RowStep = 44;
    constexpr int FirstRow = 52;
    constexpr int ValueWidth = 64;
    const int fieldWidth = DesignWidth - FieldX - Inset;
    m_modeLabel->SetBounds(pixels(Inset), pixels(FirstRow), pixels(LabelWidth), pixels(RowHeight));
    m_mode->SetBounds(pixels(FieldX), pixels(FirstRow), pixels(fieldWidth), pixels(RowHeight));
    const int mapTop = FirstRow + RowStep;
    m_mapModeLabel->SetBounds(pixels(Inset), pixels(mapTop), pixels(LabelWidth), pixels(RowHeight));
    m_mapMode->SetBounds(pixels(FieldX), pixels(mapTop), pixels(fieldWidth), pixels(RowHeight));
    for (int index = 0; index < SliderCount; ++index) {
        const int top = FirstRow + (index + 2) * RowStep;
        m_sliderLabels[index]->SetBounds(pixels(Inset), pixels(top), pixels(LabelWidth), pixels(RowHeight));
        m_sliders[index]->SetBounds(pixels(FieldX), pixels(top), pixels(fieldWidth - ValueWidth), pixels(RowHeight));
        m_values[index]->SetBounds(pixels(DesignWidth - Inset - ValueWidth), pixels(top), pixels(ValueWidth),
                                   pixels(RowHeight));
    }
    constexpr int CheckTop = FirstRow + 6 * RowStep;
    constexpr int CheckRowStep = 36;
    constexpr int ColumnWidth = (DesignWidth - Inset * 2) / 2;
    for (int index = 0; index < CheckCount; ++index) {
        const int top = CheckTop + (index / 2) * CheckRowStep;
        m_checks[index]->SetBounds(pixels(Inset + (index % 2) * ColumnWidth), pixels(top), pixels(ColumnWidth),
                                   pixels(RowHeight));
    }
    constexpr int ButtonTop = DesignHeight - 52;
    constexpr int ButtonWidth = 112;
    constexpr int ButtonGap = 8;
    m_defaults->SetBounds(pixels(Inset), pixels(ButtonTop), pixels(144), pixels(RowHeight));
    m_ok->SetBounds(pixels(DesignWidth - Inset - ButtonWidth * 3 - ButtonGap * 2), pixels(ButtonTop),
                    pixels(ButtonWidth), pixels(RowHeight));
    m_cancel->SetBounds(pixels(DesignWidth - Inset - ButtonWidth * 2 - ButtonGap), pixels(ButtonTop),
                        pixels(ButtonWidth), pixels(RowHeight));
    m_apply->SetBounds(pixels(DesignWidth - Inset - ButtonWidth), pixels(ButtonTop), pixels(ButtonWidth),
                       pixels(RowHeight));
}
