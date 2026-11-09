#include "VGUI_App.h"
#include "VGUI_Font.h"
#include "VGUI_Panel.h"
#include "VGUI_LineBorder.h"
#include "VGUI_Bitmap.h"
#include "VGui1FontServices.h"
#include "VGUI_Surface.h"
#include "vgui/ISystem.h"
#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s: %s\n", message, SDL_GetError());
        std::exit(1);
    }
}

class App final : public vgui::App {
    void main(int, char*[]) override {}
};

class Surface final : public vgui::Surface {
public:
    explicit Surface(vgui::Panel* panel) : vgui::Surface(panel) {}
    using vgui::Surface::drawSetColor;
    using vgui::Surface::drawFilledRect;
    using vgui::Surface::drawSetTextureRGBA;
    using vgui::Surface::drawSetTexture;
    using vgui::Surface::drawTexturedRect;
    using vgui::Surface::drawSetTextFont;
    using vgui::Surface::drawSetTextPos;
    using vgui::Surface::drawSetTextColor;
    using vgui::Surface::drawPrintText;
    using vgui::Surface::createNewTextureID;
};

class Bitmap final : public vgui::Bitmap {
public:
    Bitmap() {
        setSize(1, 1);
        setRGBA(0, 0, 240, 70, 10, 255);
        setPos(4, 4);
        setColor(vgui::Color(255, 255, 255, 0));
    }
    ~Bitmap() {
        delete[] _rgba;
    }
};

void Pixel(SDL_Renderer* renderer, int x, int y, Uint8 red, Uint8 green, Uint8 blue) {
    SDL_Surface* image = SDL_RenderReadPixels(renderer, nullptr);
    Require(image != nullptr, "read actual rendered surface");
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;
    Require(SDL_ReadSurfacePixel(image, x, y, &r, &g, &b, &a), "read pixel");
    SDL_DestroySurface(image);
    if (r != red || g != green || b != blue) {
        std::fprintf(stderr, "pixel (%d,%d) = %u,%u,%u, expected %u,%u,%u\n", x, y, r, g, b, red, green, blue);
        std::exit(1);
    }
}
}

