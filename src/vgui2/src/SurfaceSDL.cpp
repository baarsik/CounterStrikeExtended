// Standalone SDL implementation of the retained ISurface026 contract.
#include "vgui/ISurface.h"
#include "vgui/IInputInternal.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui_internal.h"
#include "VPanel.h"
#include "NativeKeyState.h"
#include "KeyValues.h"
#include "FileSystem.h"
#include "native/StandaloneWindow.h"
#include "../vgui_surfacelib/FontManager.h"
#include "../vgui_surfacelib/LinuxFont.h"
#include "../vgui_surfacelib/native/MacFontRegistration.h"
#include "../../common/sdl3_keyboard.h"
#include "../../common/sdl3_policy.h"
#include "../../common/legacy_text_encoding.h"
#include "../../common/legacy_bmp_records.h"
#include "image_codec.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace vgui2 {
extern ISchemeManager* g_pScheme;
// Retained record, identical to the Windows standalone vertex record.
class VGuiVertex {
public:
    int x;
    int y;
    float u;
    float v;
};
} // namespace vgui2
using namespace vgui2;
using cse::vgui::RasterImage;
using cse::vgui::StandaloneWindow;

namespace {
constexpr int OpaqueAlpha = 255;
constexpr int DefaultWidth = 640;
constexpr int DefaultHeight = 480;
constexpr int HdWidth = 1280;
constexpr int HdHeight = 720;
constexpr std::size_t MaximumFileBytes = 64 * 1024 * 1024;

bool DecodeIndexedBitmap(const std::vector<unsigned char>& file, std::vector<unsigned char>& rgba,
    int& width, int& height) {
    constexpr std::uint16_t BitmapSignature = 0x4d42;
    constexpr std::uint16_t IndexedBits = 8;
    constexpr std::size_t PaletteEntries = 256;
    constexpr std::size_t RowAlignment = 4;
    constexpr std::size_t HeaderBytes = sizeof(cse_bmp::FileHeader) + sizeof(cse_bmp::InfoHeader);
    constexpr std::size_t PaletteBytes = PaletteEntries * sizeof(cse_bmp::PaletteEntry);
    if (file.size() < HeaderBytes + PaletteBytes) {
        return false;
    }
    cse_bmp::FileHeader header{};
    cse_bmp::InfoHeader info{};
    std::memcpy(&header, file.data(), sizeof(header));
    std::memcpy(&info, file.data() + sizeof(header), sizeof(info));
    // Retain the original standalone loader's uncompressed, bottom-up 256-color BMP profile.
    if (header.bfType != BitmapSignature || info.biSize != sizeof(info) || info.biPlanes != 1
        || info.biBitCount != IndexedBits || info.biCompression != cse_bmp::RgbCompression
        || info.biWidth <= 0 || info.biHeight <= 0 || info.biWidth > cse::vgui::MaximumRasterDimension
        || info.biHeight > cse::vgui::MaximumRasterDimension || info.biClrUsed > PaletteEntries
        || header.bfOffBits < HeaderBytes + PaletteBytes || header.bfOffBits > file.size()) {
        return false;
    }
    const std::size_t pixels = static_cast<std::size_t>(info.biWidth) * info.biHeight;
    const std::size_t pitch = (static_cast<std::size_t>(info.biWidth) + RowAlignment - 1) & ~(RowAlignment - 1);
    if (pixels > MaximumFileBytes / cse::vgui::PixelChannels
        || pitch > (file.size() - header.bfOffBits) / static_cast<std::size_t>(info.biHeight)) {
        return false;
    }
    std::vector<unsigned char> replacement(pixels * cse::vgui::PixelChannels);
    for (int y = 0; y < info.biHeight; ++y) {
        const unsigned char* source = file.data() + header.bfOffBits
            + static_cast<std::size_t>(info.biHeight - y - 1) * pitch;
        for (int x = 0; x < info.biWidth; ++x) {
            const unsigned char* color = file.data() + HeaderBytes + source[x] * sizeof(cse_bmp::PaletteEntry);
            unsigned char* destination = replacement.data()
                + (static_cast<std::size_t>(y) * info.biWidth + x) * cse::vgui::PixelChannels;
            destination[0] = color[2];
            destination[1] = color[1];
            destination[2] = color[0];
            destination[3] = OpaqueAlpha;
        }
    }
    rgba.swap(replacement);
    width = info.biWidth;
    height = info.biHeight;
    return true;
}

VPanel* Panel(VPANEL panel) {
    return reinterpret_cast<VPanel*>(panel);
}

class CSDLSurface final : public ISurface {
public:
    ~CSDLSurface() override;
    void Shutdown() override;
    void RunFrame() override;
    VPANEL GetEmbeddedPanel() override;
    void SetEmbeddedPanel(VPANEL panel) override;
    void PushMakeCurrent(VPANEL panel, bool useInsets) override;
    void PopMakeCurrent(VPANEL panel) override;
    void DrawSetColor(int r, int g, int b, int a) override;
    void DrawSetColor(Color color) override;
    void DrawFilledRect(int x0, int y0, int x1, int y1) override;
    void DrawOutlinedRect(int x0, int y0, int x1, int y1) override;
    void DrawLine(int x0, int y0, int x1, int y1) override;
    void DrawPolyLine(int* x, int* y, int count) override;
    void DrawSetTextFont(HFont font) override;
    void DrawSetTextColor(int r, int g, int b, int a) override;
    void DrawSetTextColor(Color color) override;
    void DrawSetTextPos(int x, int y) override;
    void DrawGetTextPos(int& x, int& y) override;
    void DrawPrintText(const wchar_t* text, int length) override;
    void DrawUnicodeChar(wchar_t character) override;
    void DrawUnicodeCharAdd(wchar_t character) override;
    void DrawFlushText() override;
    IHTML* CreateHTMLWindow(IHTMLEvents* events, VPANEL context) override;
    void PaintHTMLWindow(IHTML* window) override;
    void DeleteHTMLWindow(IHTML* window) override;
    void DrawSetTextureFile(int id, const char* filename, int hardwareFilter, bool forceReload) override;
    void DrawSetTextureRGBA(int id, const unsigned char* rgba, int wide, int tall, int hardwareFilter,
                            bool forceReload) override;
    void DrawSetTexture(int id) override;
    void DrawGetTextureSize(int id, int& wide, int& tall) override;
    void DrawTexturedRect(int x0, int y0, int x1, int y1) override;
    bool IsTextureIDValid(int id) override;
    int CreateNewTextureID(bool procedural = false) override;
    void GetScreenSize(int& wide, int& tall) override;
    void SetAsTopMost(VPANEL panel, bool state) override;
    void BringToFront(VPANEL panel) override;
    void SetForegroundWindow(VPANEL panel) override;
    void SetPanelVisible(VPANEL panel, bool state) override;
    void SetMinimized(VPANEL panel, bool state) override;
    bool IsMinimized(VPANEL panel) override;
    void FlashWindow(VPANEL panel, bool state) override;
    void SetTitle(VPANEL panel, const wchar_t* title) override;
    void SetAsToolBar(VPANEL panel, bool state) override;
    void CreatePopup(VPANEL panel, bool minimized, bool showTaskbarIcon, bool disabled, bool mouseInput,
                     bool keyboardInput) override;
    void SwapBuffers(VPANEL panel) override;
    void Invalidate(VPANEL panel) override;
    void SetCursor(HCursor cursor) override;
    bool IsCursorVisible() override;
    void ApplyChanges() override;
    bool IsWithin(int x, int y) override;
    bool HasFocus() override;
    bool SupportsFeature(SurfaceFeature_e feature) override;
    void RestrictPaintToSinglePanel(VPANEL panel) override;
    void SetModalPanel(VPANEL panel) override;
    VPANEL GetModalPanel() override;
    void UnlockCursor() override;
    void LockCursor() override;
    void SetTranslateExtendedKeys(bool state) override;
    VPANEL GetTopmostPopup() override;
    void SetTopLevelFocus(VPANEL panel) override;
    HFont CreateFont() override;
    bool AddGlyphSetToFont(HFont font, const char* name, int tall, int weight, int blur, int scanlines, int flags,
                           int rangeMin, int rangeMax) override;
    bool AddCustomFontFile(const char* filename) override;
    int GetFontTall(HFont font) override;
    void GetCharABCwide(HFont font, int character, int& a, int& b, int& c) override;
    int GetCharacterWidth(HFont font, int character) override;
    void GetTextSize(HFont font, const wchar_t* text, int& wide, int& tall) override;
    VPANEL GetNotifyPanel() override;
    void SetNotifyIcon(VPANEL context, HTexture icon, VPANEL panel, const char* text) override;
    void PlaySound(const char* filename) override;
    int GetPopupCount() override;
    VPANEL GetPopup(int index) override;
    bool ShouldPaintChildPanel(VPANEL panel) override;
    bool RecreateContext(VPANEL panel) override;
    void AddPanel(VPANEL panel) override;
    void ReleasePanel(VPANEL panel) override;
    void MovePopupToFront(VPANEL panel) override;
    void MovePopupToBack(VPANEL panel) override;
    void SolveTraverse(VPANEL panel, bool forceApplySchemeSettings = false) override;
    void PaintTraverse(VPANEL panel) override;
    void EnableMouseCapture(VPANEL panel, bool state) override;
    void GetWorkspaceBounds(int& x, int& y, int& wide, int& tall) override;
    void GetAbsoluteWindowBounds(int& x, int& y, int& wide, int& tall) override;
    void GetProportionalBase(int& wide, int& tall) override;
    void CalculateMouseVisible() override;
    bool NeedKBInput() override;
    bool HasCursorPosFunctions() override;
    void SurfaceGetCursorPos(int& x, int& y) override;
    void SurfaceSetCursorPos(int x, int y) override;
    void DrawTexturedPolygon(VGuiVertex* vertices, int count) override;
    int GetFontAscent(HFont font, wchar_t character) override;
    void SetAllowHTMLJavaScript(bool state) override;
    void SetLanguage(const char* language) override;
    const char* GetLanguage() override;
    bool DeleteTextureByID(int id) override;
    void DrawUpdateRegionTextureBGRA(int id, int x, int y, const unsigned char* pixels, int wide, int tall) override;
    void DrawSetTextureBGRA(int id, const unsigned char* pixels, int wide, int tall) override;
    void CreateBrowser(VPANEL panel, IHTMLResponses* responses, bool popup, const char* userAgent) override;
    void RemoveBrowser(VPANEL panel, IHTMLResponses* responses) override;
    IHTMLChromeController* AccessChromeHTMLController() override;
    void DrawTexturedRectAdd(int x0, int y0, int x1, int y1) override;
    void SetSupportsEsc(bool state) override;
    int GetFontBlur(HFont font) override;
    bool IsAdditive(HFont font) override;
    void SetProportionalBase(int wide, int tall) override;
    void GetHDProportionalBase(int& wide, int& tall) override;
    void SetHDProportionalBase(int wide, int tall) override;

private:
    struct Context {
        HPanel panel = 0;
        int x = 0;
        int y = 0;
        SDL_Rect clip{};
    };
    struct CallbackOwner {
        HPanel panel = 0;
        HPanel window = 0;
        std::uint64_t generation = 0;
        bool hasWindow = false;
    };
    CallbackOwner TrackCallback(VPANEL panel);
    VPANEL ResolveCallback(const CallbackOwner& owner);
    bool Video();
    StandaloneWindow* Window(VPANEL panel);
    StandaloneWindow* Current();
    void ContextChanged();
    void MoveCursor(int x, int y);
    void ResetMouseButtons();
    void ReconcileSize(VPANEL panel, bool nativeEvent);
    void Event(const SDL_Event& event, StandaloneWindow& window, VPANEL panel);
    void TexturedRect(int x0, int y0, int x1, int y1, bool additive);
    void Glyph(wchar_t character, bool additive);
    bool ReadFile(const char* name, std::vector<unsigned char>& bytes, const char* pathId = nullptr);
    void Traverse(VPANEL panel, int phase, bool force);
    std::map<HPanel, std::shared_ptr<StandaloneWindow>> windows_;
    std::vector<HPanel> popups_;
    std::map<int, RasterImage> textures_;
    std::map<std::pair<HFont, wchar_t>, int> glyphs_;
    std::set<HPanel> panels_;
    std::vector<Context> contexts_;
    Context context_;
    VPANEL embedded_ = 0;
    HPanel captured_ = 0;
    HPanel focused_ = 0;
    HPanel restricted_ = 0;
    HPanel notify_ = 0;
    std::array<HPanel, MOUSE_LAST> mouseWindows_{};
    std::array<HPanel, MOUSE_LAST> mouseTargets_{};
    SDL_Keymod eventModifiers_ = SDL_KMOD_NONE;
    std::uint64_t generation_ = 0;
    int nextTexture_ = 1;
    int texture_ = 0;
    HFont font_ = 0;
    Color color_{OpaqueAlpha, OpaqueAlpha, OpaqueAlpha, OpaqueAlpha};
    Color textColor_{OpaqueAlpha, OpaqueAlpha, OpaqueAlpha, OpaqueAlpha};
    int textX_ = 0;
    int textY_ = 0;
    int cursorX_ = 0;
    int cursorY_ = 0;
    bool cursorPositionValid_ = false;
    int baseWidth_ = DefaultWidth;
    int baseHeight_ = DefaultHeight;
    int hdWidth_ = HdWidth;
    int hdHeight_ = HdHeight;
    bool video_ = false;
    bool audio_ = false;
    bool cursorLocked_ = false;
    bool cursorVisible_ = true;
    bool supportsEscape_ = true;
    bool extendedKeys_ = true;
    std::string language_ = "english";
    std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> cursor_{nullptr, SDL_DestroyCursor};
    SDL_SystemCursor cursorShape_ = SDL_SYSTEM_CURSOR_DEFAULT;
    SDL_AudioStream* sound_ = nullptr;
};
CSDLSurface standaloneSurface;
} // namespace
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSDLSurface, ISurface, "VGUI_Surface026", standaloneSurface);

