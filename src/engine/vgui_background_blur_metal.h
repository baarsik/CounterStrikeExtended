#pragma once
#include "../public/vgui_backdrop.h"
namespace cse::ui {
void BeginMetalBackgroundFrame();
void CaptureMetalBackground(int x, int y, int width, int height);
bool DrawMetalBackground(const BackdropRect& panel, const BackdropRect& clip, int width, int height,
    const BackdropStyle& style, float radius = 0);
}