int main() {
    Require(SDL_Init(SDL_INIT_VIDEO), "initialize dummy video");
    auto factory = reinterpret_cast<CreateInterfaceFn>(dlsym(RTLD_DEFAULT, "CreateInterface"));
    // Resolve vgui2's factory specifically; both UI libraries export this name.
    Dl_info library{};
    Require(dladdr(reinterpret_cast<void*>(&vgui::App::getInstance), &library) != 0, "locate vgui image");
    const auto slash = std::string(library.dli_fname).find_last_of('/');
    const auto path = std::string(library.dli_fname).substr(0, slash + 1) + "vgui2.dylib";
    void* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    Require(module != nullptr, "load vgui2");
    factory = reinterpret_cast<CreateInterfaceFn>(dlsym(module, "CreateInterface"));
    Require(factory != nullptr, "resolve vgui2 factory");
    auto* system = static_cast<vgui2::ISystem*>(factory("VGUI_System009", nullptr));
    Require(system != nullptr, "load real native System interface");
    Require(factory("VGUI_Surface026", nullptr) == nullptr, "native client must use engine Surface");
    const double before = system->GetCurrentTime();
    SDL_Delay(5);
    Require(system->GetCurrentTime() >= before, "monotonic native clock");
    system->RunFrame();
    Require(system->GetFrameTime() >= before, "native frame clock");
    Require(system->CommandLineParamExists("-cse-fixture"), "native process argument");
    char argument[64]{};
    Require(system->GetCommandLineParamValue("-cse-fixture", argument, sizeof(argument)), "native argument value");
    Require(std::strcmp(argument, "argument with spaces") == 0, "quoted argument preservation");
    Require(!system->GetCommandLineParamValue("-missing", argument, sizeof(argument)), "missing argument failure");
    int year = 0;
    int month = 0;
    Require(system->GetCurrentTimeAndDate(&year, &month, nullptr, nullptr, nullptr, nullptr, nullptr), "calendar");
    Require(year >= 2026 && month >= 1 && month <= 12, "calendar output initialized");
    Require(system->GetFreeDiskSpace("/") > 0, "native disk availability");

    const wchar_t text[]{L'A', 0xd83d, 0xde00, L'Z'};
    system->SetClipboardText(text, 4);
    wchar_t output[8]{};
    Require(system->GetClipboardText(0, output, sizeof(output)) == 5, "wide clipboard count including NUL");
    Require(std::memcmp(output, text, sizeof(text)) == 0 && output[4] == 0, "UTF16 cells on wchar32 host");
    char raw[16]{};
    Require(system->GetClipboardText(2, raw, sizeof(raw)) == 8, "byte-offset clipboard extent");
    Require(static_cast<unsigned char>(raw[0]) == 0x3d && static_cast<unsigned char>(raw[1]) == 0xd8,
        "clipboard byte overload preserves UTF16LE");
    output[0] = L'Q';
    Require(system->GetClipboardText(-1, output, sizeof(output)) == 0 && output[0] == L'Q', "negative offset");

    App app;
    Require(app.getKeyCodeChar(vgui::KEY_A, false) == 'a'
        && app.getKeyCodeChar(vgui::KEY_A, true) == 'A', "VGUI1 printable key translation");
    char keyName[32]{};
    app.getKeyCodeText(vgui::KEY_A, keyName, sizeof(keyName));
    Require(std::strcmp(keyName, "KEY_A") == 0, "VGUI1 key binding label");
    char partial[]{'x', 'y', 'z', 'q'};
    app.setClipboardText(partial, 3);
    char copied[8]{};
    Require(app.getClipboardTextCount() == 4, "App bounded clipboard count");
    Require(app.getClipboardText(1, copied, sizeof(copied)) == 3 && std::strcmp(copied, "yz") == 0,
        "App clipboard offset and explicit input length");
    Require(app.getTimeMillis() >= 0, "App cold clock");
    Require(SDL_SetClipboardText(u8"café €"), "publish external Unicode text");
    const unsigned char legacy[]{'c', 'a', 'f', 0xe9, ' ', 0x80, 0};
    Require(app.getClipboardTextCount() == sizeof(legacy), "VGUI1 clipboard counts CP1252 bytes");
    Require(app.getClipboardText(0, copied, sizeof(copied)) == sizeof(legacy)
        && std::memcmp(copied, legacy, sizeof(legacy)) == 0, "VGUI1 decodes external Unicode to legacy bytes");
    app.setClipboardText(reinterpret_cast<const char*>(legacy), sizeof(legacy) - 1);
    char* publicText = SDL_GetClipboardText();
    Require(publicText && std::strcmp(publicText, u8"café €") == 0, "VGUI1 publishes legacy bytes as Unicode");
    SDL_free(publicText);

    vgui::Panel panel(0, 0, 64, 64);
    char diagnostic[256]{};
    Require(CseVgui1FontsBindFilesV1(nullptr, 0, diagnostic, sizeof(diagnostic)) == CseVgui1FontResultV1::FilesBound,
        "bind native font catalog");
    vgui::Font font("Helvetica", 16, 0, 0, 400, false, false, false, false);
    CseVgui1FontStatusV1 fontStatus{};
    fontStatus.structSize = sizeof(fontStatus);
    Require(CseVgui1FontPrepareV1(font.getPlat(), &fontStatus, diagnostic, sizeof(diagnostic))
        == CseVgui1FontResultV1::Ready, diagnostic);
    int a = 0;
    int b = 0;
    int c = 0;
    font.getCharABCwide('A', a, b, c);
    Require(font.getTall() > 0 && a + b + c > 0, "native font metrics");
    {
        Surface surface(&panel);
        int count = 0;
        SDL_Window** windows = SDL_GetWindows(&count);
        Require(count == 1, "one actual native Surface window");
        SDL_Renderer* renderer = SDL_GetRenderer(windows[0]);
        SDL_free(windows);
        Require(renderer != nullptr, "native Surface renderer");
        surface.drawSetColor(17, 83, 149, 0);
        surface.drawFilledRect(0, 0, 64, 64);
        Pixel(renderer, 4, 4, 17, 83, 149);
        constexpr unsigned char Texel[]{240, 70, 10, 255};
        const int texture = surface.createNewTextureID();
        surface.drawSetTextureRGBA(texture, reinterpret_cast<const char*>(Texel), 1, 1);
        surface.drawSetColor(255, 255, 255, 0);
        surface.drawSetTexture(texture);
        surface.drawTexturedRect(8, 8, 24, 24);
        Pixel(renderer, 16, 16, 240, 70, 10);
        Pixel(renderer, 4, 4, 17, 83, 149);
        panel.setBorder(new vgui::LineBorder(1, vgui::Color(255, 0, 0, 0)));
        panel.setBgColor(0, 255, 0, 0);
        panel.setPaintEnabled(false);
        panel.solveTraverse();
        panel.repaint();
        panel.paintTraverse();
        Pixel(renderer, 0, 0, 255, 0, 0);
        Pixel(renderer, 1, 1, 0, 255, 0);
        surface.drawSetTextFont(&font);
        surface.drawSetTextColor(255, 255, 255, 0);
        surface.drawSetTextPos(24, 24);
        surface.drawPrintText("A", 1);
        SDL_Surface* textPixels = SDL_RenderReadPixels(renderer, nullptr);
        Require(textPixels != nullptr, "read native glyph raster");
        bool glyphPainted = false;
        for (int y = 24; y < 48; ++y) {
            for (int x = 24; x < 48; ++x) {
                Uint8 red = 0;
                Uint8 green = 0;
                Uint8 blue = 0;
                Uint8 alpha = 0;
                Require(SDL_ReadSurfacePixel(textPixels, x, y, &red, &green, &blue, &alpha), "read glyph pixel");
                glyphPainted = glyphPainted || red != 0 || blue != 0;
            }
        }
        SDL_DestroySurface(textPixels);
        Require(glyphPainted, "actual native font glyph is visible");
        Bitmap shared;
        shared.doPaint(&panel);
        Pixel(renderer, 4, 4, 240, 70, 10);
        vgui::Panel popup(0, 0, 64, 64);
        {
            Surface popupSurface(&popup);
            int windowCount = 0;
            SDL_Window** allWindows = SDL_GetWindows(&windowCount);
            SDL_Renderer* popupRenderer = nullptr;
            for (int index = 0; index < windowCount; ++index) {
                SDL_Renderer* candidate = SDL_GetRenderer(allWindows[index]);
                if (candidate != renderer) {
                    popupRenderer = candidate;
                }
            }
            SDL_free(allWindows);
            Require(popupRenderer != nullptr, "popup renderer");
            popupSurface.drawSetColor(0, 255, 0, 0);
            popupSurface.drawFilledRect(0, 0, 64, 64);
            shared.doPaint(&popup);
            Pixel(popupRenderer, 4, 4, 240, 70, 10);
        }
    }
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_free(windows);
    Require(count == 0, "Surface shutdown releases its window");
    Require(CseVgui1FontsReleaseFilesV1(diagnostic, sizeof(diagnostic)) == CseVgui1FontResultV1::FilesReleased,
        "release native font catalog after drawing");
    system->Shutdown();
    system->Shutdown();
    dlclose(module);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    Require(SDL_WasInit(SDL_INIT_VIDEO) == 0, "Surface balances its video subsystem lease");
    SDL_Quit();
    std::puts("Native client System/App clipboard, clock, arguments, rendering and shutdown passed");
}
