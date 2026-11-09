#include <VGUI_App.h>

#include <cstddef>

extern "C" std::size_t VguiPublicAppSize() {
    return sizeof(vgui::App);
}