CSDLSurface::~CSDLSurface() {
    // Cross-TU font singletons may already be destroyed at process teardown.
    // Explicit Shutdown owns font retirement; C++ members own emergency SDL cleanup.
    SDL_DestroyAudioStream(sound_);
    cursor_.reset();
    windows_.clear();
    if (audio_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    if (video_) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

bool CSDLSurface::Video() {
    if (video_) {
        return true;
    }
    if (!SDL_IsMainThread() || !cse::ConfigureKeyboardMapping() || !SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return false;
    }
    video_ = true;
    eventModifiers_ = SDL_GetModState();
    return true;
}

StandaloneWindow* CSDLSurface::Window(VPANEL panel) {
    for (VPanel* current = Panel(panel); current; current = current->GetParent()) {
        auto found = windows_.find(g_pIVgui->PanelToHandle(reinterpret_cast<VPANEL>(current)));
        if (found != windows_.end()) {
            return found->second.get();
        }
    }
    return nullptr;
}

StandaloneWindow* CSDLSurface::Current() {
    const auto found = windows_.find(context_.panel);
    return found != windows_.end() ? found->second.get() : nullptr;
}

CSDLSurface::CallbackOwner CSDLSurface::TrackCallback(VPANEL panel) {
    CallbackOwner owner;
    owner.panel = g_pIVgui->PanelToHandle(panel);
    if (auto* window = Window(panel)) {
        owner.window = window->Panel();
        owner.generation = window->Generation();
        owner.hasWindow = true;
    }
    return owner;
}

VPANEL CSDLSurface::ResolveCallback(const CallbackOwner& owner) {
    const VPANEL panel = g_pIVgui->HandleToPanel(owner.panel);
    if (!panel) {
        return 0;
    }
    // Packed HPanel zero is valid for the first panel, not an absent-owner sentinel.
    if (owner.hasWindow) {
        const auto found = windows_.find(owner.window);
        if (found == windows_.end() || found->second->Generation() != owner.generation ||
            Window(panel) != found->second.get()) {
            return 0;
        }
    }
    return panel;
}

void CSDLSurface::Shutdown() {
    ResetMouseButtons();
    contexts_.clear();
    context_ = {};
    glyphs_.clear();
    textures_.clear();
    windows_.clear();
    popups_.clear();
    panels_.clear();
    SDL_DestroyAudioStream(sound_);
    sound_ = nullptr;
    if (audio_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audio_ = false;
    }
    cursor_.reset();
    cursorLocked_ = false;
    cursorVisible_ = true;
    if (video_) {
        SDL_CaptureMouse(false);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        video_ = false;
    }
    FontManager().ClearAllFonts();
    cse::fonts::ReleaseMacCustomFonts(this);
    embedded_ = 0;
    focused_ = 0;
    captured_ = 0;
    restricted_ = 0;
    notify_ = 0;
    cursorPositionValid_ = false;
    eventModifiers_ = SDL_KMOD_NONE;
}

VPANEL CSDLSurface::GetEmbeddedPanel() {
    return embedded_;
}

void CSDLSurface::SetEmbeddedPanel(VPANEL panel) {
    embedded_ = panel;
    if (panel) {
        CreatePopup(panel, false, false, false, false, false);
        FontManager().SetLanguage(language_.c_str());
    }
}

void CSDLSurface::PushMakeCurrent(VPANEL panel, bool useInsets) {
    contexts_.push_back(context_);
    context_ = {};
    auto* window = Window(panel);
    if (!window) {
        return;
    }
    const auto owner = TrackCallback(panel);
    context_.panel = window->Panel();
    int windowX = 0;
    int windowY = 0;
    Panel(g_pIVgui->HandleToPanel(window->Panel()))->GetAbsPos(windowX, windowY);
    int x = 0;
    int y = 0;
    Panel(panel)->GetAbsPos(x, y);
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    if (useInsets) {
        Panel(panel)->Client()->GetInset(left, top, right, bottom);
        panel = ResolveCallback(owner);
        if (!panel) {
            context_ = {};
            return;
        }
    }
    context_.x = x - windowX + left;
    context_.y = y - windowY + top;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    Panel(panel)->Client()->GetClipRect(x0, y0, x1, y1);
    if (!ResolveCallback(owner)) {
        context_ = {};
        return;
    }
    context_.clip = {x0 - windowX, y0 - windowY, std::max(0, x1 - x0 - right), std::max(0, y1 - y0 - bottom)};
    ContextChanged();
}

void CSDLSurface::PopMakeCurrent(VPANEL) {
    context_ = contexts_.empty() ? Context{} : contexts_.back();
    if (!contexts_.empty()) {
        contexts_.pop_back();
    }
    ContextChanged();
}

void CSDLSurface::ContextChanged() {
    if (auto* window = Current()) {
        SDL_SetRenderClipRect(window->raster.Renderer(), &context_.clip);
    }
}

void CSDLSurface::DrawSetColor(int r, int g, int b, int a) {
    color_ = Color(r, g, b, a);
}

void CSDLSurface::DrawSetColor(Color color) {
    color_ = color;
}

void CSDLSurface::DrawFilledRect(int x0, int y0, int x1, int y1) {
    if (auto* window = Current()) {
        auto* renderer = window->raster.Renderer();
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, color_[0], color_[1], color_[2], color_[3]);
        const SDL_FRect rectangle{static_cast<float>(x0 + context_.x), static_cast<float>(y0 + context_.y),
                                  static_cast<float>(x1 - x0), static_cast<float>(y1 - y0)};
        SDL_RenderFillRect(renderer, &rectangle);
    }
}

void CSDLSurface::DrawOutlinedRect(int x0, int y0, int x1, int y1) {
    DrawFilledRect(x0, y0, x1, y0 + 1);
    DrawFilledRect(x0, y1 - 1, x1, y1);
    DrawFilledRect(x0, y0 + 1, x0 + 1, y1 - 1);
    DrawFilledRect(x1 - 1, y0 + 1, x1, y1 - 1);
}

void CSDLSurface::DrawLine(int x0, int y0, int x1, int y1) {
    if (auto* window = Current()) {
        auto* renderer = window->raster.Renderer();
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, color_[0], color_[1], color_[2], color_[3]);
        SDL_RenderLine(renderer, static_cast<float>(x0 + context_.x), static_cast<float>(y0 + context_.y),
                       static_cast<float>(x1 + context_.x), static_cast<float>(y1 + context_.y));
    }
}

