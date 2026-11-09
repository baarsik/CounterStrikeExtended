#include "VGUI_Scheme.h"

// Deliberately controlled dependencies: no fonts/windows/native Surface are
// instantiated. Actual App.cpp and all thirteen new platform methods are linked.
namespace vgui {
Scheme::Scheme() : _color{}, _font{}, _cursor{} {
}
void Scheme::setColor(SchemeColor row, int r, int g, int b, int a) {
    _color[row][0] = r;
    _color[row][1] = g;
    _color[row][2] = b;
    _color[row][3] = a;
}
void Scheme::getColor(SchemeColor row, int& r, int& g, int& b, int& a) {
    r = _color[row][0];
    g = _color[row][1];
    b = _color[row][2];
    a = _color[row][3];
}
void Scheme::setFont(SchemeFont row, Font* font) {
    _font[row] = font;
}
Font* Scheme::getFont(SchemeFont row) {
    return _font[row];
}
void Scheme::setCursor(SchemeCursor row, Cursor* cursor) {
    _cursor[row] = cursor;
}
Cursor* Scheme::getCursor(SchemeCursor row) {
    return _cursor[row];
}
void Font_Reset() {
}
}
