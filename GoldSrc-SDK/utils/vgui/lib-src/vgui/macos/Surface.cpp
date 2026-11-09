#include "VGUI_Surface.h"
#include "VGUI_Cursor.h"
#include "VGUI_Bitmap.h"
#include "VGUI_Font.h"
#include "VGUI_ImagePanel.h"
#include "VGUI_Panel.h"
#include "../posix/VGui1FontPlatform.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <vector>

namespace vgui {

namespace {
constexpr int TextureChannels = VGUI_RGBA_BYTES;
constexpr int MaximumColorComponent = 255;
constexpr SDL_Color OpaqueWhite{MaximumColorComponent, MaximumColorComponent, MaximumColorComponent,
    MaximumColorComponent};

struct TexturePixels {
    std::vector<unsigned char> bytes;
    int width = 0;
    int height = 0;
    std::uint64_t generation = 0;
};

// Bitmap caches one upload and ID across windows. CPU pixels therefore belong
// to this image, while each SDL renderer owns a separate realization.
std::unordered_map<int, TexturePixels> texturePixels;
int nextTextureId = 1;
std::uint64_t textureGeneration = 0;

struct RenderTexture {
    SDL_Texture* texture = nullptr;
    std::uint64_t generation = 0;
};
}

class SurfacePlat {
public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Cursor* cursor = nullptr;
    bool videoInitialized = false;
    BaseFontPlat* font = nullptr;
    SDL_Color color = OpaqueWhite;
    SDL_Color textColor = OpaqueWhite;
    int textX = 0;
    int textY = 0;
    int offsetX = 0;
    int offsetY = 0;
    int texture = 0;
    std::unordered_map<int, RenderTexture> textures;

    SDL_Texture* ResolveTexture(int id) {
        const auto pixels = texturePixels.find(id);
        if (pixels == texturePixels.end()) {
            return nullptr;
        }
        auto& local = textures[id];
        const auto& source = pixels->second;
        if (local.texture && local.generation == source.generation) {
            return local.texture;
        }
        SDL_Texture* replacement = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC, source.width, source.height);
        if (!replacement) {
            return nullptr;
        }
        if (!SDL_UpdateTexture(replacement, nullptr, source.bytes.data(), source.width * TextureChannels)) {
            SDL_DestroyTexture(replacement);
            return nullptr;
        }
        SDL_SetTextureBlendMode(replacement, SDL_BLENDMODE_BLEND);
        if (local.texture) {
            SDL_DestroyTexture(local.texture);
        }
        local = {replacement, source.generation};
        return replacement;
    }

    ~SurfacePlat() {
        for (const auto& entry : textures) {
            if (entry.second.texture) {
                SDL_DestroyTexture(entry.second.texture);
            }
        }
        if (cursor) {
            SDL_DestroyCursor(cursor);
        }
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        if (videoInitialized) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    }
};

void CseVgui1SelectFontForSurfaceV1(SurfacePlat* surface, BaseFontPlat* font) {
    if (surface) {
        surface->font = font;
    }
}

Surface::~Surface() {
    delete _plat;
}

bool Surface::createPlat() {
    _plat = new SurfacePlat;
    int width = 0;
    int height = 0;
    getPanel()->getSize(width, height);
    _plat->videoInitialized = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (!_plat->videoInitialized
        || !SDL_CreateWindowAndRenderer("CounterStrikeExtended", std::max(width, 1), std::max(height, 1),
            SDL_WINDOW_RESIZABLE, &_plat->window, &_plat->renderer)) {
        std::fprintf(stderr, "VGUI1 surface creation failed: %s\n", SDL_GetError());
        std::abort();
    }
    SDL_SetRenderDrawBlendMode(_plat->renderer, SDL_BLENDMODE_BLEND);
    return true;
}

bool Surface::recreateContext() {
    applyChanges();
    return true;
}

void Surface::applyChanges() {
    int width = 0;
    int height = 0;
    getPanel()->getSize(width, height);
    SDL_SetWindowSize(_plat->window, std::max(width, 1), std::max(height, 1));
}

void Surface::swapBuffers() {
    SDL_RenderPresent(_plat->renderer);
}

void Surface::drawSetColor(int r, int g, int b, int a) {
    _plat->color = {static_cast<Uint8>(r), static_cast<Uint8>(g), static_cast<Uint8>(b),
        static_cast<Uint8>(MaximumColorComponent - a)};
    SDL_SetRenderDrawColor(_plat->renderer, _plat->color.r, _plat->color.g, _plat->color.b, _plat->color.a);
}

void Surface::drawFilledRect(int x0, int y0, int x1, int y1) {
    const SDL_FRect rect{static_cast<float>(x0 + _plat->offsetX), static_cast<float>(y0 + _plat->offsetY),
        static_cast<float>(x1 - x0), static_cast<float>(y1 - y0)};
    SDL_RenderFillRect(_plat->renderer, &rect);
}

