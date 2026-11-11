#pragma once
#include "platform_types.h"
struct A3DCTRL_SRC_SUPER;
// Private producer/consumer declarations. Task 4 supplies the production storage and bodies.
class A3dNativeMixer {
public:
    HRESULT Render(SHORT* stereo, DWORD frames);
};
class A3dNativeVoice {
public:
    HRESULT SetControl(const A3DCTRL_SRC_SUPER& control);
};