void CSDLSurface::DrawPolyLine(int* x, int* y, int count) {
    if (!x || !y || count < 2) {
        return;
    }
    for (int index = 1; index < count; ++index) {
        DrawLine(x[index - 1], y[index - 1], x[index], y[index]);
    }
}

void CSDLSurface::DrawSetTextFont(HFont font) {
    font_ = font;
}

void CSDLSurface::DrawSetTextColor(int r, int g, int b, int a) {
    textColor_ = Color(r, g, b, a);
}

void CSDLSurface::DrawSetTextColor(Color color) {
    textColor_ = color;
}

void CSDLSurface::DrawSetTextPos(int x, int y) {
    textX_ = x;
    textY_ = y;
}

void CSDLSurface::DrawGetTextPos(int& x, int& y) {
    x = textX_;
    y = textY_;
}

void CSDLSurface::Glyph(wchar_t character, bool additive) {
    const int previousTexture = texture_;
    auto* font = FontManager().GetFontForChar(font_, character);
    if (!font) {
        return;
    }
    int a = 0;
    int b = 0;
    int c = 0;
    font->GetCharABCWidths(character, a, b, c);
    const int width = std::max(1, font->GetUnderlined() ? a + b + c : b);
    const int height = font->GetHeight();
    if (width > cse::vgui::MaximumRasterDimension || height <= 0 || height > cse::vgui::MaximumRasterDimension) {
        return;
    }
    const auto key = std::make_pair(font_, character);
    auto found = glyphs_.find(key);
    int glyph = 0;
    if (found == glyphs_.end()) {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * cse::vgui::PixelChannels);
        font->GetCharRGBA(character, 0, 0, width, height, pixels.data());
        glyph = CreateNewTextureID(true);
        DrawSetTextureRGBA(glyph, pixels.data(), width, height, 0, false);
        glyphs_[key] = glyph;
    } else {
        glyph = found->second;
    }
    const Color previousColor = color_;
    texture_ = glyph;
    color_ = textColor_;
    TexturedRect(textX_ + a, textY_, textX_ + a + width, textY_ + height, additive);
    texture_ = previousTexture;
    color_ = previousColor;
    textX_ += a + b + c;
}

void CSDLSurface::DrawPrintText(const wchar_t* text, int length) {
    if (!text || length < 0) {
        return;
    }
    for (int index = 0; index < length; ++index) {
        Glyph(text[index], IsAdditive(font_));
    }
}

void CSDLSurface::DrawUnicodeChar(wchar_t character) {
    Glyph(character, false);
}

void CSDLSurface::DrawUnicodeCharAdd(wchar_t character) {
    Glyph(character, true);
}

void CSDLSurface::DrawFlushText() {
    // Text is submitted synchronously to the renderer; no borrowed glyph batch remains.
}

IHTML* CSDLSurface::CreateHTMLWindow(IHTMLEvents*, VPANEL) {
    return nullptr; // The retained standalone implementation has no embedded browser.
}

void CSDLSurface::PaintHTMLWindow(IHTML*) {
    SDL_Log("Standalone VGUI has no embedded HTML window");
}

void CSDLSurface::DeleteHTMLWindow(IHTML*) {
    // CreateHTMLWindow never transfers an HTML object to this owner.
}

void CSDLSurface::CreateBrowser(VPANEL, IHTMLResponses*, bool, const char*) {
    SDL_Log("Standalone VGUI has no embedded browser; native help uses ISystem::ShellExecute");
}

void CSDLSurface::RemoveBrowser(VPANEL, IHTMLResponses*) {
    // No embedded browser can be created by this surface.
}

IHTMLChromeController* CSDLSurface::AccessChromeHTMLController() {
    return nullptr;
}

void CSDLSurface::SetAllowHTMLJavaScript(bool) {
    // No embedded HTML capability is advertised.
}

void CSDLSurface::DrawSetTextureRGBA(int id, const unsigned char* pixels, int wide, int tall, int filter, bool) {
    auto found = textures_.find(id);
    if (found != textures_.end() && found->second.Assign(pixels, wide, tall, false, filter != 0)) {
        texture_ = id;
    }
}

void CSDLSurface::DrawSetTextureBGRA(int id, const unsigned char* pixels, int wide, int tall) {
    auto found = textures_.find(id);
    if (found != textures_.end() && found->second.Assign(pixels, wide, tall, true, false)) {
        texture_ = id;
    }
}

void CSDLSurface::DrawUpdateRegionTextureBGRA(int id, int x, int y, const unsigned char* pixels, int wide, int tall) {
    auto found = textures_.find(id);
    if (found != textures_.end()) {
        found->second.UpdateFromBacking(x, y, pixels, wide, tall);
    }
}

void CSDLSurface::DrawSetTexture(int id) {
    texture_ = IsTextureIDValid(id) ? id : 0;
}

void CSDLSurface::DrawGetTextureSize(int id, int& wide, int& tall) {
    const auto found = textures_.find(id);
    wide = found == textures_.end() ? 0 : found->second.width;
    tall = found == textures_.end() ? 0 : found->second.height;
}

bool CSDLSurface::IsTextureIDValid(int id) {
    return textures_.find(id) != textures_.end();
}

int CSDLSurface::CreateNewTextureID(bool) {
    if (nextTexture_ == INT_MAX) {
        return 0;
    }
    const int id = nextTexture_++;
    textures_.emplace(id, RasterImage{});
    return id;
}

bool CSDLSurface::DeleteTextureByID(int id) {
    for (auto& window : windows_) {
        window.second->raster.DeleteTexture(id);
    }
    if (texture_ == id) {
        texture_ = 0;
    }
    return textures_.erase(id) != 0;
}

