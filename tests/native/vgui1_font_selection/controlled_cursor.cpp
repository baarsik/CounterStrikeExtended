#include "VGUI_Cursor.h"

// Unrelated cursor binding only. Actual Scheme and native Font/owner methods
// run; cursor images/Surface creation are outside this component.
vgui::Cursor::Cursor(DefaultCursor cursor) {
    _dc = cursor;
    privateInit(nullptr, 0, 0);
}
