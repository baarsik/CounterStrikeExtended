#include "VGUI_App.h"
#include "VGUI_Panel.h"
#include "VGUI_Scheme.h"
#include "VGUI_SurfaceBase.h"
#include <cstdio>
#include <cstdlib>

namespace {
[[noreturn]] void Unrelated() {
    std::fputs("Unexpected unrelated VGUI drawing/service call\n", stderr);
    std::abort();
}
}
namespace vgui {
App* App::getInstance() {
    Unrelated();
}

Scheme* App::getScheme() {
    Unrelated();
}

void Scheme::getColor(SchemeColor, int&, int&, int&, int&) {
    Unrelated();
}

SurfaceBase* Panel::getSurfaceBase() {
    Unrelated();
}

void Panel::drawSetColor(Scheme::SchemeColor) {
    Unrelated();
}

void Panel::drawSetColor(int, int, int, int) {
    Unrelated();
}

void Panel::drawFilledRect(int, int, int, int) {
    Unrelated();
}

void Panel::drawOutlinedRect(int, int, int, int) {
    Unrelated();
}

void Panel::drawSetTextFont(Scheme::SchemeFont) {
    Unrelated();
}

void Panel::drawSetTextFont(Font*) {
    Unrelated();
}

void Panel::drawSetTextColor(Scheme::SchemeColor) {
    Unrelated();
}

void Panel::drawSetTextColor(int, int, int, int) {
    Unrelated();
}

void Panel::drawSetTextPos(int, int) {
    Unrelated();
}

void Panel::drawPrintText(const char*, int) {
    Unrelated();
}

void Panel::drawPrintText(int, int, const char*, int) {
    Unrelated();
}

void Panel::drawPrintChar(char) {
    Unrelated();
}

void Panel::drawPrintChar(int, int, char) {
    Unrelated();
}

void Panel::drawSetTextureRGBA(int, const char*, int, int) {
    Unrelated();
}

void Panel::drawSetTexture(int) {
    Unrelated();
}

void Panel::drawTexturedRect(int, int, int, int) {
    Unrelated();
}
}