void CSDLSurface::TexturedRect(int x0, int y0, int x1, int y1, bool additive) {
    auto* window = Current();
    const auto found = textures_.find(texture_);
    if (!window || found == textures_.end()) {
        return;
    }
    auto* texture = window->raster.Texture(texture_, found->second);
    if (!texture) {
        return;
    }
    SDL_SetTextureColorMod(texture, color_[0], color_[1], color_[2]);
    SDL_SetTextureAlphaMod(texture, color_[3]);
    SDL_SetTextureBlendMode(texture, additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
    const SDL_FRect rectangle{static_cast<float>(x0 + context_.x), static_cast<float>(y0 + context_.y),
                              static_cast<float>(x1 - x0), static_cast<float>(y1 - y0)};
    SDL_RenderTexture(window->raster.Renderer(), texture, nullptr, &rectangle);
}

void CSDLSurface::DrawTexturedRect(int x0, int y0, int x1, int y1) {
    TexturedRect(x0, y0, x1, y1, false);
}

void CSDLSurface::DrawTexturedRectAdd(int x0, int y0, int x1, int y1) {
    TexturedRect(x0, y0, x1, y1, true);
}

void CSDLSurface::DrawTexturedPolygon(VGuiVertex* vertices, int count) {
    auto* window = Current();
    const auto found = textures_.find(texture_);
    constexpr int MaximumPolygonVertices = 4096;
    if (!window || !vertices || count < 3 || count > MaximumPolygonVertices || found == textures_.end()) {
        return;
    }
    auto* texture = window->raster.Texture(texture_, found->second);
    if (!texture) {
        return;
    }
    std::vector<SDL_Vertex> points(static_cast<std::size_t>(count));
    std::vector<int> indices;
    const SDL_FColor color{color_[0] / static_cast<float>(OpaqueAlpha), color_[1] / static_cast<float>(OpaqueAlpha),
                           color_[2] / static_cast<float>(OpaqueAlpha), color_[3] / static_cast<float>(OpaqueAlpha)};
    for (int index = 0; index < count; ++index) {
        points[index] = {
            {static_cast<float>(vertices[index].x + context_.x), static_cast<float>(vertices[index].y + context_.y)},
            color,
            {vertices[index].u, vertices[index].v}};
        if (index >= 2) {
            indices.push_back(0);
            indices.push_back(index - 1);
            indices.push_back(index);
        }
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(window->raster.Renderer(), texture, points.data(), count, indices.data(),
                       static_cast<int>(indices.size()));
}

bool CSDLSurface::ReadFile(const char* name, std::vector<unsigned char>& bytes, const char* pathId) {
    if (!g_pFileSystem || !name) {
        return false;
    }
    FileHandle_t file = g_pFileSystem->Open(name, "rb", pathId);
    if (!file) {
        return false;
    }
    const unsigned int size = g_pFileSystem->Size(file);
    bool success = false;
    if (size > 0 && size <= MaximumFileBytes) {
        bytes.resize(size);
        success = g_pFileSystem->Read(bytes.data(), static_cast<int>(size), file) == static_cast<int>(size);
    }
    g_pFileSystem->Close(file);
    return success;
}

void CSDLSurface::DrawSetTextureFile(int id, const char* filename, int filter, bool force) {
    auto found = textures_.find(id);
    if (found == textures_.end() || !filename) {
        return;
    }
    const std::string request(filename);
    if (!force && found->second.filename == request && !found->second.pixels.empty()) {
        texture_ = id;
        return;
    }
    std::vector<unsigned char> file;
    const std::string webpPath = cse::image::WebPPath(filename);
    if (ReadFile(webpPath.c_str(), file, "SKIN") || ReadFile(webpPath.c_str(), file)) {
        cse::image::Image image;
        if (cse::image::DecodeWebP(file.data(), file.size(), image)
            && found->second.Assign(image.rgba.data(), image.width, image.height, false, filter != 0)) {
            found->second.filename = request;
            texture_ = id;
            return;
        }
    }
    if (cse::image::HasWebPExtension(filename)) {
        return;
    }
    const std::string stem = webpPath.substr(0, webpPath.size() - 5);
    // Preserve the requested TGA spelling on case-sensitive filesystems.
    const std::string path = request.size() == stem.size() + 4 ? request : stem + ".tga";
    if (!ReadFile(path.c_str(), file, "SKIN") && !ReadFile(path.c_str(), file)) {
        const std::string bitmapPath = stem + ".bmp";
        if (!ReadFile(bitmapPath.c_str(), file, "SKIN") && !ReadFile(bitmapPath.c_str(), file)) {
            return;
        }
        std::vector<unsigned char> rgba;
        int width = 0;
        int height = 0;
        if (DecodeIndexedBitmap(file, rgba, width, height)
            && found->second.Assign(rgba.data(), width, height, false, filter != 0)) {
            found->second.filename = request;
            texture_ = id;
        }
        return;
    }
    // TGA uses an 18-byte serialized header; never read a native padded structure.
    constexpr std::size_t TgaHeaderBytes = 18;
    constexpr unsigned char UncompressedTrueColor = 2;
    constexpr unsigned char RunLengthTrueColor = 10;
    constexpr unsigned char TopOrigin = 0x20;
    constexpr unsigned char RightOrigin = 0x10;
    constexpr unsigned char RgbBits = 24;
    constexpr unsigned char RgbaBits = 32;
    constexpr unsigned int BitsPerByte = 8;
    constexpr unsigned char RunLengthMask = 0x7f;
    constexpr unsigned char RunLengthFlag = 0x80;
    if (file.size() < TgaHeaderBytes || file[1] != 0 ||
        (file[2] != UncompressedTrueColor && file[2] != RunLengthTrueColor) ||
        (file[16] != RgbBits && file[16] != RgbaBits)) {
        return;
    }
    const int width = file[12] | (file[13] << BitsPerByte);
    const int height = file[14] | (file[15] << BitsPerByte);
    if (width <= 0 || height <= 0 || width > cse::vgui::MaximumRasterDimension ||
        height > cse::vgui::MaximumRasterDimension) {
        return;
    }
    const std::size_t count = static_cast<std::size_t>(width) * height;
    if (count > MaximumFileBytes / cse::vgui::PixelChannels) {
        return;
    }
    std::vector<unsigned char> rgba(count * cse::vgui::PixelChannels);
    std::size_t source = TgaHeaderBytes + file[0];
    std::size_t pixel = 0;
    const std::size_t channels = file[16] / BitsPerByte;
    while (pixel < count) {
        std::size_t run = 1;
        bool repeated = false;
        if (file[2] == RunLengthTrueColor) {
            if (source >= file.size()) {
                return;
            }
            const unsigned char packet = file[source++];
            run = (packet & RunLengthMask) + 1;
            repeated = (packet & RunLengthFlag) != 0;
        }
        if (run > count - pixel) {
            return;
        }
        for (std::size_t index = 0; index < run; ++index) {
            if (source > file.size() || channels > file.size() - source) {
                return;
            }
            std::size_t x = pixel % width;
            std::size_t y = pixel / width;
            if (!(file[17] & TopOrigin)) {
                y = height - 1 - y;
            }
            if (file[17] & RightOrigin) {
                x = width - 1 - x;
            }
            const std::size_t destination = (y * width + x) * cse::vgui::PixelChannels;
            rgba[destination] = file[source + 2];
            rgba[destination + 1] = file[source + 1];
            rgba[destination + 2] = file[source];
            rgba[destination + 3] = channels == 4 ? file[source + 3] : OpaqueAlpha;
            ++pixel;
            if (!repeated || index + 1 == run) {
                source += channels;
            }
        }
    }
    if (found->second.Assign(rgba.data(), width, height, false, filter != 0)) {
        found->second.filename = request;
        texture_ = id;
    }
}

void CSDLSurface::CreatePopup(VPANEL panel, bool minimized, bool taskbar, bool disabled, bool mouse, bool keyboard) {
    if (!panel || !g_pIVgui || !Video()) {
        return;
    }
    const HPanel handle = g_pIVgui->PanelToHandle(panel);
    if (windows_.count(handle)) {
        return;
    }
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    Panel(panel)->GetPos(x, y);
    Panel(panel)->GetSize(width, height);
    auto window = std::make_shared<StandaloneWindow>();
    if (!window->Open(handle, ++generation_, x, y, std::max(1, width), std::max(1, height), !taskbar)) {
        SDL_Log("VGUI popup creation failed: %s", SDL_GetError());
        return;
    }
    window->disabled = disabled;
    Panel(panel)->SetPopup(true);
    Panel(panel)->SetMouseInputEnabled(mouse);
    Panel(panel)->SetKeyBoardInputEnabled(keyboard);
    if (!Panel(panel)->GetParent() && panel != embedded_) {
        Panel(panel)->SetParent(Panel(embedded_));
    }
    windows_.emplace(handle, std::move(window));
    popups_.push_back(handle);
    SetMinimized(panel, minimized);
    SetPanelVisible(panel, panel != embedded_ && Panel(panel)->IsVisible());
}

void CSDLSurface::AddPanel(VPANEL panel) {
    panels_.insert(g_pIVgui->PanelToHandle(panel));
}

void CSDLSurface::ReleasePanel(VPANEL panel) {
    const HPanel handle = g_pIVgui->PanelToHandle(panel);
    if (std::find(mouseWindows_.begin(), mouseWindows_.end(), handle) != mouseWindows_.end() ||
        std::find(mouseTargets_.begin(), mouseTargets_.end(), handle) != mouseTargets_.end()) {
        ResetMouseButtons();
    }
    if (captured_ == handle) {
        SDL_CaptureMouse(false);
        captured_ = 0;
    }
    if (focused_ == handle) {
        focused_ = 0;
    }
    if (restricted_ == handle) {
        restricted_ = 0;
    }
    if (embedded_ == panel) {
        embedded_ = 0;
    }
    popups_.erase(std::remove(popups_.begin(), popups_.end(), handle), popups_.end());
    // Remove the route before releasing SDL resources; old queued IDs resolve to nothing.
    windows_.erase(handle);
    panels_.erase(handle);
    if (context_.panel == handle) {
        context_ = {};
    }
    for (auto& context : contexts_) {
        if (context.panel == handle) {
            context = {};
        }
    }
}

int CSDLSurface::GetPopupCount() {
    return static_cast<int>(popups_.size());
}

VPANEL CSDLSurface::GetPopup(int index) {
    return index >= 0 && static_cast<std::size_t>(index) < popups_.size() ? g_pIVgui->HandleToPanel(popups_[index]) : 0;
}

void CSDLSurface::MovePopupToFront(VPANEL panel) {
    const HPanel handle = g_pIVgui->PanelToHandle(panel);
    auto found = std::find(popups_.begin(), popups_.end(), handle);
    if (found != popups_.end()) {
        popups_.erase(found);
        popups_.push_back(handle);
    }
}

void CSDLSurface::MovePopupToBack(VPANEL panel) {
    const HPanel handle = g_pIVgui->PanelToHandle(panel);
    auto found = std::find(popups_.begin(), popups_.end(), handle);
    if (found != popups_.end()) {
        popups_.erase(found);
        popups_.insert(popups_.begin(), handle);
    }
}

void CSDLSurface::SetAsTopMost(VPANEL panel, bool state) {
    if (auto* window = Window(panel)) {
        SDL_SetWindowAlwaysOnTop(window->Window(), state);
    }
}

void CSDLSurface::BringToFront(VPANEL panel) {
    if (auto* window = Window(panel)) {
        SDL_RaiseWindow(window->Window());
        MovePopupToFront(g_pIVgui->HandleToPanel(window->Panel()));
    }
}

void CSDLSurface::SetForegroundWindow(VPANEL panel) {
    BringToFront(panel);
}

void CSDLSurface::SetTopLevelFocus(VPANEL panel) {
    if (auto* window = Window(panel)) {
        focused_ = window->Panel();
        BringToFront(g_pIVgui->HandleToPanel(focused_));
    }
}

void CSDLSurface::SetPanelVisible(VPANEL panel, bool state) {
    // VPanel calls this for every child; only the panel that owns a native popup
    // may change that window's visibility (matching retained Plat() semantics).
    const auto found = windows_.find(g_pIVgui->PanelToHandle(panel));
    if (found == windows_.end()) {
        return;
    }
    auto& owner = *found->second;
    state = state && panel != embedded_;
    for (VPanel* ancestor = Panel(panel)->GetParent(); state && ancestor; ancestor = ancestor->GetParent()) {
        state = ancestor->IsVisible();
    }
    if (state) {
        // Cocoa's addChildWindow orders even a hidden child onto the screen.
        // Show deliberately before attaching, and never attach hidden menus.
        SDL_ShowWindow(owner.Window());
        SDL_Window* nativeParent = nullptr;
        if (Panel(panel)->GetParent()) {
            if (auto* parent = Window(reinterpret_cast<VPANEL>(Panel(panel)->GetParent()))) {
                if (g_pIVgui->HandleToPanel(parent->Panel()) != embedded_) {
                    nativeParent = parent->Window();
                }
            }
        }
        if (SDL_GetWindowParent(owner.Window()) != nativeParent) {
            SDL_SetWindowParent(owner.Window(), nativeParent);
        }
    } else {
        if (owner.modal) {
            SDL_SetWindowModal(owner.Window(), false);
            owner.modal = false;
        }
        if (SDL_GetWindowParent(owner.Window())) {
            SDL_SetWindowParent(owner.Window(), nullptr);
        }
        SDL_HideWindow(owner.Window());
    }
}

void CSDLSurface::SetMinimized(VPANEL panel, bool state) {
    if (auto* window = Window(panel)) {
        if (state) {
            SDL_MinimizeWindow(window->Window());
        } else {
            SDL_RestoreWindow(window->Window());
        }
    }
}

bool CSDLSurface::IsMinimized(VPANEL panel) {
    auto* window = Window(panel);
    return window && (SDL_GetWindowFlags(window->Window()) & SDL_WINDOW_MINIMIZED) != 0;
}

void CSDLSurface::FlashWindow(VPANEL panel, bool state) {
    if (auto* window = Window(panel)) {
        SDL_FlashWindow(window->Window(), state ? SDL_FLASH_UNTIL_FOCUSED : SDL_FLASH_CANCEL);
    }
}

void CSDLSurface::SetTitle(VPANEL panel, const wchar_t* title) {
    if (auto* window = Window(panel)) {
        if (title) {
            const std::size_t length = std::wcslen(title);
            constexpr std::size_t MaximumTitleCells = 4096;
            if (length <= MaximumTitleCells) {
                std::vector<char> utf8((length + 1) * cse::legacy_text::detail::MaximumUtf8Bytes);
                if (cse::legacy_text::WideCellsToUtf8(title, utf8.data(), static_cast<int>(utf8.size())) > 0) {
                    SDL_SetWindowTitle(window->Window(), utf8.data());
                }
            }
        }
    }
}

void CSDLSurface::SetAsToolBar(VPANEL panel, bool state) {
    // Utility classification is immutable in SDL: it is selected at CreatePopup.
    // Tool windows are borderless and owned by their parent's native window.
    if (auto* window = Window(panel)) {
        SDL_SetWindowBordered(window->Window(), false);
        if (state && !(SDL_GetWindowFlags(window->Window()) & SDL_WINDOW_HIDDEN) && Panel(panel)->GetParent()) {
            if (auto* parent = Window(reinterpret_cast<VPANEL>(Panel(panel)->GetParent()))) {
                SDL_SetWindowParent(window->Window(), parent->Window());
            }
        } else if (!state) {
            SDL_SetWindowParent(window->Window(), nullptr);
        }
    }
}

void CSDLSurface::SwapBuffers(VPANEL panel) {
    // Retained controls swap after every child traversal. Only a native popup
    // owner may publish the shared backing, once its complete subtree is drawn.
    const auto found = windows_.find(g_pIVgui->PanelToHandle(panel));
    if (found != windows_.end()) {
        found->second->raster.Present();
        ContextChanged();
    }
}

void CSDLSurface::Invalidate(VPANEL panel) {
    if (auto* window = Window(panel)) {
        window->dirty = true;
    }
}

bool CSDLSurface::RecreateContext(VPANEL panel) {
    auto* window = Window(panel);
    if (!window) {
        return false;
    }
    int width = 0;
    int height = 0;
    Panel(g_pIVgui->HandleToPanel(window->Panel()))->GetSize(width, height);
    return window->Resize(width, height);
}

void CSDLSurface::ReconcileSize(VPANEL panel, bool nativeEvent) {
    const auto callback = TrackCallback(panel);
    const auto found = windows_.find(callback.window);
    if (!callback.hasWindow || found == windows_.end() || found->second->SizeRequestInFlight()) {
        return;
    }
    // SyncWindow pumps native callbacks. Retiring a panel must remove routing
    // immediately, but SDL's in-call window data and video subsystem must survive
    // until the native call returns. Destruction order releases the owner first.
    struct VideoReference {
        bool retained = false;
        ~VideoReference() {
            if (retained) {
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
            }
        }
    } videoReference;
    const auto nativeLease = found->second;
    auto* window = nativeLease.get();
    int requestedWidth = 0;
    int requestedHeight = 0;
    Panel(panel)->GetSize(requestedWidth, requestedHeight);
    const bool newIntent = window->HasSizeIntent(requestedWidth, requestedHeight);
    if (!newIntent && !nativeEvent && !window->BackingResizePending()) {
        return;
    }
    videoReference.retained = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (!videoReference.retained) {
        return;
    }
    if (!newIntent && !nativeEvent) {
        // Native geometry already settled. Retry only the independent backing
        // allocation, using current retained dimensions; never resubmit the request.
        if (window->Resize(requestedWidth, requestedHeight)) {
            panel = ResolveCallback(callback);
            if (panel) {
                Invalidate(panel);
            }
        }
        return;
    }
    if (newIntent) {
        window->BeginSizeRequest(requestedWidth, requestedHeight);
        // Standalone popups never request native fullscreen/maximize. SDL may defer
        // a size indefinitely in those states, even when SyncWindow returns true.
        constexpr SDL_WindowFlags UnsupportedSizeStates = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED;
        const SDL_WindowID id = window->Id();
        const bool supported = (SDL_GetWindowFlags(window->Window()) & UnsupportedSizeStates) == 0;
        const bool submitted = supported && SDL_SetWindowSize(window->Window(), requestedWidth, requestedHeight);
        panel = ResolveCallback(callback);
        if (!panel) {
            return;
        }
        window = Window(panel);
        // SDL 3.4.20 documents this finite completion boundary; Cocoa can wait up
        // to 2.5 seconds during a native transition. Never synchronize unchanged
        // requests or infer completion from requested-size equality.
        const bool synchronized = submitted && SDL_SyncWindow(window->Window());
        panel = ResolveCallback(callback);
        if (!panel) {
            return;
        }
        window = Window(panel);
        const bool completed = synchronized &&
            (SDL_GetWindowFlags(window->Window()) & UnsupportedSizeStates) == 0;
        window->EndSizeRequest(completed);
        if (!completed) {
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO,
                        "VGUI size request rejected: window=%u requested=%dx%d supported=%d submitted=%d: %s",
                        id, requestedWidth, requestedHeight, supported, submitted, SDL_GetError());
        }
        int currentWidth = 0;
        int currentHeight = 0;
        Panel(panel)->GetSize(currentWidth, currentHeight);
        if (!window->MatchesSizeRequest(currentWidth, currentHeight)) {
            // A nested frame/callback made newer intent. Leave its retained value
            // intact; the next event or ApplyChanges will submit it independently.
            return;
        }
        // Failure is terminal for this request, not an acknowledgement. Settle to
        // live geometry, including denial/clamp, without resubmitting the fallback.
        // A later real native resize or a new retained request remains admissible.
    }
    int nativeWidth = 0;
    int nativeHeight = 0;
    if (!SDL_GetWindowSize(window->Window(), &nativeWidth, &nativeHeight)) {
        return;
    }
    int minimumWidth = 0;
    int minimumHeight = 0;
    Panel(panel)->GetMinimumSize(minimumWidth, minimumHeight);
    const int acceptedWidth = std::max(nativeWidth, minimumWidth);
    const int acceptedHeight = std::max(nativeHeight, minimumHeight);
    // VPanel clamps to its own minimum before the callback. That automatic clamp
    // is not a new request, even when the WM imposes an incompatible smaller size.
    window->AcceptRetainedSize(acceptedWidth, acceptedHeight);
    Panel(panel)->SetSize(nativeWidth, nativeHeight);
    panel = ResolveCallback(callback);
    if (!panel) {
        return;
    }
    int retainedWidth = 0;
    int retainedHeight = 0;
    Panel(panel)->GetSize(retainedWidth, retainedHeight);
    if (retainedWidth != acceptedWidth || retainedHeight != acceptedHeight) {
        // OnSizeChanged may request another size, replace the owner, or run a
        // nested frame. Never resize the backing from the pre-callback snapshot.
        return;
    }
    Window(panel)->Resize(retainedWidth, retainedHeight);
    Invalidate(panel);
}

void CSDLSurface::ApplyChanges() {
    const auto handles = popups_;
    for (HPanel handle : handles) {
        VPANEL panel = g_pIVgui->HandleToPanel(handle);
        if (!panel || windows_.find(handle) == windows_.end()) {
            continue;
        }
        const auto callback = TrackCallback(panel);
        ReconcileSize(panel, false);
        panel = ResolveCallback(callback);
        if (!panel) {
            continue;
        }
        int x = 0;
        int y = 0;
        Panel(panel)->GetPos(x, y);
        Window(panel)->SynchronizePosition(x, y);
        panel = ResolveCallback(callback);
        if (!panel) {
            continue;
        }
        SetPanelVisible(panel, Panel(panel)->IsVisible());
        panel = ResolveCallback(callback);
        if (!panel) {
            continue;
        }
        auto* window = Window(panel);
        const auto* modal = Window(g_pInput->GetAppModalSurface());
        const bool desiredModal = modal == window && SDL_GetWindowParent(window->Window()) != nullptr;
        if (window->modal != desiredModal && SDL_SetWindowModal(window->Window(), desiredModal)) {
            panel = ResolveCallback(callback);
            if (panel) {
                Window(panel)->modal = desiredModal;
            }
        }
    }
}

void CSDLSurface::Traverse(VPANEL panel, int phase, bool force) {
    if (!panel || !Panel(panel)->Client()) {
        return;
    }
    const auto owner = TrackCallback(panel);
    std::vector<HPanel> children;
    for (int index = 0; index < Panel(panel)->GetChildCount(); ++index) {
        children.push_back(g_pIVgui->PanelToHandle(reinterpret_cast<VPANEL>(Panel(panel)->GetChild(index))));
    }
    if (phase == 1) {
        Panel(panel)->Client()->Think();
    } else if (phase == 2) {
        Panel(panel)->Solve();
    }
    panel = ResolveCallback(owner);
    if (!panel) {
        return;
    }
    for (HPanel child : children) {
        const VPANEL live = g_pIVgui->HandleToPanel(child);
        if (live && (Panel(live)->IsVisible() || (phase == 0 && force))) {
            Traverse(live, phase, force);
            panel = ResolveCallback(owner);
            if (!panel) {
                return;
            }
        }
    }
    if (phase == 0) {
        Panel(panel)->Client()->PerformApplySchemeSettings();
    }
}

void CSDLSurface::SolveTraverse(VPANEL panel, bool force) {
    const auto owner = TrackCallback(panel);
    Traverse(panel, 0, force);
    panel = ResolveCallback(owner);
    if (panel && Panel(panel)->IsVisible()) {
        Traverse(panel, 1, force);
        panel = ResolveCallback(owner);
        if (panel) {
            Traverse(panel, 2, force);
        }
    }
}

void CSDLSurface::PaintTraverse(VPANEL panel) {
    if (!panel) {
        return;
    }
    const auto owner = TrackCallback(panel);
    auto* window = Window(panel);
    const bool force = window && window->dirty;
    if (window) {
        window->dirty = false;
    }
    Panel(panel)->Client()->PaintTraverse(force, true);
    panel = ResolveCallback(owner);
    if (panel) {
        SwapBuffers(panel);
    }
}

bool CSDLSurface::ShouldPaintChildPanel(VPANEL panel) {
    if (!panel || Panel(panel)->IsPopup()) {
        return false;
    }
    const VPANEL restricted = g_pIVgui->HandleToPanel(restricted_);
    return !restricted || panel == restricted || Panel(panel)->HasParent(Panel(restricted)) ||
           Panel(restricted)->HasParent(Panel(panel));
}

extern "C" KeyCode KeyCode_VirtualKeyToVGUI(int key);

void CSDLSurface::ResetMouseButtons() {
    // Cancellation is not a native release: never enqueue MouseReleased for
    // a newly focused panel or a replacement that reused the old allocation.
    ResetStandaloneMouseState();
    mouseWindows_.fill(0);
    mouseTargets_.fill(0);
}

void CSDLSurface::Event(const SDL_Event& event, StandaloneWindow& window, VPANEL panel) {
    const auto owner = TrackCallback(panel);
    const VPANEL modal = g_pInput->GetAppModalSurface();
    const bool allowed =
        !modal || panel == modal || Panel(modal)->HasParent(Panel(panel)) || Panel(panel)->HasParent(Panel(modal));
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        eventModifiers_ = SDL_KMOD_NONE;
        ResetMouseButtons();
        window.ResetWheel();
        if (focused_ == window.Panel()) {
            focused_ = 0;
        }
        g_pInput->SetMouseCapture(0);
        g_pInput->SetMouseFocus(0);
        captured_ = 0;
        SDL_CaptureMouse(false);
        for (int code = KEY_NONE + 1; code < KEY_LAST; ++code) {
            const auto key = static_cast<KeyCode>(code);
            if (g_pInput->IsKeyDown(key)) {
                g_pInput->InternalKeyCodeReleased(key);
            }
        }
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        ReconcileSize(panel, true);
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_MOVED) {
        int x = 0;
        int y = 0;
        window.AcceptNativePosition(x, y);
        Panel(panel)->SetPos(x, y);
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE && !captured_) {
        window.ResetWheel();
        cursorPositionValid_ = false;
        g_pInput->SetMouseFocus(0);
        return;
    }
    if (!allowed || window.disabled) {
        window.ResetWheel();
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        g_pIVgui->PostMessage(panel, new KeyValues("Close"), 0);
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_KEY_DOWN ||
        event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_TEXT_INPUT) {
        focused_ = window.Panel();
        MovePopupToFront(panel);
        // The retained input owner calculates its target from popup order. Refresh it
        // before enqueueing this window's key message, not after the event batch.
        g_pInput->RunFrame();
        panel = ResolveCallback(owner);
        if (!panel) {
            return;
        }
    }
    int x = 0;
    int y = 0;
    Panel(panel)->GetAbsPos(x, y);
    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
        if (Panel(panel)->IsMouseInputEnabled()) {
            MoveCursor(x + static_cast<int>(event.motion.x), y + static_cast<int>(event.motion.y));
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!Panel(panel)->IsMouseInputEnabled()) {
            break;
        }
        MouseCode code;
        switch (event.button.button) {
        case SDL_BUTTON_LEFT:
            code = MOUSE_LEFT;
            break;
        case SDL_BUTTON_RIGHT:
            code = MOUSE_RIGHT;
            break;
        case SDL_BUTTON_MIDDLE:
            code = MOUSE_MIDDLE;
            break;
        case SDL_BUTTON_X1:
            code = MOUSE_4;
            break;
        case SDL_BUTTON_X2:
            code = MOUSE_5;
            break;
        default:
            return;
        }
        MovePopupToFront(panel);
        MoveCursor(x + static_cast<int>(event.button.x), y + static_cast<int>(event.button.y));
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (mouseWindows_[code] != window.Panel()) {
                break;
            }
            mouseWindows_[code] = 0;
            mouseTargets_[code] = 0;
            g_pInput->InternalMouseReleased(code);
        } else {
            mouseWindows_[code] = window.Panel();
            const VPANEL target = captured_ ? g_pIVgui->HandleToPanel(captured_) : g_pInput->GetMouseOver();
            mouseTargets_[code] = target ? g_pIVgui->PanelToHandle(target) : 0;
            if (event.button.clicks >= 2) {
                g_pInput->InternalMouseDoublePressed(code);
            } else {
                g_pInput->InternalMousePressed(code);
            }
        }
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        if (!Panel(panel)->IsMouseInputEnabled()) {
            window.ResetWheel();
            break;
        }
        MoveCursor(x + static_cast<int>(event.wheel.mouse_x), y + static_cast<int>(event.wheel.mouse_y));
        const VPANEL target = g_pInput->GetMouseOver();
        if (!target) {
            window.ResetWheel();
            break;
        }
        // SDL marks inverted native-device deltas explicitly. Normalize to the
        // retained positive-up detent convention before per-target accumulation.
        const float delta = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
        const int whole = window.TakeWheel(delta, g_pIVgui->PanelToHandle(target));
        if (whole != 0) {
            g_pInput->InternalMouseWheeled(whole);
        }
        break;
    }
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        if (!Panel(panel)->IsKeyBoardInputEnabled()) {
            break;
        }
        SDL_Keycode key = event.key.key;
        if (key == SDLK_KP_PERIOD) {
            key = SDLK_KP_DECIMAL;
        }
        const KeyCode code = KeyCode_VirtualKeyToVGUI(static_cast<int>(key));
        if (code == KEY_NONE || (!extendedKeys_ && (code == KEY_LWIN || code == KEY_RWIN))) {
            break;
        }
        if (event.type == SDL_EVENT_KEY_UP) {
            g_pInput->InternalKeyCodeReleased(code);
        } else {
            if (!event.key.repeat) {
                g_pInput->InternalKeyCodePressed(code);
            }
            g_pInput->InternalKeyCodeTyped(code);
        }
        break;
    }
    case SDL_EVENT_TEXT_INPUT: {
        if (!Panel(panel)->IsKeyBoardInputEnabled() || !event.text.text) {
            break;
        }
        const char* cursor = event.text.text;
        const char* end = cursor + std::strlen(cursor);
        while (cursor < end) {
            const Uint32 scalar = SDL_StepUTF8(&cursor, nullptr);
            if (scalar == 0) {
                break;
            }
            const cse::Utf16Character units = cse::EncodeUtf16(scalar);
            g_pInput->InternalKeyTyped(static_cast<wchar_t>(units.first));
            if (units.count == 2) {
                g_pInput->InternalKeyTyped(static_cast<wchar_t>(units.second));
            }
        }
        break;
    }
    case SDL_EVENT_DROP_FILE:
    case SDL_EVENT_DROP_TEXT:
        // No retained dedicated/control listener consumes OS drag/drop. Preserve
        // the event as an ordinary routed VGUI message, without claiming OLE support.
        g_pIVgui->PostMessage(panel, new KeyValues("DragDrop", "text", event.drop.data), 0);
        break;
    default:
        break;
    }
}

