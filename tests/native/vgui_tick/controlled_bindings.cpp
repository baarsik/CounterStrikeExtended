#include <VGUI_App.h>
#include <VGUI_Scheme.h>

// The scheduler is the production App.cpp. Platform clock, UI setup, clipboard,
// and registry bindings are controlled here so no window, font, or provider starts.
long vguiTestClockMillis = 0;

namespace vgui {

long App::getTimeMillis() {
    return vguiTestClockMillis;
}

void App::platTick() {
}

void App::setCursorPos(int, int) {
}

void App::getCursorPos(int& x, int& y) {
    x = 0;
    y = 0;
}

void App::internalSetMouseArena(int, int, int, int, bool) {
}

void App::setClipboardText(const char*, int) {
}

int App::getClipboardTextCount() {
    return 0;
}

int App::getClipboardText(int, char*, int) {
    return 0;
}

int App::getRegistryString(const char*, char*, int) {
    return 0;
}

int App::setRegistryString(const char*, const char*) {
    return 0;
}

int App::setRegistryInteger(const char*, int) {
    return 0;
}

int App::getRegistryInteger(const char*, int&) {
    return 0;
}

Scheme::Scheme() : _color{}, _font{}, _cursor{} {
}

void Scheme::setColor(SchemeColor color, int red, int green, int blue, int alpha) {
    _color[color][0] = red;
    _color[color][1] = green;
    _color[color][2] = blue;
    _color[color][3] = alpha;
}

void Scheme::getColor(SchemeColor color, int& red, int& green, int& blue, int& alpha) {
    red = _color[color][0];
    green = _color[color][1];
    blue = _color[color][2];
    alpha = _color[color][3];
}

void Scheme::setFont(SchemeFont font, Font* value) {
    _font[font] = value;
}

Font* Scheme::getFont(SchemeFont font) {
    return _font[font];
}

void Scheme::setCursor(SchemeCursor cursor, Cursor* value) {
    _cursor[cursor] = value;
}

Cursor* Scheme::getCursor(SchemeCursor cursor) {
    return _cursor[cursor];
}

void Font_Reset() {
}

}
