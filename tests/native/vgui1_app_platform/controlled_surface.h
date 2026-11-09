#ifndef CSE_TEST_CONTROLLED_SURFACE_H
#define CSE_TEST_CONTROLLED_SURFACE_H
#include "VGUI_SurfaceBase.h"
#include <cstdlib>

// Actual declared base type and virtual call; controlled constructor/unused
// methods are fixture bindings, NOT a working native Surface/draw backend.
class ControlledSurface : public vgui::SurfaceBase {
public:
    ControlledSurface() : vgui::SurfaceBase(nullptr) {
    }
    int reads = 0;
    void GetMousePos(int& x, int& y) override {
        ++reads;
        x = -123;
        y = 456;
    }
    void setTitle(const char*) override {
        std::abort(); // Any unqualified backend use fails.
    }
    bool setFullscreenMode(int, int, int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void setWindowedMode() override {
        std::abort(); // Any unqualified backend use fails.
    }
    void setAsTopMost(bool) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void createPopup(vgui::Panel*) override {
        std::abort(); // Any unqualified backend use fails.
    }
    bool hasFocus() override {
        std::abort(); // Any unqualified backend use fails.
    }
    bool isWithin(int, int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    int createNewTextureID() override {
        std::abort(); // Any unqualified backend use fails.
    }
    void drawSetColor(int, int, int, int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void drawFilledRect(int, int, int, int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void drawOutlinedRect(int, int, int, int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void drawSetTextFont(vgui::Font*) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void drawSetTextColor(int, int, int, int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void drawSetTextPos(int, int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void drawPrintText(const char*, int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void drawSetTextureRGBA(int, const char*, int, int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void drawSetTexture(int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void drawTexturedRect(int, int, int, int) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void invalidate(vgui::Panel*) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void enableMouseCapture(bool) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void setCursor(vgui::Cursor*) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void swapBuffers() override {
        std::abort(); // Any unqualified backend use fails.
    }
    void pushMakeCurrent(vgui::Panel*, bool) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void popMakeCurrent(vgui::Panel*) override {
        std::abort(); // Any unqualified backend use fails.
    }
    void applyChanges() override {
        std::abort(); // Any unqualified backend use fails.
    }
};
#endif