void CSDLSurface::RunFrame() {
    if (!video_ || !SDL_IsMainThread()) {
        return;
    }
    // Apply newly requested retained geometry before reading old creation events.
    // Only changed retained requests are submitted, preserving pending OS moves.
    ApplyChanges();
    if (!video_) {
        return;
    }
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        // SDL's global modifiers already describe the end of the pumped batch.
        // Retain event order across frames, including modifier events without a
        // window ID, so mouse/text callbacks see the state at their own event.
        if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
            eventModifiers_ = event.key.mod;
        }
        if (event.type == SDL_EVENT_QUIT) {
            g_pIVgui->Stop();
            continue;
        }
        const SDL_Keymod modifiers = eventModifiers_;
        const cse_vgui2_input::Snapshot snapshot{false,
                                                 false,
                                                 false,
                                                 (modifiers & SDL_KMOD_LSHIFT) != 0,
                                                 (modifiers & SDL_KMOD_RSHIFT) != 0,
                                                 (modifiers & SDL_KMOD_LCTRL) != 0,
                                                 (modifiers & SDL_KMOD_RCTRL) != 0,
                                                 (modifiers & SDL_KMOD_LALT) != 0,
                                                 (modifiers & SDL_KMOD_RALT) != 0,
                                                 (modifiers & SDL_KMOD_CAPS) != 0,
                                                 (modifiers & SDL_KMOD_NUM) != 0,
                                                 (modifiers & SDL_KMOD_SCROLL) != 0};
        cse_vgui2_input::ApplyStandaloneModifiers(
            snapshot, (modifiers & SDL_KMOD_LGUI) != 0, (modifiers & SDL_KMOD_RGUI) != 0);
        SDL_Window* eventWindow = SDL_GetWindowFromEvent(&event);
        if (!eventWindow) {
            continue;
        }
        cse_vgui2_input::standaloneEventSnapshot = &snapshot;
        const SDL_WindowID id = SDL_GetWindowID(eventWindow);
        for (const auto& entry : windows_) {
            auto& owner = *entry.second;
            const VPANEL panel = g_pIVgui->HandleToPanel(owner.Panel());
            if (owner.Id() == id && panel && event.common.timestamp >= owner.Created()) {
                Event(event, owner, panel);
                break;
            }
        }
        if (!video_) {
            cse_vgui2_input::standaloneEventSnapshot = nullptr;
            return;
        }
        // Controls inspect IInput state inside their queued callbacks. Dispatch
        // before the next native event can release modifiers or change focus.
        // This occurs outside the owner iteration because callbacks can delete panels.
        // A size-completion callback may have entered a nested surface frame.
        cse_vgui2_input::standaloneEventSnapshot = &snapshot;
        DispatchStandaloneInputMessages();
        cse_vgui2_input::standaloneEventSnapshot = nullptr;
        if (!video_) {
            return;
        }
    }
    ApplyChanges();
    const auto handles = popups_;
    for (HPanel handle : handles) {
        const VPANEL panel = g_pIVgui->HandleToPanel(handle);
        if (panel && panel != embedded_ && Panel(panel)->IsVisible() && !IsMinimized(panel)) {
            const auto owner = TrackCallback(panel);
            SolveTraverse(panel, false);
            const VPANEL live = ResolveCallback(owner);
            if (live) {
                PaintTraverse(live);
            }
        }
    }
}

