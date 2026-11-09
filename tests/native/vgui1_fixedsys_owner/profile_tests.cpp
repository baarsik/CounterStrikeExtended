#include "fake_provider.h"
#include "VGui1FontServices.h"
#include "VGUI_Font.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using Result = CseVgui1FontResultV1;
constexpr int FixedSysHeight = 18;
constexpr int RegularWeight = 400;
constexpr int CachedFontId = -1;
constexpr const char* FixedSysProfile = "apple-fixedsys18-courier-cell8x15-gray-v1";

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(17);
    }
}

void Prepare(vgui::Font& font) {
    CseVgui1FontStatusV1 status{};
    status.structSize = sizeof(status);
    char diagnostic[256] = {};
    Require(CseVgui1FontPrepareV1(font.getPlat(), &status, diagnostic, sizeof(diagnostic)) == Result::Ready,
        diagnostic);
}

void Check(const char* name, int tall, int wide, float rotation, int weight,
    bool italic, bool underline, bool strikeout, bool symbol, bool roleExpected) {
    vgui::Font_Reset();
    vgui::Font font(name, tall, wide, rotation, weight, italic, underline, strikeout, symbol);
    Prepare(font);
    const auto request = FakeFontLastRequest();
    Require(request.requestedName == name, "Requested font identity changed");
    Require(request.tall == tall && request.wide == wide && request.rotation == rotation
        && request.weight == weight && request.ansiCodePage == cse_vgui1::LegacyAnsiCodePage,
        "Request parameters changed");
    const auto expectedFlags = (italic ? cse_vgui1::FontItalic : 0)
        | (underline ? cse_vgui1::FontUnderline : 0) | (strikeout ? cse_vgui1::FontStrikeout : 0)
        | (symbol ? cse_vgui1::FontSymbol : 0);
    Require(request.flags == expectedFlags, "Requested font flags changed");
#if defined(__APPLE__)
    if (roleExpected) {
        Require(request.lookupIdentity == "Courier" && request.resolutionProfile == FixedSysProfile,
            "Exact Apple FixedSys role was not dispatched");
    } else {
        Require(request.lookupIdentity == name && request.resolutionProfile == "exact-request-v1",
            "Unmeasured neighboring request acquired a fallback");
    }
#else
    (void)roleExpected;
    Require(request.lookupIdentity == name && request.resolutionProfile == "exact-request-v1",
        "Apple role leaked to another platform");
#endif
    const int realizations = FakeFontSnapshot().realizations;
    const int firstId = font.getId();
    vgui::Font repeated(name, tall, wide, rotation, weight, italic, underline, strikeout, symbol);
    Prepare(repeated);
    Require(repeated.getPlat() == font.getPlat() && repeated.getId() == CachedFontId && font.getId() == firstId
        && FakeFontSnapshot().realizations == realizations, "Existing cache/identity behavior changed");
}

}

int main(int argc, char** argv) {
    Require(argc == 4, "Three owned model paths required");
    const char* paths[]{argv[1], argv[2], argv[3]};
    Require(CseVgui1FontsBindFilesV1(paths, 3, nullptr, 0) == Result::FilesBound, "Bind authored fake-provider models");
    Check("FixedSys", FixedSysHeight, 0, 0.0f, RegularWeight, false, false, false, false, true);
    Check("fIxEdSyS", FixedSysHeight, 0, 0.0f, RegularWeight, false, false, false, false, true);
    Check("Courier", FixedSysHeight, 0, 0.0f, RegularWeight, false, false, false, false, false);
    Check("FixedSys", FixedSysHeight - 1, 0, 0.0f, RegularWeight, false, false, false, false, false);
    Check("FixedSys", FixedSysHeight + 1, 0, 0.0f, RegularWeight, false, false, false, false, false);
    Check("FixedSys", FixedSysHeight, 1, 0.0f, RegularWeight, false, false, false, false, false);
    Check("FixedSys", FixedSysHeight, 0, 0.01f, RegularWeight, false, false, false, false, false);
    Check("FixedSys", FixedSysHeight, 0, 0.0f, RegularWeight - 1, false, false, false, false, false);
    Check("FixedSys", FixedSysHeight, 0, 0.0f, RegularWeight + 1, false, false, false, false, false);
    Check("FixedSys", FixedSysHeight, 0, 0.0f, 0, false, false, false, false, false);
    Check("FixedSys", FixedSysHeight, 0, 0.0f, RegularWeight, true, false, false, false, false);
    Check("FixedSys", FixedSysHeight, 0, 0.0f, RegularWeight, false, true, false, false, false);
    Check("FixedSys", FixedSysHeight, 0, 0.0f, RegularWeight, false, false, true, false, false);
    Check("FixedSys", FixedSysHeight, 0, 0.0f, RegularWeight, false, false, false, true, false);
    Require(CseVgui1FontsReleaseFilesV1(nullptr, 0) == Result::FilesReleased, "Release fake provider");
    std::puts("PASS exact FixedSys owner role and 12 neighboring requests; no native font provider executed");
    return 0;
}
