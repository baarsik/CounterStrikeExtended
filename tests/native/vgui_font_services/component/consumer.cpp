#include "VGUI_Font.h"
#include "VGui1FontServices.h"
#include <cstdio>
#include <cstdlib>

#ifndef CSE_FONT_CONSUMER_NAME
#error Consumer identity is required
#endif

namespace {
struct Borrower {
    vgui::Font font{"Owned Face", 20, 0, 0.0f, 400, false, false, false, false};

    ~Borrower() {
        // The process-exit control intentionally uses the font here. Merely
        // checking final absence would miss a service destroyed too early.
        CseVgui1FontStatusV1 status{};
        status.structSize = sizeof(status);
        if (CseVgui1FontPrepareV1(font.getPlat(), &status, nullptr, 0) != CseVgui1FontResultV1::Ready
            || status.state != CseVgui1FontStateV1::Ready) {
            std::abort();
        }
        const int height = font.getTall();
        if (height != 5) {
            std::abort();
        }
        if (const char* receipt = std::getenv("CSE_VGUI1_FONT_TEARDOWN_RECEIPT")) {
            FILE* output = std::fopen(receipt, "ab");
            if (!output) {
                std::abort();
            }
            std::fprintf(output, "consumer=%s live-metric=%d\n", CSE_FONT_CONSUMER_NAME, height);
            if (std::fclose(output) != 0) {
                std::abort();
            }
        }
    }
};

Borrower g_Borrower;
}

extern "C" int CseFixtureFontTall() {
    return g_Borrower.font.getTall();
}

extern "C" const void* CseFixtureFontPlatform() {
    return g_Borrower.font.getPlat();
}

extern "C" void CseFixtureFontReset() {
    vgui::Font_Reset();
}

extern "C" CseVgui1FontResultV1 CseFixtureFontPrepare() {
    CseVgui1FontStatusV1 status{};
    status.structSize = sizeof(status);
    return CseVgui1FontPrepareV1(g_Borrower.font.getPlat(), &status, nullptr, 0);
}