void CSDLSurface::EnableMouseCapture(VPANEL panel, bool state) {
    if (state) {
        auto* window = Window(panel);
        if (!window) {
            return;
        }
        captured_ = g_pIVgui->PanelToHandle(panel);
        SDL_CaptureMouse(true);
    } else {
        captured_ = 0;
        SDL_CaptureMouse(false);
    }
}

bool CSDLSurface::HasFocus() {
    if (focused_ && g_pIVgui->HandleToPanel(focused_)) {
        return true;
    }
    const SDL_Window* focused = SDL_GetKeyboardFocus();
    for (const auto& entry : windows_) {
        if (entry.second->Window() == focused) {
            return true;
        }
    }
    return false;
}

bool CSDLSurface::IsWithin(int x, int y) {
    for (HPanel handle : popups_) {
        const VPANEL panel = g_pIVgui->HandleToPanel(handle);
        if (!panel || !Panel(panel)->IsVisible() || panel == embedded_) {
            continue;
        }
        int left = 0;
        int top = 0;
        int width = 0;
        int height = 0;
        Panel(panel)->GetAbsPos(left, top);
        Panel(panel)->GetSize(width, height);
        if (x >= left && y >= top && x - left < width && y - top < height) {
            return true;
        }
    }
    return false;
}

void CSDLSurface::SetCursor(HCursor cursor) {
    if (cursorLocked_ || !video_) {
        return;
    }
    constexpr HCursor HiddenCursor = 1;
    constexpr HCursor BlankCursor = 15;
    cursorVisible_ = cursor != HiddenCursor && cursor != BlankCursor;
    if (!cursorVisible_) {
        SDL_HideCursor();
        return;
    }
    const SDL_SystemCursor shapes[] = {
        SDL_SYSTEM_CURSOR_DEFAULT,     SDL_SYSTEM_CURSOR_DEFAULT,     SDL_SYSTEM_CURSOR_DEFAULT,
        SDL_SYSTEM_CURSOR_TEXT,        SDL_SYSTEM_CURSOR_WAIT,        SDL_SYSTEM_CURSOR_PROGRESS,
        SDL_SYSTEM_CURSOR_CROSSHAIR,   SDL_SYSTEM_CURSOR_DEFAULT,     SDL_SYSTEM_CURSOR_NWSE_RESIZE,
        SDL_SYSTEM_CURSOR_NESW_RESIZE, SDL_SYSTEM_CURSOR_EW_RESIZE,   SDL_SYSTEM_CURSOR_NS_RESIZE,
        SDL_SYSTEM_CURSOR_MOVE,        SDL_SYSTEM_CURSOR_NOT_ALLOWED, SDL_SYSTEM_CURSOR_POINTER};
    if (cursor < sizeof(shapes) / sizeof(shapes[0])) {
        const SDL_SystemCursor shape = shapes[cursor];
        if (cursor_ && cursorShape_ == shape) {
            // SDL's identity check avoids native redraws, but still restores our
            // cursor if another SDL owner changed the selection in the meantime.
            SDL_SetCursor(cursor_.get());
        } else {
            decltype(cursor_) replacement(SDL_CreateSystemCursor(shape), SDL_DestroyCursor);
            if (replacement && SDL_SetCursor(replacement.get())) {
                cursor_ = std::move(replacement);
                cursorShape_ = shape;
            }
        }
    }
    SDL_ShowCursor();
}