void Surface::drawOutlinedRect(int x0, int y0, int x1, int y1) {
    drawFilledRect(x0, y0, x1, y0 + 1);
    drawFilledRect(x0, y1 - 1, x1, y1);
    drawFilledRect(x0, y0 + 1, x0 + 1, y1 - 1);
    drawFilledRect(x1 - 1, y0 + 1, x1, y1 - 1);
}

void Surface::drawSetTextColor(int r, int g, int b, int a) {
    _plat->textColor = {static_cast<Uint8>(r), static_cast<Uint8>(g), static_cast<Uint8>(b),
        static_cast<Uint8>(MaximumColorComponent - a)};
}

void Surface::drawSetTextPos(int x, int y) {
    _plat->textX = x;
    _plat->textY = y;
}

void Surface::drawSetTextFont(Font* font) {
    _plat->font = font ? font->getPlat() : nullptr;
}

void Surface::drawPrintText(const char* text, int length) {
    if (!text || !_plat->font) {
        return;
    }
    constexpr int Channels = VGUI_RGBA_BYTES;
    const int height = _plat->font->getTall();
    for (int index = 0; index < length; ++index) {
        const unsigned char character = static_cast<unsigned char>(text[index]);
        int a = 0;
        int b = 0;
        int c = 0;
        _plat->font->getCharABCwide(character, a, b, c);
        if (b > 0 && height > 0 && b <= (std::numeric_limits<int>::max)() / Channels / height) {
            std::vector<unsigned char> pixels(static_cast<std::size_t>(b) * height * Channels);
            _plat->font->getCharRGBA(character, 0, 0, b, height, pixels.data());
            SDL_Texture* glyph = SDL_CreateTexture(_plat->renderer, SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_STATIC, b, height);
            if (glyph) {
                SDL_UpdateTexture(glyph, nullptr, pixels.data(), b * Channels);
                SDL_SetTextureBlendMode(glyph, SDL_BLENDMODE_BLEND);
                SDL_SetTextureColorMod(glyph, _plat->textColor.r, _plat->textColor.g, _plat->textColor.b);
                SDL_SetTextureAlphaMod(glyph, _plat->textColor.a);
                const SDL_FRect target{static_cast<float>(_plat->textX + a + _plat->offsetX),
                    static_cast<float>(_plat->textY + _plat->offsetY),
                    static_cast<float>(b), static_cast<float>(height)};
                SDL_RenderTexture(_plat->renderer, glyph, nullptr, &target);
                SDL_DestroyTexture(glyph);
            }
        }
        _plat->textX += a + b + c;
    }
}

int Surface::createNewTextureID() {
    if (nextTextureId == (std::numeric_limits<int>::max)()) {
        std::abort();
    }
    return nextTextureId++;
}

void Surface::drawSetTexture(int id) {
    _plat->texture = id;
}

void Surface::drawSetTextureRGBA(int id, const char* pixels, int width, int height) {
    if (!pixels || width <= 0 || height <= 0 || width > (std::numeric_limits<int>::max)() / TextureChannels) {
        return;
    }
    TexturePixels source;
    const auto pitch = static_cast<std::size_t>(width) * TextureChannels;
    if (static_cast<std::size_t>(height) > source.bytes.max_size() / pitch) {
        return;
    }
    if (textureGeneration == (std::numeric_limits<std::uint64_t>::max)()) {
        std::abort();
    }
    const auto* bytes = reinterpret_cast<const unsigned char*>(pixels);
    source.bytes.assign(bytes, bytes + pitch * static_cast<std::size_t>(height));
    source.width = width;
    source.height = height;
    source.generation = ++textureGeneration;
    texturePixels[id] = std::move(source);
}

void Surface::drawTexturedRect(int x0, int y0, int x1, int y1) {
    SDL_Texture* texture = _plat->ResolveTexture(_plat->texture);
    if (!texture) {
        return;
    }
    const SDL_FRect target{static_cast<float>(x0 + _plat->offsetX), static_cast<float>(y0 + _plat->offsetY),
        static_cast<float>(x1 - x0), static_cast<float>(y1 - y0)};
    SDL_SetTextureColorMod(texture, _plat->color.r, _plat->color.g, _plat->color.b);
    SDL_SetTextureAlphaMod(texture, _plat->color.a);
    SDL_RenderTexture(_plat->renderer, texture, nullptr, &target);
}

