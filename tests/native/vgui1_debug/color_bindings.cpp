#include "VGUI_App.h"
#include <cstdlib>

// Scheme-backed RGBA resolution is outside the copy fixture. Unexpected delegation fails closed.
vgui::App* vgui::App::getInstance() {
    std::abort();
}