bool CSDLSurface::IsCursorVisible() {
    return cursorVisible_;
}

void CSDLSurface::LockCursor() {
    cursorLocked_ = true;
}

void CSDLSurface::UnlockCursor() {
    cursorLocked_ = false;
}

void CSDLSurface::CalculateMouseVisible() {
    SetCursor(g_pInput->GetCursorOveride() ? g_pInput->GetCursorOveride() : 2);
}

bool CSDLSurface::NeedKBInput() {
    return HasFocus();
}

bool CSDLSurface::HasCursorPosFunctions() {
    return true;
}

void CSDLSurface::MoveCursor(int x, int y) {
    cursorX_ = x;
    cursorY_ = y;
    cursorPositionValid_ = true;
    g_pInput->InternalCursorMoved(x, y);
}

void CSDLSurface::SurfaceGetCursorPos(int& x, int& y) {
    if (cursorPositionValid_) {
        x = cursorX_;
        y = cursorY_;
        return;
    }
    float globalX = 0.0f;
    float globalY = 0.0f;
    SDL_GetGlobalMouseState(&globalX, &globalY);
    x = static_cast<int>(globalX);
    y = static_cast<int>(globalY);
}

void CSDLSurface::SurfaceSetCursorPos(int x, int y) {
    if (video_) {
        SDL_WarpMouseGlobal(static_cast<float>(x), static_cast<float>(y));
        MoveCursor(x, y);
    }
}