void Surface::pushMakeCurrent(Panel* panel, bool useInsets) {
    int x = 0;
    int y = 0;
    int rootX = 0;
    int rootY = 0;
    int unusedRight = 0;
    int unusedBottom = 0;
    panel->getAbsExtents(x, y, unusedRight, unusedBottom);
    getPanel()->getAbsExtents(rootX, rootY, unusedRight, unusedBottom);
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    if (useInsets) {
        panel->getInset(left, top, right, bottom);
    }
    _plat->offsetX = x - rootX + left;
    _plat->offsetY = y - rootY + top;
    // A content viewport would clip borders drawn at negative inset positions.
    // Match GDI's separate viewport origin and panel clip region instead.
    SDL_SetRenderViewport(_plat->renderer, nullptr);
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    panel->getClipRect(x0, y0, x1, y1);
    const SDL_Rect clip{x0 - rootX, y0 - rootY, std::max(0, x1 - x0), std::max(0, y1 - y0)};
    SDL_SetRenderClipRect(_plat->renderer, &clip);
}

void Surface::popMakeCurrent(Panel*) {
    _plat->offsetX = 0;
    _plat->offsetY = 0;
    SDL_SetRenderClipRect(_plat->renderer, nullptr);
    SDL_SetRenderViewport(_plat->renderer, nullptr);
}

void Surface::enableMouseCapture(bool state) {
    SDL_CaptureMouse(state);
}

void Surface::setCursor(Cursor* cursor) {
    _currentCursor = cursor;
    const auto code = cursor ? cursor->getDefaultCursor() : Cursor::dc_none;
    if (code == Cursor::dc_none) {
        SDL_HideCursor();
        return;
    }
    // VGUI's custom bitmap cursor is painted by SurfaceBase.
    if (code == Cursor::dc_user) {
        _emulatedCursor->setImage(cursor->getBitmap());
        setEmulatedCursorVisible(true);
        SDL_HideCursor();
        return;
    }
    setEmulatedCursorVisible(false);
    SDL_SystemCursor shape = SDL_SYSTEM_CURSOR_DEFAULT;
    switch (code) {
    case Cursor::dc_ibeam: shape = SDL_SYSTEM_CURSOR_TEXT; break;
    case Cursor::dc_hourglass: shape = SDL_SYSTEM_CURSOR_WAIT; break;
    case Cursor::dc_crosshair: shape = SDL_SYSTEM_CURSOR_CROSSHAIR; break;
    case Cursor::dc_sizenwse: shape = SDL_SYSTEM_CURSOR_NWSE_RESIZE; break;
    case Cursor::dc_sizenesw: shape = SDL_SYSTEM_CURSOR_NESW_RESIZE; break;
    case Cursor::dc_sizewe: shape = SDL_SYSTEM_CURSOR_EW_RESIZE; break;
    case Cursor::dc_sizens: shape = SDL_SYSTEM_CURSOR_NS_RESIZE; break;
    case Cursor::dc_sizeall: shape = SDL_SYSTEM_CURSOR_MOVE; break;
    case Cursor::dc_no: shape = SDL_SYSTEM_CURSOR_NOT_ALLOWED; break;
    case Cursor::dc_hand: shape = SDL_SYSTEM_CURSOR_POINTER; break;
    default: break;
    }
    SDL_Cursor* replacement = SDL_CreateSystemCursor(shape);
    if (replacement) {
        SDL_SetCursor(replacement);
        SDL_DestroyCursor(_plat->cursor);
        _plat->cursor = replacement;
    }
    SDL_ShowCursor();
}

void Surface::invalidate(Panel*) {
    requestSwap();
}

void Surface::setTitle(const char* title) {
    SDL_SetWindowTitle(_plat->window, title ? title : "");
}

bool Surface::hasFocus() {
    return (SDL_GetWindowFlags(_plat->window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

bool Surface::isWithin(int x, int y) {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(_plat->window, &width, &height);
    return x >= 0 && y >= 0 && x < width && y < height;
}

void Surface::GetMousePos(int& x, int& y) {
    float mouseX = 0;
    float mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    x = static_cast<int>(mouseX);
    y = static_cast<int>(mouseY);
}

int Surface::getModeInfoCount() {
    resetModeInfo();
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &count);
    for (int index = 0; index < count; ++index) {
        addModeInfo(modes[index]->w, modes[index]->h, SDL_BITSPERPIXEL(modes[index]->format));
    }
    SDL_free(modes);
    return SurfaceBase::getModeInfoCount();
}

bool Surface::setFullscreenMode(int width, int height, int bpp) {
    SDL_DisplayMode mode{};
    if (!SDL_GetClosestFullscreenDisplayMode(SDL_GetDisplayForWindow(_plat->window), width, height, 0, false, &mode)
        || SDL_BITSPERPIXEL(mode.format) != bpp) {
        return false;
    }
    return SDL_SetWindowFullscreenMode(_plat->window, &mode) && SDL_SetWindowFullscreen(_plat->window, true);
}

void Surface::setWindowedMode() {
    SDL_SetWindowFullscreen(_plat->window, false);
}

void Surface::setAsTopMost(bool state) {
    SDL_SetWindowAlwaysOnTop(_plat->window, state);
}

} // namespace vgui
