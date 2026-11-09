#include "vgui/MessageMap.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace vgui2 {
class Panel {
public:
    void Invoke() {
        ++calls;
    }
    int calls = 0;
};
}

#if UINTPTR_MAX > UINT32_MAX
static_assert(alignof(vgui2::MessageMapItem_t) == alignof(vgui2::MessageFunc_t),
    "Native message maps require the member-pointer alignment, without the Win32 layout padding");
#endif

int main() {
    vgui2::MessageMapItem_t original{};
    original.name = "NativeMessage";
    original.func = &vgui2::Panel::Invoke;
    original.firstParamName = "first";
    original.secondParamName = "second";
    original.nameSymbol = 17;
    original.firstParamSymbol = 23;
    original.secondParamSymbol = 29;
    vgui2::MessageMapItem_t copied{};
    std::memcpy(&copied, &original, sizeof(copied));
    vgui2::Panel panel;
    (panel.*original.func)();
    (panel.*copied.func)();
    if (panel.calls != 2 || copied.name != original.name || copied.firstParamName != original.firstParamName ||
        copied.secondParamName != original.secondParamName || copied.nameSymbol != original.nameSymbol ||
        copied.firstParamSymbol != original.firstParamSymbol || copied.secondParamSymbol != original.secondParamSymbol) {
        return 1;
    }
    std::puts("Native VGUI message-map member pointer and fields retained");
    return 0;
}