void CSDLSurface::GetWorkspaceBounds(int& x, int& y, int& wide, int& tall) {
    SDL_Rect bounds{0, 0, DefaultWidth, DefaultHeight};
    if (video_) {
        SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &bounds);
    }
    x = bounds.x;
    y = bounds.y;
    wide = bounds.w;
    tall = bounds.h;
}

void CSDLSurface::GetAbsoluteWindowBounds(int& x, int& y, int& wide, int& tall) {
    SDL_Rect bounds{0, 0, DefaultWidth, DefaultHeight};
    if (video_) {
        SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &bounds);
    }
    x = bounds.x;
    y = bounds.y;
    wide = bounds.w;
    tall = bounds.h;
}

void CSDLSurface::GetScreenSize(int& wide, int& tall) {
    int x = 0;
    int y = 0;
    GetAbsoluteWindowBounds(x, y, wide, tall);
}

bool CSDLSurface::SupportsFeature(SurfaceFeature_e feature) {
    return feature == ANTIALIASED_FONTS || feature == DROPSHADOW_FONTS || feature == FRAME_MINIMIZE_MAXIMIZE ||
           (feature == ESCAPE_KEY && supportsEscape_);
}

void CSDLSurface::RestrictPaintToSinglePanel(VPANEL panel) {
    restricted_ = panel ? g_pIVgui->PanelToHandle(panel) : 0;
}

void CSDLSurface::SetModalPanel(VPANEL panel) {
    g_pInput->SetAppModalSurface(panel);
    ApplyChanges();
}

VPANEL CSDLSurface::GetModalPanel() {
    return g_pInput->GetAppModalSurface();
}

void CSDLSurface::SetTranslateExtendedKeys(bool state) {
    extendedKeys_ = state;
}

VPANEL CSDLSurface::GetTopmostPopup() {
    for (auto iterator = popups_.rbegin(); iterator != popups_.rend(); ++iterator) {
        const VPANEL panel = g_pIVgui->HandleToPanel(*iterator);
        if (panel && panel != embedded_ && Panel(panel)->IsVisible()) {
            return panel;
        }
    }
    return 0;
}

HFont CSDLSurface::CreateFont() {
    return FontManager().CreateFont();
}

bool CSDLSurface::AddGlyphSetToFont(HFont font, const char* name, int tall, int weight, int blur, int scanlines,
                                    int flags, int, int) {
    // Dedicated TrackerScheme references the retained game symbol font without
    // listing a CustomFontFiles entry (Windows supplies Marlett as a system font).
    if (name && strcasecmp(name, "Marlett") == 0) {
        AddCustomFontFile("resource/marlett.ttf");
    }
    return FontManager().AddGlyphSetToFont(font, name, tall, weight, blur, scanlines, flags);
}

bool CSDLSurface::AddCustomFontFile(const char* filename) {
    return cse::fonts::RegisterMacCustomFontFile(this, g_pFileSystem, filename);
}

int CSDLSurface::GetFontTall(HFont font) {
    return FontManager().GetFontTall(font);
}

void CSDLSurface::GetCharABCwide(HFont font, int character, int& a, int& b, int& c) {
    FontManager().GetCharABCwide(font, character, a, b, c);
}

int CSDLSurface::GetCharacterWidth(HFont font, int character) {
    return FontManager().GetCharacterWidth(font, character);
}

void CSDLSurface::GetTextSize(HFont font, const wchar_t* text, int& wide, int& tall) {
    FontManager().GetTextSize(font, text, wide, tall);
}

int CSDLSurface::GetFontAscent(HFont font, wchar_t character) {
    return FontManager().GetFontAscent(font, character);
}

int CSDLSurface::GetFontBlur(HFont font) {
    return FontManager().GetFontBlur(font);
}

bool CSDLSurface::IsAdditive(HFont font) {
    return FontManager().IsAdditive(font);
}

void CSDLSurface::SetLanguage(const char* language) {
    const std::string selected = language ? language : "english";
    if (selected != language_) {
        for (const auto& glyph : glyphs_) {
            DeleteTextureByID(glyph.second);
        }
        glyphs_.clear();
    }
    language_ = selected;
    FontManager().SetLanguage(language_.c_str());
}

const char* CSDLSurface::GetLanguage() {
    return language_.c_str();
}

void CSDLSurface::GetProportionalBase(int& wide, int& tall) {
    wide = baseWidth_;
    tall = baseHeight_;
}

void CSDLSurface::SetProportionalBase(int wide, int tall) {
    if (wide > 0 && tall > 0) {
        baseWidth_ = wide;
        baseHeight_ = tall;
    }
}

void CSDLSurface::GetHDProportionalBase(int& wide, int& tall) {
    wide = hdWidth_;
    tall = hdHeight_;
}

void CSDLSurface::SetHDProportionalBase(int wide, int tall) {
    if (wide > 0 && tall > 0) {
        hdWidth_ = wide;
        hdHeight_ = tall;
    }
}

void CSDLSurface::SetSupportsEsc(bool state) {
    supportsEscape_ = state;
}

VPANEL CSDLSurface::GetNotifyPanel() {
    return notify_ ? g_pIVgui->HandleToPanel(notify_) : 0;
}

void CSDLSurface::SetNotifyIcon(VPANEL, HTexture, VPANEL, const char*) {
    // No call is consumed by retained dedicated/admin/controls. SDL tray ownership
    // is intentionally absent; do not fabricate a notification target or icon.
    SDL_Log("Standalone VGUI notification icons are unavailable");
}

void CSDLSurface::PlaySound(const char* filename) {
    std::vector<unsigned char> bytes;
    if (!ReadFile(filename, bytes)) {
        return;
    }
    SDL_AudioSpec specification{};
    Uint8* samples = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV_IO(SDL_IOFromConstMem(bytes.data(), bytes.size()), true, &specification, &samples, &length)) {
        return;
    }
    if (!audio_) {
        audio_ = SDL_InitSubSystem(SDL_INIT_AUDIO);
    }
    if (audio_ && length <= INT_MAX) {
        SDL_DestroyAudioStream(sound_);
        sound_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, nullptr, nullptr);
        if (sound_) {
            if (!SDL_PutAudioStreamData(sound_, samples, static_cast<int>(length)) || !SDL_FlushAudioStream(sound_) ||
                !SDL_ResumeAudioStreamDevice(sound_)) {
                SDL_DestroyAudioStream(sound_);
                sound_ = nullptr;
            }
        }
    }
    SDL_free(samples);
}

#include "native_standalone_abi.h"
extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetStandaloneVguiAbiV1() {
    return &cse::standalone_abi::Provider;
}
