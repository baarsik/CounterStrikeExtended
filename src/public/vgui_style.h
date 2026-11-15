#pragma once

namespace cse {
namespace ui {

// Shared style selection for VGUI presentation. Individual controls opt into the extended style.
constexpr char VGuiStyleCvar[] = "vgui_style";
constexpr int OriginalVGuiStyle = 0;
constexpr int ExtendedVGuiStyle = 1;

} // namespace ui
} // namespace cse
