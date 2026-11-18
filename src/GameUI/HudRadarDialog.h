#pragma once

#include "Frame.h"

#include <array>

namespace vgui2 {
class Button;
class CheckButton;
class ComboBox;
class Label;
class Slider;
} // namespace vgui2

// Native presentation of NextClient's HUD radar settings. Edits are committed by OK or Apply.
class CHudRadarDialog : public vgui2::Frame {
    DECLARE_CLASS_SIMPLE_NAMED(CHudRadarDialog, vgui2::Frame, "Frame");

  public:
    explicit CHudRadarDialog(vgui2::Panel* parent);
    void OnCommand(const char* command) override;

  protected:
    void PerformLayout() override;
    MESSAGE_FUNC(OnControlChanged, "SliderMoved");
    MESSAGE_FUNC(OnCheckChanged, "CheckButtonChecked");
    MESSAGE_FUNC(OnTextChanged, "TextChanged");

  private:
    static constexpr int SliderCount = 4;
    static constexpr int CheckCount = 10;
    void LoadValues(bool defaults);
    void ApplyValues();
    void UpdateControls();
    void UpdateModified();
    float SliderValue(int index) const;
    float MapModeValue() const;

    vgui2::ComboBox* m_mode;
    vgui2::ComboBox* m_mapMode;
    vgui2::Label* m_modeLabel;
    vgui2::Label* m_mapModeLabel;
    std::array<vgui2::Slider*, SliderCount> m_sliders{};
    std::array<vgui2::Label*, SliderCount> m_sliderLabels{};
    std::array<vgui2::Label*, SliderCount> m_values{};
    std::array<vgui2::CheckButton*, CheckCount> m_checks{};
    vgui2::Button* m_defaults;
    vgui2::Button* m_ok;
    vgui2::Button* m_cancel;
    vgui2::Button* m_apply;
    std::array<float, 2 + SliderCount + CheckCount> m_baseline{};
    bool m_defaultsRequested = false;
};
