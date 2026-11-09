#include "cse_rect.h"
#include <stddef.h>
_Static_assert(sizeof(RECT) == 16, "Canonical C rectangle size");
_Static_assert(offsetof(RECT, left) == 0 && offsetof(RECT, top) == 4 && offsetof(RECT, right) == 8
    && offsetof(RECT, bottom) == 12, "Canonical C rectangle field order");
RECT CseRectCDeclarationProbe = {1, 2, 3, 4};
