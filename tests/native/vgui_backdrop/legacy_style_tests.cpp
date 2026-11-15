#include "vgui_smoked_glass.h"
#include "vgui_style_service.h"
#include <cassert>
#include <vector>

namespace vgui {
struct Scheme {
    enum SchemeColor { sc_user, sc_secondary3 };
    enum SchemeFont { sf_user, sf_primary1, sf_primary2, sf_primary3, sf_secondary1 };
};
struct Color {
    cse::ui::Rgba rgba{204, 204, 204, 0};
    Scheme::SchemeColor source = Scheme::sc_secondary3;
    void getColor(int& r, int& g, int& b, int& a) {
        r = rgba.r; g = rgba.g; b = rgba.b; a = rgba.a;
    }
    void getColor(Scheme::SchemeColor& value) { value = source; }
};
struct Panel {
    virtual ~Panel() = default;
    Color background;
    cse::ui::Rgba drawColor{};
    std::vector<cse::ui::Rgba> painted;
    void getPaintSize(int& width, int& height) { width = 80; height = 40; }
    Panel* getParent() { return nullptr; }
    void getAbsExtents(int& x, int& y, int& right, int& bottom) { x = 0; y = 0; right = 80; bottom = 40; }
    void getClipRect(int& x, int& y, int& right, int& bottom) { getAbsExtents(x, y, right, bottom); }
    void getInset(int& left, int& top, int& right, int& bottom) { left = top = right = bottom = 0; }
    bool hasFocus() { return false; }
    void getBgColor(int& r, int& g, int& b, int& a) { background.getColor(r, g, b, a); }
    void getBgColor(Color& value) { value = background; }
    void setBgColor(Color value) { background = value; }
    void setBgColor(int r, int g, int b, int a) { background.rgba = {r, g, b, a}; background.source = Scheme::sc_user; }
    void drawSetColor(int r, int g, int b, int a) { drawColor = {r, g, b, a}; }
    void drawFilledRect(int, int, int, int) { painted.push_back(drawColor); }
    void paintBackground();
};
struct Frame : Panel {};
struct Label : Panel {};
struct TextPanel : Panel {};
struct Button : Label {
    bool isSelected() { return false; }
    bool isArmed() { return false; }
};
}
using namespace vgui;
struct CLabelHeader : Label {
    int _row = 1;
    void paintBackground();
};
struct ScoreBoard { int m_iHighlightRow = -1; } scoreboard;
struct Viewport { ScoreBoard* GetScoreBoard() { return &scoreboard; } } viewport;
Viewport* gViewPort = &viewport;
int selectedStyle = 1;
int CSE_ABI_CALL CurrentStyle() { return selectedStyle; }
bool CSE_ABI_CALL Draw(const cse::ui::BackdropRect*, const cse::ui::BackdropRect*,
    const cse::ui::BackdropStyle*, float) { return false; }
cse::ui::StyleApi styleApi{sizeof(styleApi), cse::ui::StyleApiVersion, CurrentStyle, Draw};
#include "legacy_style.inc"
const cse::ui::StyleApi* cse_vgui1::smoked::Style() { return &styleApi; }

void Check(const cse::ui::Rgba& actual, cse::ui::Rgba expected) {
    assert(actual.r == expected.r && actual.g == expected.g && actual.b == expected.b && actual.a == expected.a);
}

int main() {
    assert(cse_vgui1::smoked::IsTextFont(Scheme::sf_primary1));
    assert(cse_vgui1::smoked::IsTextFont(Scheme::sf_primary3));
    assert(!cse_vgui1::smoked::IsTextFont(Scheme::sf_secondary1)); // Marlett owns checkbox/window symbols.
    assert(!cse_vgui1::smoked::IsTextFont(Scheme::sf_primary2)); // Fixed-width terminal text.
    assert(!cse_vgui1::smoked::IsTextFont(Scheme::sf_user));
    CLabelHeader row;
    // The actual scoreboard assigns explicit RGB/inverse-alpha for local player and killer rows.
    for (auto color : {cse::ui::Rgba{64, 96, 255, 196}, cse::ui::Rgba{255, 0, 0, 75}}) {
        row.setBgColor(color.r, color.g, color.b, color.a);
        row.paintBackground();
        assert(!row.painted.empty());
        Check(row.painted.back(), color);
    }
    scoreboard.m_iHighlightRow = row._row;
    row.paintBackground();
    Check(row.painted.back(), {134, 91, 19, 0});
    Check(row.background.rgba, {255, 0, 0, 75}); // Hover drawing restores the stored killer state.
    selectedStyle = 0;
    scoreboard.m_iHighlightRow = -1;
    row.paintBackground();
    Check(row.painted.back(), {255, 0, 0, 75});
    Color literal;
    literal.source = Scheme::sc_user;
    literal.rgba = {255, 160, 0, 33};
    selectedStyle = 1;
    Check(cse_vgui1::smoked::TextColor(literal), literal.rgba);
}
