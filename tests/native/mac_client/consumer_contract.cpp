// Consume the public SDK headers, as the actual Half-Life client does.
#include "VGUI_App.h"
#include "VGUI_Font.h"
#include "VGUI_Scheme.h"
#include "VGUI_TextGrid.h"
#include "VGUI_TextImage.h"
#include "VGui1FontServices.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

class App final : public vgui::App {
public:
    void main(int, char*[]) override {}
};

#if defined(__clang__)
__attribute__((noinline))
#endif
void AssignFont(vgui::TextImage& image, vgui::Font* font) {
    image.setFont(font);
}

#if defined(__clang__)
__attribute__((noinline))
#endif
void AssignSchemeFont(vgui::TextImage& image, vgui::Scheme::SchemeFont role) {
    image.setFont(role);
}

#if defined(__clang__)
__attribute__((noinline))
#endif
void AssignText(vgui::TextImage& image, const char* text) {
    image.setText(text);
}

#if defined(__clang__)
__attribute__((noinline))
#endif
void AssignSizedText(vgui::TextImage& image, int bytes, const char* text) {
    image.setText(bytes, text);
}
}

int main(int argc, char** argv) {
    Require(argc == 2, "select font or grid contract");
    App app;
    char diagnostic[256]{};
    Require(CseVgui1FontsBindFilesV1(nullptr, 0, diagnostic, sizeof(diagnostic))
        == CseVgui1FontResultV1::FilesBound, diagnostic);
    auto* primary = app.getScheme()->getFont(vgui::Scheme::sf_primary1);
    CseVgui1FontStatusV1 status{};
    status.structSize = sizeof(status);
    Require(CseVgui1FontPrepareV1(primary->getPlat(), &status, diagnostic, sizeof(diagnostic))
        == CseVgui1FontResultV1::Ready, diagnostic);
    if (std::strcmp(argv[1], "font") == 0) {
        vgui::Font explicitFont("Helvetica", 16, 0, 0, 400, false, false, false, false);
        vgui::TextImage image("consumer text");
        AssignText(image, "short");
        int shortWide = 0;
        int shortTall = 0;
        image.getTextSize(shortWide, shortTall);
        constexpr char LongerText[] = "longer consumer text";
        AssignSizedText(image, sizeof(LongerText), LongerText);
        int longerWide = 0;
        int longerTall = 0;
        image.getTextSize(longerWide, longerTall);
        Require(shortWide > 0 && longerWide > shortWide && longerTall == shortTall,
            "both virtual text overloads preserve text and font metrics");
        AssignFont(image, &explicitFont);
        Require(image.getFont() == &explicitFont, "virtual pointer font overload preserves exact font identity");
        vgui::TextImage schemeImage("scheme text");
        AssignSchemeFont(schemeImage, vgui::Scheme::sf_primary2);
        Require(schemeImage.getFont() == app.getScheme()->getFont(vgui::Scheme::sf_primary2),
            "virtual scheme font overload selects the requested role");
    } else {
        constexpr std::size_t GuardBytes = 128;
        constexpr unsigned char Canary = 0xa5;
        struct Storage {
            alignas(vgui::TextGrid) unsigned char object[sizeof(vgui::TextGrid)];
            std::array<unsigned char, GuardBytes> tail;
        } storage{};
        storage.tail.fill(Canary);
        auto* grid = new (storage.object) vgui::TextGrid(8, 4, 0, 0, 80, 40);
        Require(std::all_of(storage.tail.begin(), storage.tail.end(), [](unsigned char byte) { return byte == Canary; }),
            "provider constructor respects consumer TextGrid allocation size");
        grid->printf("grid %d", 1);
        grid->~TextGrid();
    }
    Require(CseVgui1FontsReleaseFilesV1(diagnostic, sizeof(diagnostic))
        == CseVgui1FontResultV1::FilesReleased, diagnostic);
}
