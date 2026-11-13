#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <map>
#include <tuple>
#include <functional>
#include <vector>
#include <cstdint>
#include <objc/message.h>
#include <objc/runtime.h>
#include "../../../src/vgui2/controls/controls.h"
#include "../../../src/vgui2/controls/Panel.h"
#include "../../../src/vgui2/controls/Frame.h"
#include "../../../src/vgui2/controls/TextEntry.h"
#include "../../../src/vgui2/controls/Label.h"
#include "../../../src/vgui2/controls/Button.h"
#include "../../../src/vgui2/controls/CheckButton.h"
#include "../../../src/vgui2/controls/ComboBox.h"
#include "../../../src/vgui2/controls/Menu.h"
#include "../../../src/vgui2/controls/ScrollBar.h"
#include "../../../src/engine/controls.h"
#include "FileSystem.h"
#include "vgui/KeyValues.h"
#include "vgui/IVGui.h"
#include "vgui/IScheme.h"
#include "vgui/IInput.h"
#include "vgui/ISystem.h"
#include "vgui/IPanel.h"
#include "vgui/IImage.h"
#include "../../../src/common/legacy_bmp_records.h"
#include <unistd.h>

namespace {
bool diagnostics = false;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s (%s)\n", message, SDL_GetError());
        std::exit(1);
    }
}

class FixturePanel : public vgui2::Panel {
public:
    FixturePanel(const char* name) : Panel(nullptr, name) {
        SetSize(320, 200);
        SetPaintBorderEnabled(false);
        SetPaintBackgroundEnabled(false);
        SetPaintEnabled(false);
    }
    int keyCount = 0;
    int mouseCount = 0;
    int releaseCount = 0;
    wchar_t lastCharacter = 0;
    void OnKeyTyped(wchar_t character) override {
        ++keyCount;
        lastCharacter = character;
    }
    void OnMousePressed(vgui2::MouseCode) override {
        ++mouseCount;
    }
    void OnMouseReleased(vgui2::MouseCode) override {
        ++releaseCount;
    }
};

class RetiringPanel : public FixturePanel {
public:
    RetiringPanel() : FixturePanel("Retiring callback") {
    }
    std::function<void()> retire;
    int thinkCalls = 0;
    enum class Phase { None, Resize, Scheme, Think };
    Phase phase = Phase::None;
    void Invoke() {
        auto callback = std::move(retire);
        phase = Phase::None;
        callback();
    }
    void OnSizeChanged(int wide, int tall) override {
        if (phase == Phase::Resize) {
            Invoke();
            return;
        }
        FixturePanel::OnSizeChanged(wide, tall);
    }
    void PerformApplySchemeSettings() override {
        if (phase == Phase::Scheme) {
            Invoke();
            return;
        }
        FixturePanel::PerformApplySchemeSettings();
    }
    void Think() override {
        if (phase == Phase::Think) {
            Invoke();
            return;
        }
        ++thinkCalls;
        FixturePanel::Think();
    }
};

class FixtureTextEntry : public vgui2::TextEntry {
public:
    FixtureTextEntry(vgui2::Panel* parent, const char* name) : TextEntry(parent, name) {
    }
    bool shiftDuringText = false;
    void OnKeyTyped(wchar_t character) override {
        shiftDuringText = vgui2::input()->IsKeyDown(vgui2::KEY_LSHIFT);
        TextEntry::OnKeyTyped(character);
    }
    void OnKeyCodeTyped(vgui2::KeyCode code) override {
        if (diagnostics) {
            std::printf("ENTRY_KEY code=%d ctrl=%d shift=%d\n", code, vgui2::input()->IsKeyDown(vgui2::KEY_LCONTROL),
                        vgui2::input()->IsKeyDown(vgui2::KEY_LSHIFT));
            std::fflush(stdout);
        }
        TextEntry::OnKeyCodeTyped(code);
    }
    void OnMousePressed(vgui2::MouseCode code) override {
        if (diagnostics) {
            std::printf("ENTRY_MOUSE code=%d\n", code);
            std::fflush(stdout);
        }
        TextEntry::OnMousePressed(code);
    }
};

// Catches a later native snapshot overwriting a retained command in one surface event batch.
class MidBatchResizeFrame : public vgui2::Frame {
public:
    MidBatchResizeFrame(int wide, int initialTall, int requestedTall)
        : Frame(nullptr, "MidBatchResize", false, false), requestedTall_(requestedTall) {
        SetDeleteSelfOnClose(false);
        SetTitle("Mid-batch resize", true);
        SetSize(wide, initialTall);
        Activate();
    }

    SDL_Window* nativeWindow = nullptr;
    bool armed = false;
    bool inSurfaceFrame = false;
    bool inCommand = false;
    int commands = 0;
    int nativeTallAtCommand = 0;
    int immediateTall = 0;
    std::vector<int> laterHeights;
    std::vector<int> commandHeights;

    void OnCommand(const char* command) override {
        if (std::strcmp(command, "ExpandDuringNativeBatch") != 0) {
            Frame::OnCommand(command);
            return;
        }
        Require(armed && inSurfaceFrame, "ordinary command dispatch occurs inside actual surface RunFrame");
        ++commands;
        if (!commandHeights.empty()) {
            Require(static_cast<std::size_t>(commands) <= commandHeights.size(), "one height per queued command");
            requestedTall_ = commandHeights[static_cast<std::size_t>(commands - 1)];
        }
        int nativeWide = 0;
        Require(SDL_GetWindowSize(nativeWindow, &nativeWide, &nativeTallAtCommand), "sample live size before command");
        std::fprintf(stderr, "RESIZE_ORDER command window=%u panel=%llu native=%dx%d requested=%d\n",
                     SDL_GetWindowID(nativeWindow), static_cast<unsigned long long>(GetVPanel()),
                     nativeWide, nativeTallAtCommand, requestedTall_);
        inCommand = true;
        SetTall(requestedTall_);
        immediateTall = GetTall();
        inCommand = false;
        std::fprintf(stderr, "RESIZE_ORDER immediate retained=%d\n", immediateTall);
    }

    void OnSizeChanged(int wide, int tall) override {
        if (armed) {
            int nativeWide = 0;
            int nativeTall = 0;
            Require(SDL_GetWindowSize(nativeWindow, &nativeWide, &nativeTall), "sample live size in callback");
            std::fprintf(stderr, "RESIZE_ORDER callback surface=%d command=%d retained=%dx%d native=%dx%d\n",
                         inSurfaceFrame, inCommand, wide, tall, nativeWide, nativeTall);
            if (commands != 0 && !inCommand) {
                Require(inSurfaceFrame, "later resize callback occurs in the same surface frame");
                laterHeights.push_back(tall);
            }
        }
        Frame::OnSizeChanged(wide, tall);
    }

private:
    int requestedTall_;
};

class ResizeCallbackPanel : public FixturePanel {
public:
    ResizeCallbackPanel() : FixturePanel("Resize callback") {
    }
    std::function<void()> resizeCallback;

    void OnSizeChanged(int wide, int tall) override {
        FixturePanel::OnSizeChanged(wide, tall);
        auto callback = std::move(resizeCallback);
        if (callback) {
            callback();
        }
    }
};

std::function<void()> resizeSyncAction;

void ResizeSyncCallback() {
    auto callback = std::move(resizeSyncAction);
    Require(static_cast<bool>(callback), "armed completion callback");
    callback();
}

struct ResizeObserver {
    using Observe = void (*)(SDL_Window*, int, int, int, void (*)());
    Observe observe = reinterpret_cast<Observe>(dlsym(RTLD_DEFAULT, "CseTestObserveResize"));
    int (*count)(bool) = reinterpret_cast<int (*)(bool)>(dlsym(RTLD_DEFAULT, "CseTestResizeCount"));
    void (*density)(float) = reinterpret_cast<void (*)(float)>(dlsym(RTLD_DEFAULT, "CseTestResizeDensity"));

    ResizeObserver() {
        Require(observe && count && density, "actual SDL geometry boundary observer loaded");
    }
};

void QueueWindowEvent(SDL_Window* window, SDL_EventType type) {
    SDL_Event event{};
    event.type = type;
    event.window.windowID = SDL_GetWindowID(window);
    event.common.timestamp = SDL_GetTicksNS();
    Require(SDL_PushEvent(&event), "queue routed native geometry event");
}

void RequireGeometry(vgui2::Panel* panel, SDL_Window* window, int wide, int tall, int density = 1) {
    int nativeWide = 0;
    int nativeTall = 0;
    Require(SDL_GetWindowSize(window, &nativeWide, &nativeTall), "read accepted native logical geometry");
    Require(panel->GetWide() == wide && panel->GetTall() == tall && nativeWide == wide && nativeTall == tall,
            "retained and accepted native logical dimensions agree with expected result");
    SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    Require(pixels && pixels->w == wide * density && pixels->h == tall * density,
            "actual backing dimensions agree with retained logical size and density");
    SDL_DestroySurface(pixels);
}

class ControlFrame : public vgui2::Frame {
public:
    ControlFrame(vgui2::Panel* parent, const char* name, const char* title, int x) : Frame(parent, name, true, false) {
        SetDeleteSelfOnClose(false);
        SetTitle(title, true);
        SetSizeable(true);
        SetMinimizeButtonVisible(true);
        SetMaximizeButtonVisible(true);
        entry = new FixtureTextEntry(this, "StringEdit");
        new vgui2::Label(this, "Label1", "Edit Unicode text; copy and paste between windows.");
        auto* okay = new vgui2::Button(this, "OKButton", "OK");
        auto* close = new vgui2::Button(this, "CancelButton", "Close");
        LoadControlSettings("Admin/VarEditDialog_String.res", nullptr);
        SetBounds(x, 100, 440, 300);
        okay->SetBounds(20, 250, 90, 28);
        close->SetBounds(120, 250, 90, 28);
        entry->SetBounds(20, 92, 400, 28);
        entry->SetAllowNonAsciiCharacters(true);
        entry->SetText(L"Native VGUI — zażółć gęślą");
        check = new vgui2::CheckButton(this, "Check", "Retained checkbox");
        check->SetBounds(20, 135, 230, 28);
        combo = new vgui2::ComboBox(this, "Combo", 4, false);
        combo->SetBounds(20, 180, 280, 28);
        combo->AddItem("Counter-Strike", nullptr);
        combo->AddItem("Half-Life", nullptr);
        combo->ActivateItemByRow(0);
        auto* quit = new vgui2::Button(this, "Quit", "Quit both windows");
        quit->SetCommand("QuitFixture");
        quit->AddActionSignalTarget(this);
        quit->SetBounds(240, 250, 180, 28);
        Activate();
    }
    vgui2::TextEntry* entry = nullptr;
    vgui2::CheckButton* check = nullptr;
    vgui2::ComboBox* combo = nullptr;
    void OnCommand(const char* command) override {
        if (diagnostics) {
            std::printf("FRAME_COMMAND name=%s command=%s\n", GetName(), command);
            std::fflush(stdout);
        }
        if (std::strcmp(command, "QuitFixture") == 0) {
            vgui2::ivgui()->Stop();
        } else {
            Frame::OnCommand(command);
        }
    }
};

SDL_Window* FindWindow(const char* title) {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* found = nullptr;
    for (int index = 0; index < count; ++index) {
        if (std::strcmp(SDL_GetWindowTitle(windows[index]), title) == 0) {
            found = windows[index];
        }
    }
    SDL_free(windows);
    return found;
}

bool Watch(void*, SDL_Event* event) {
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
        std::printf("SDL_KEY window=%u type=%u key=%u scan=%u mods=%u repeat=%d\n", event->key.windowID, event->type,
                    event->key.key, event->key.scancode, event->key.mod, event->key.repeat);
    } else if (event->type == SDL_EVENT_TEXT_INPUT) {
        std::printf("SDL_TEXT window=%u text=%s\n", event->text.windowID, event->text.text);
    } else if (event->type == SDL_EVENT_WINDOW_FOCUS_GAINED || event->type == SDL_EVENT_WINDOW_FOCUS_LOST ||
               event->type == SDL_EVENT_WINDOW_MOVED || event->type == SDL_EVENT_WINDOW_RESIZED ||
               event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        std::printf("SDL_WINDOW window=%u type=%u data=%d,%d\n", event->window.windowID, event->type,
                    event->window.data1, event->window.data2);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        float globalX = 0.0f;
        float globalY = 0.0f;
        SDL_GetGlobalMouseState(&globalX, &globalY);
        std::printf("SDL_MOUSE window=%u type=%u button=%u local=%.1f,%.1f global=%.1f,%.1f\n", event->button.windowID,
                    event->type, event->button.button, event->button.x, event->button.y, globalX, globalY);
    } else {
        return true;
    }
    std::fflush(stdout);
    return true;
}

void CheckCocoaVisibility() {
    using NativeState = std::tuple<bool, bool, void*>;
    static std::map<SDL_WindowID, NativeState> previous;
    static std::map<SDL_WindowID, int> disagreements;
    constexpr int MaximumTransitionFrames = 10;
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    for (int index = 0; index < count; ++index) {
        SDL_Window* window = windows[index];
        const auto id = SDL_GetWindowID(window);
        void* native =
            SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        if (!native) {
            continue;
        }
        auto visibleQuery = reinterpret_cast<BOOL (*)(void*, SEL)>(objc_msgSend);
        auto parentQuery = reinterpret_cast<void* (*)(void*, SEL)>(objc_msgSend);
        const bool visible = visibleQuery(native, sel_registerName("isVisible"));
        void* parent = parentQuery(native, sel_registerName("parentWindow"));
        const bool hidden = (SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN) != 0;
        const NativeState current{hidden, visible, parent};
        if (!previous.count(id) || previous[id] != current) {
            std::printf("COCOA_VISIBILITY id=%u sdl_hidden=%d native_visible=%d native_parent=%p\n", id, hidden,
                        visible, parent);
            previous[id] = current;
            std::fflush(stdout);
        }
        if (hidden && visible) {
            Require(++disagreements[id] < MaximumTransitionFrames,
                    "Cocoa window remains visible while its owning SDL window is hidden");
        } else {
            disagreements[id] = 0;
        }
    }
    SDL_free(windows);
}

void Key(SDL_Window* window, SDL_EventType type, SDL_Keycode key, SDL_Keymod modifiers) {
    SDL_Event event{};
    event.type = type;
    event.key.windowID = window ? SDL_GetWindowID(window) : 0;
    event.key.key = key;
    event.key.mod = modifiers;
    Require(SDL_PushEvent(&event), "queue key");
}

void Click(SDL_Window* window, float x, float y) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
    Require(SDL_PushEvent(&event), "queue pointer down");
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    Require(SDL_PushEvent(&event), "queue pointer up");
}

void Text(SDL_Window* window, const char* text) {
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = SDL_GetWindowID(window);
    event.text.text = text;
    Require(SDL_PushEvent(&event), "queue text");
}
} // namespace

int main(int argc, char** argv) {
    Require(argc == 4, "usage: control_tests runtime-root resource-root test-or-cocoa");
    const std::string runtime(argv[1]);
    void* filesystemImage = dlopen((runtime + "/filesystem_stdio.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    void* guiImage = dlopen((runtime + "/vgui2.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    Require(filesystemImage && guiImage, "load actual owning libraries");
    auto filesystemFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(filesystemImage, "CreateInterface"));
    auto guiFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(guiImage, "CreateInterface"));
    auto* filesystem = static_cast<IFileSystem*>(filesystemFactory("VFileSystem009", nullptr));
    filesystem->Mount();
    filesystem->AddSearchPath(argv[2], "PLATFORM");
    if (std::strcmp(argv[3], "surface_partial_failure_reopen") == 0) {
        auto* initialGui = static_cast<vgui2::IVGui*>(guiFactory("VGUI_ivgui006", nullptr));
        Require(!initialGui->Init(&guiFactory, 1), "missing filesystem rejects initialization");
    }
    CreateInterfaceFn factories[] = {guiFactory, filesystemFactory};
    Require(vgui2::VGuiControls_Init("STANDALONE_TEST", factories, 2), "initialize retained controls");
    auto* surface = vgui2::surface();
    auto* gui = vgui2::ivgui();
    gui->SetSleep(false);
    gui->Start();
    Require(vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", nullptr) != 0,
            "retained scheme resource");
    const auto font = surface->CreateFont();
    Require(surface->AddGlyphSetToFont(font, "Arial", 16, 400, 0, 0, 0, 0, 0), "retained font provider");
    auto* first = new FixturePanel("First");
    auto* second = new FixturePanel("Second");
    first->SetPos(20, 20);
    second->SetPos(400, 20);
    surface->CreatePopup(first->GetVPanel(), false, true, false, true, true);
    surface->CreatePopup(second->GetVPanel(), false, true, false, true, true);
    surface->SetTitle(first->GetVPanel(), L"First standalone window");
    surface->SetTitle(second->GetVPanel(), L"Second standalone window");
    SDL_Window* firstWindow = FindWindow("First standalone window");
    SDL_Window* secondWindow = FindWindow("Second standalone window");
    Require(firstWindow && secondWindow && firstWindow != secondWindow, "two actual SDL windows");
    Require(!SDL_GetHintBoolean(SDL_HINT_MAC_PRESS_AND_HOLD, true), "actual video owner suppresses accent picker");
    gui->RunFrame();
    const std::string scenario(argv[3]);
    diagnostics = scenario == "cocoa";
    if (scenario == "cocoa" || scenario == "controls") {
        delete first;
        delete second;
        first = nullptr;
        second = nullptr;
        auto* root = new FixturePanel("FixtureRoot");
        surface->SetEmbeddedPanel(root->GetVPanel());
        vgui2::localize()->AddFile(filesystem, "Admin/server_english.txt");
        auto* left = new ControlFrame(root, "Left", "Standalone VGUI — window A", 100);
        auto* right = new ControlFrame(root, "Right", "Standalone VGUI — window B", 570);
        gui->RunFrame();
        if (scenario == "controls") {
            SDL_Window* window = FindWindow("Standalone VGUI — window A");
            Require(window != nullptr, "retained frame native title");
            int count = 0;
            SDL_Window** windows = SDL_GetWindows(&count);
            for (int index = 0; index < count; ++index) {
                Require(!(SDL_GetWindowFlags(windows[index]) & SDL_WINDOW_HIDDEN) ||
                            !SDL_GetWindowParent(windows[index]),
                        "hidden retained menus never attach to a Cocoa parent that would show them");
            }
            SDL_free(windows);
            auto hiddenParents = reinterpret_cast<int (*)()>(dlsym(RTLD_DEFAULT, "CseTestHiddenParentAttempts"));
            Require(hiddenParents && hiddenParents() == 0, "real native parent API is never called on a hidden window");
            if (const char* path = SDL_getenv("CSE_TEST_PIXEL_RECEIPT")) {
                SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
                Require(pixels && SDL_SaveBMP(pixels, path), "retain actual controls raster receipt");
                SDL_DestroySurface(pixels);
            }
            left->check->SetVisible(false);
            Require((SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN) == 0,
                    "hiding a retained child does not hide its native popup ancestor");
            left->check->SetVisible(true);
            Click(window, 30.0f, 149.0f);
            gui->RunFrame();
            Require(left->check->IsSelected(), "real pointer down/up toggles retained checkbox");
            Require(!left->combo->IsDropdownVisible(), "combo initially closed");
            Click(window, 290.0f, 194.0f);
            gui->RunFrame();
            Require(left->combo->IsDropdownVisible(), "real combo arrow opens its separate menu window");
            surface->SetTitle(left->combo->GetMenu()->GetVPanel(), L"Fixture combo menu");
            SDL_Window* menuWindow = FindWindow("Fixture combo menu");
            Require(menuWindow != nullptr, "combo has actual separate native window");
            Key(menuWindow, SDL_EVENT_KEY_DOWN, SDLK_ESCAPE, SDL_KMOD_NONE);
            Key(menuWindow, SDL_EVENT_KEY_UP, SDLK_ESCAPE, SDL_KMOD_NONE);
            gui->RunFrame();
            left->entry->RequestFocus(0);
            Key(window, SDL_EVENT_KEY_DOWN, SDLK_END, SDL_KMOD_NONE);
            Key(window, SDL_EVENT_KEY_UP, SDLK_END, SDL_KMOD_NONE);
            Key(window, SDL_EVENT_KEY_DOWN, SDLK_BACKSPACE, SDL_KMOD_NONE);
            Key(window, SDL_EVENT_KEY_UP, SDLK_BACKSPACE, SDL_KMOD_NONE);
            gui->RunFrame();
            wchar_t edited[128]{};
            left->entry->GetText(edited, 128);
            Require(std::wcscmp(edited, L"Native VGUI — zażółć gęśl") == 0, "real Backspace edits focused TextEntry");
            Click(window, 330.0f, 263.0f);
            gui->RunFrame();
            Require(!gui->IsRunning(), "real Quit button receives pointer down/up and action message");
            // Ancestor visibility is independent of the key focus tests above:
            // retained Frame hiding dismisses focus and can emit MenuClose.
            left->combo->ShowMenu();
            surface->ApplyChanges();
            Require((SDL_GetWindowFlags(menuWindow) & SDL_WINDOW_HIDDEN) == 0, "menu window is shown");
            left->SetVisible(false);
            surface->ApplyChanges();
            Require((SDL_GetWindowFlags(menuWindow) & SDL_WINDOW_HIDDEN) != 0,
                    "visible menu obeys hidden retained ancestor");
        }
        int windowCount = 0;
        SDL_Window** nativeWindows = SDL_GetWindows(&windowCount);
        for (int index = 0; index < windowCount; ++index) {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
            SDL_GetWindowPosition(nativeWindows[index], &x, &y);
            SDL_GetWindowSize(nativeWindows[index], &width, &height);
            std::printf("WINDOW_GEOMETRY id=%u title=%s pos=%d,%d size=%d,%d density=%.2f\n",
                        SDL_GetWindowID(nativeWindows[index]), SDL_GetWindowTitle(nativeWindows[index]), x, y, width,
                        height, SDL_GetWindowPixelDensity(nativeWindows[index]));
        }
        SDL_free(nativeWindows);
        if (diagnostics) {
            CheckCocoaVisibility();
        }
        std::puts("COCOA_READY: two retained control windows. Quit both windows or close both to finish.");
        std::fflush(stdout);
        int frames = 0;
        std::string previous[2];
        vgui2::VPANEL previousFocus = 0;
        vgui2::VPANEL previousMouse = 0;
        SDL_AddEventWatch(Watch, nullptr);
        while (gui->IsRunning() && (left->IsVisible() || right->IsVisible())) {
            gui->RunFrame();
            if (diagnostics) {
                CheckCocoaVisibility();
            }
            const auto focus = vgui2::input()->GetFocus();
            const auto mouse = vgui2::input()->GetMouseOver();
            if (focus != previousFocus || mouse != previousMouse) {
                int x = 0;
                int y = 0;
                vgui2::input()->GetCursorPos(x, y);
                std::printf("VGUI_FOCUS key=%s mouse=%s cursor=%d,%d\n",
                            focus ? vgui2::ipanel()->GetName(focus) : "none",
                            mouse ? vgui2::ipanel()->GetName(mouse) : "none", x, y);
                previousFocus = focus;
                previousMouse = mouse;
                std::fflush(stdout);
            }
            ControlFrame* controls[] = {left, right};
            for (int index = 0; index < 2; ++index) {
                char value[2048]{};
                controls[index]->entry->GetText(value, sizeof(value));
                if (previous[index] != value) {
                    previous[index] = value;
                    const auto focus = vgui2::input()->GetFocus();
                    std::printf("TEXT_VALUE window=%c value=%s focus=%s ctrl=%d shift=%d\n", 'A' + index, value,
                                focus ? vgui2::ipanel()->GetName(focus) : "none",
                                vgui2::input()->IsKeyDown(vgui2::KEY_LCONTROL),
                                vgui2::input()->IsKeyDown(vgui2::KEY_LSHIFT));
                    std::fflush(stdout);
                }
            }
            SDL_Delay(10);
            if (scenario == "controls" && ++frames == 5) {
                gui->Stop();
            }
        }
        SDL_RemoveEventWatch(Watch, nullptr);
        surface->SetEmbeddedPanel(0);
        delete root;
    } else if (scenario == "cursor_ownership") {
        constexpr int CreateCalls = 0;
        constexpr int SelectCalls = 1;
        constexpr int DestroyCalls = 2;
        constexpr int LiveCursors = 3;
        constexpr int FailCreation = 1;
        constexpr int FailSelection = 2;
        auto count = reinterpret_cast<int (*)(int)>(dlsym(RTLD_DEFAULT, "CseCursorCount"));
        auto shape = reinterpret_cast<int (*)()>(dlsym(RTLD_DEFAULT, "CseCursorShape"));
        auto fail = reinterpret_cast<void (*)(int)>(dlsym(RTLD_DEFAULT, "CseCursorFail"));
        Require(count && shape && fail, "cursor boundary observer available");
        const bool simulated = std::getenv("CSE_CURSOR_SIMULATED") != nullptr;
        Require(simulated || std::strcmp(SDL_GetCurrentVideoDriver(), "cocoa") == 0,
                "native cursor qualification requires actual Cocoa");
        std::printf("CURSOR_BACKEND %s\n", simulated ? "SIMULATED ownership only" : "ACTUAL Cocoa SDL");
        constexpr vgui2::HCursor Arrow = 2;
        constexpr vgui2::HCursor TextCursor = 3;
        constexpr vgui2::HCursor Hand = 14;
        constexpr vgui2::HCursor Hidden = 1;
        constexpr vgui2::HCursor Blank = 15;
        constexpr int RepeatCount = 2048;
        surface->SetCursor(TextCursor);
        Require(shape() == SDL_SYSTEM_CURSOR_TEXT && count(LiveCursors) == 1, "real text shape selected and owned");
        SDL_Cursor* original = SDL_GetCursor();
        const int initialCreates = count(CreateCalls);
        const int initialDestroys = count(DestroyCalls);
        for (int index = 0; index < RepeatCount; ++index) {
            surface->SetCursor(TextCursor);
        }
        Require(count(CreateCalls) == initialCreates && count(DestroyCalls) == initialDestroys && SDL_GetCursor() == original,
                "unchanged shape reuses its live cursor without allocation or destruction");
        surface->LockCursor();
        surface->SetCursor(Hand);
        surface->SetCursor(Hidden);
        Require(SDL_GetCursor() == original && SDL_CursorVisible(), "lock preserves shape and visibility");
        surface->UnlockCursor();
        surface->SetCursor(Hidden);
        surface->SetCursor(Blank);
        Require(!surface->IsCursorVisible() && !SDL_CursorVisible(), "both retained hidden cursors hide");
        surface->SetCursor(TextCursor);
        Require(SDL_GetCursor() == original && SDL_CursorVisible() && count(CreateCalls) == initialCreates,
                "show restores the same owned shape without replacement");
        fail(FailCreation);
        surface->SetCursor(Hand);
        Require(SDL_GetCursor() == original && count(LiveCursors) == 1, "failed create preserves prior cursor");
        fail(FailSelection);
        surface->SetCursor(Hand);
        Require(SDL_GetCursor() == original && count(LiveCursors) == 1,
                "failed selection retires replacement and preserves prior cursor");
        surface->SetCursor(Hand);
        Require(shape() == SDL_SYSTEM_CURSOR_POINTER && count(LiveCursors) == 1, "shape change retries successfully");
        surface->SetCursor(Arrow);
        const int arrowCreates = count(CreateCalls);
        surface->SetCursor(0); // Retained default alias has the same native shape.
        Require(shape() == SDL_SYSTEM_CURSOR_DEFAULT && count(CreateCalls) == arrowCreates, "native shape alias reuses cursor");
        // Another SDL owner may change selection. Reapply the still-owned native shape.
        SDL_Cursor* ownedArrow = SDL_GetCursor();
        SDL_Cursor* external = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
        Require(external && SDL_SetCursor(external), "external cursor selection");
        const int externalCreates = count(CreateCalls);
        surface->SetCursor(Arrow);
        Require(SDL_GetCursor() == ownedArrow && count(CreateCalls) == externalCreates,
                "unchanged shape restores owned cursor after external selection without allocating");
        SDL_DestroyCursor(external);
        constexpr vgui2::HCursor InvalidCursor = 300;
        surface->SetCursor(InvalidCursor);
        Require(SDL_GetCursor() == ownedArrow && count(LiveCursors) == 1, "invalid shape preserves live cursor");
        surface->EnableMouseCapture(first->GetVPanel(), true);
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        focus.window.windowID = SDL_GetWindowID(firstWindow);
        SDL_PushEvent(&focus);
        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        focus.window.windowID = SDL_GetWindowID(secondWindow);
        SDL_PushEvent(&focus);
        gui->RunFrame();
        surface->SetCursor(TextCursor);
        Require(shape() == SDL_SYSTEM_CURSOR_TEXT && SDL_CursorVisible(), "focus and capture transition permits new shape");
        surface->SetCursor(Hidden);
        surface->LockCursor();
        delete first;
        delete second;
        surface->Shutdown();
        Require(count(LiveCursors) == 0 && SDL_WasInit(SDL_INIT_VIDEO) == 0, "cursor retires before video subsystem");
        first = new FixturePanel("Cursor reopen");
        second = new FixturePanel("Cursor reopen second");
        surface->CreatePopup(first->GetVPanel(), false, true, false, true, true);
        surface->CreatePopup(second->GetVPanel(), false, true, false, true, true);
        fail(FailCreation);
        surface->SetCursor(TextCursor);
        Require(count(LiveCursors) == 0, "initial create failure transfers no ownership after reopen");
        fail(FailSelection);
        surface->SetCursor(TextCursor);
        Require(count(LiveCursors) == 0, "initial selection failure releases unselected cursor after reopen");
        surface->SetCursor(TextCursor);
        Require(shape() == SDL_SYSTEM_CURSOR_TEXT && count(LiveCursors) == 1 && surface->IsCursorVisible() && SDL_CursorVisible(),
                "reopen clears stale lock and hidden state and selects a new owned cursor");
        std::printf("CURSOR_RESULT repeats=%d creates=%d sets=%d destroys=%d live=%d\n",
                    RepeatCount, count(CreateCalls), count(SelectCalls), count(DestroyCalls), count(LiveCursors));
    } else if (scenario == "callback_resize_retirement" || scenario == "callback_scheme_retirement" ||
               scenario == "callback_think_retirement") {
        constexpr int SelfRetirement = 0;
        constexpr int AncestorRetirement = 1;
        constexpr int WindowReplacement = 2;
        for (int mode : {SelfRetirement, AncestorRetirement, WindowReplacement}) {
            const bool ancestor = mode == AncestorRetirement;
            auto* retiring = new RetiringPanel();
            FixturePanel* parent = ancestor ? new FixturePanel("Retiring ancestor") : retiring;
            if (ancestor) {
                retiring->SetParent(parent);
            }
            surface->CreatePopup(parent->GetVPanel(), false, true, false, true, true);
            if (ancestor && scenario == "callback_resize_retirement") {
                surface->CreatePopup(retiring->GetVPanel(), false, true, false, true, true);
            }
            surface->SetTitle(retiring->GetVPanel(), L"Retiring callback window");
            gui->RunFrame();
            const auto oldParent = gui->PanelToHandle(parent->GetVPanel());
            const auto oldChild = gui->PanelToHandle(retiring->GetVPanel());
            const int oldThinkCalls = retiring->thinkCalls;
            FixturePanel* replacement = nullptr;
            int callbacks = 0;
            retiring->retire = [&] {
                ++callbacks;
                if (mode == WindowReplacement) {
                    const auto panel = retiring->GetVPanel();
                    surface->ReleasePanel(panel);
                    surface->CreatePopup(panel, false, true, false, true, true);
                    return;
                }
                delete parent;
                parent = nullptr;
                replacement = new FixturePanel("Callback replacement");
                surface->CreatePopup(replacement->GetVPanel(), false, true, false, true, true);
            };
            if (scenario == "callback_resize_retirement") {
                retiring->phase = RetiringPanel::Phase::Resize;
                SDL_Window* window = FindWindow("Retiring callback window");
                Require(window && SDL_SetWindowSize(window, 350, 220), "native resize before deleting callback");
                gui->RunFrame();
            } else {
                retiring->phase = scenario == "callback_scheme_retirement" ? RetiringPanel::Phase::Scheme
                                                                           : RetiringPanel::Phase::Think;
                surface->SolveTraverse(parent->GetVPanel(), true);
            }
            Require(callbacks == 1, "synchronous retirement callback executes exactly once");
            if (mode == WindowReplacement) {
                Require(gui->HandleToPanel(oldParent) && gui->HandleToPanel(oldChild),
                        "native window replacement preserves the live retained panel");
                if (scenario == "callback_scheme_retirement") {
                    Require(retiring->thinkCalls == oldThinkCalls,
                            "old scheme traversal never advances into the replacement window generation");
                }
                delete parent;
            } else {
                Require(!gui->HandleToPanel(oldParent) && !gui->HandleToPanel(oldChild),
                        "synchronous callback retires self or ancestor");
                Require(replacement && replacement->keyCount == 0, "replacement is not the old callback target");
            }
            gui->RunFrame();
            delete replacement;
        }
    } else if (scenario == "mouse_button_retirement") {
        const auto press = [&](SDL_Window* window) {
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.windowID = SDL_GetWindowID(window);
            event.button.button = SDL_BUTTON_LEFT;
            event.button.x = 8.0f;
            event.button.y = 8.0f;
            Require(SDL_PushEvent(&event), "queue held mouse button");
            gui->RunFrame();
            Require(vgui2::input()->IsMouseDown(vgui2::MOUSE_LEFT), "real retained mouse button is held");
        };
        press(firstWindow);
        vgui2::input()->SetMouseCapture(first->GetVPanel());
        SDL_Event lost{};
        lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        lost.window.windowID = SDL_GetWindowID(firstWindow);
        Require(SDL_PushEvent(&lost), "queue loss without mouse release");
        gui->RunFrame();
        Require(!vgui2::input()->IsMouseDown(vgui2::MOUSE_LEFT), "focus loss retires held mouse state");
        SDL_Event abandonedRelease{};
        abandonedRelease.type = SDL_EVENT_MOUSE_BUTTON_UP;
        abandonedRelease.button.windowID = SDL_GetWindowID(secondWindow);
        abandonedRelease.button.button = SDL_BUTTON_LEFT;
        abandonedRelease.button.x = 8.0f;
        abandonedRelease.button.y = 8.0f;
        Require(SDL_PushEvent(&abandonedRelease), "queue abandoned release over a different target");
        gui->RunFrame();
        Require(first->releaseCount == 0 && second->releaseCount == 0, "reset emits no fabricated release");
        auto* editor = new ControlFrame(nullptr, "FocusEditor", "Keyboard focus editor", 40);
        gui->RunFrame();
        editor->entry->SelectAllOnFirstFocus(false);
        editor->entry->SetText("abc");
        editor->entry->GotoTextStart();
        editor->check->RequestFocus(0);
        gui->RunFrame();
        editor->entry->RequestFocus(0);
        gui->RunFrame();
        Text(FindWindow("Keyboard focus editor"), "X");
        gui->RunFrame();
        char value[16]{};
        editor->entry->GetText(value, sizeof(value));
        Require(std::strcmp(value, "abcX") == 0, "TextEntry keyboard focus moves caret after abandoned mouse press");
        delete editor;
        press(firstWindow);
        delete first;
        first = new FixturePanel("Mouse replacement");
        surface->CreatePopup(first->GetVPanel(), false, true, false, true, true);
        gui->RunFrame();
        Require(!vgui2::input()->IsMouseDown(vgui2::MOUSE_LEFT) && first->releaseCount == 0 &&
                    second->releaseCount == 0,
                "popup retirement clears held mouse state without releasing into replacement");
        press(secondWindow);
        surface->Shutdown();
        Require(!vgui2::input()->IsMouseDown(vgui2::MOUSE_LEFT), "shutdown clears held mouse state");
        surface->CreatePopup(first->GetVPanel(), false, true, false, true, true);
        surface->CreatePopup(second->GetVPanel(), false, true, false, true, true);
        gui->RunFrame();
        Require(!vgui2::input()->IsMouseDown(vgui2::MOUSE_LEFT) && first->releaseCount == 0 &&
                    second->releaseCount == 0,
                "same-image reopening inherits no mouse press or synthetic release");
    } else if (scenario == "shift_click_event_order") {
        auto* editor = new ControlFrame(nullptr, "ShiftEditor", "Shift click editor", 40);
        gui->RunFrame();
        SDL_Window* window = FindWindow("Shift click editor");
        editor->entry->RequestFocus(0);
        gui->RunFrame();
        editor->entry->SetText("abcdef");
        editor->entry->GotoTextStart();
        editor->entry->SelectNone();
        SDL_SetModState(SDL_KMOD_LSHIFT);
        Key(window, SDL_EVENT_KEY_DOWN, SDLK_LSHIFT, SDL_KMOD_LSHIFT);
        gui->RunFrame();
        Require(vgui2::input()->IsKeyDown(vgui2::KEY_LSHIFT), "Shift is held from the preceding frame");
        // SDL's live state is already at batch end before queued events dispatch.
        SDL_SetModState(SDL_KMOD_NONE);
        Click(window, 410.0f, 105.0f);
        Key(window, SDL_EVENT_KEY_UP, SDLK_LSHIFT, SDL_KMOD_NONE);
        gui->RunFrame();
        int start = -1;
        int end = -1;
        Require(editor->entry->GetSelectedRange(start, end) && start == 0 && end == 6,
                "real Shift-click extends selection before later Shift-up in the same batch");
        Text(window, "X");
        gui->RunFrame();
        char value[16]{};
        editor->entry->GetText(value, sizeof(value));
        Require(std::strcmp(value, "X") == 0, "text replaces the actual Shift-click selection");
        auto* traced = static_cast<FixtureTextEntry*>(editor->entry);
        Key(window, SDL_EVENT_KEY_DOWN, SDLK_LSHIFT, SDL_KMOD_LSHIFT);
        gui->RunFrame();
        Text(window, "Y");
        Key(nullptr, SDL_EVENT_KEY_UP, SDLK_LSHIFT, SDL_KMOD_NONE);
        gui->RunFrame();
        Require(traced->shiftDuringText, "text callback precedes windowless modifier release");
        Text(window, "Z");
        gui->RunFrame();
        Require(!traced->shiftDuringText, "next text callback observes prior windowless modifier release");
        delete editor;
    } else if (scenario == "resize_intent_sequences") {
        ResizeObserver observer;
        constexpr int SequenceWidth = 438;
        constexpr int InitialHeight = 173;
        constexpr int FinalHeight = 361;
        auto* growing = new MidBatchResizeFrame(SequenceWidth, InitialHeight, FinalHeight);
        auto* shrinking = new MidBatchResizeFrame(360, 420, 190);
        shrinking->SetTitle("Independent shrink", true);
        gui->RunFrame();
        auto* growWindow = FindWindow("Mid-batch resize");
        auto* shrinkWindow = FindWindow("Independent shrink");
        Require(growWindow && shrinkWindow && growWindow != shrinkWindow, "independent native resize owners");
        growing->nativeWindow = growWindow;
        shrinking->nativeWindow = shrinkWindow;
        growing->commandHeights = {450, 160, FinalHeight};
        growing->armed = true;
        shrinking->armed = true;
        observer.observe(growWindow, 0, 0, 0, nullptr);
        for (std::size_t command = 0; command < growing->commandHeights.size(); ++command) {
            gui->PostMessage(growing->GetVPanel(), new KeyValues("Command", "command", "ExpandDuringNativeBatch"),
                             0, 0.0f);
        }
        gui->PostMessage(shrinking->GetVPanel(), new KeyValues("Command", "command", "ExpandDuringNativeBatch"),
                         0, 0.0f);
        QueueWindowEvent(growWindow, SDL_EVENT_WINDOW_EXPOSED);
        QueueWindowEvent(shrinkWindow, SDL_EVENT_WINDOW_RESIZED);
        QueueWindowEvent(growWindow, SDL_EVENT_WINDOW_RESIZED);
        growing->inSurfaceFrame = true;
        shrinking->inSurfaceFrame = true;
        surface->RunFrame();
        growing->inSurfaceFrame = false;
        shrinking->inSurfaceFrame = false;
        Require(static_cast<std::size_t>(growing->commands) == growing->commandHeights.size() &&
                    shrinking->commands == 1,
                "ordinary queued grow/shrink commands dispatched");
        RequireGeometry(growing, growWindow, SequenceWidth, FinalHeight);
        RequireGeometry(shrinking, shrinkWindow, 360, 190);
        Require(observer.count(false) == 1 && observer.count(true) == 1,
                "only latest queued intent is submitted and completed once");
        growing->armed = false;
        shrinking->armed = false;
        constexpr int StabilityFrames = 3;
        for (int frame = 0; frame < StabilityFrames; ++frame) {
            gui->RunFrame();
        }
        Require(observer.count(false) == 1 && observer.count(true) == 1, "stable frames never resubmit geometry");
        RequireGeometry(growing, growWindow, SequenceWidth, FinalHeight);
        RequireGeometry(shrinking, shrinkWindow, 360, 190);
        observer.observe(nullptr, 0, 0, 0, nullptr);
        delete growing;
        delete shrinking;
    } else if (scenario == "resize_external_clamp_density") {
        ResizeObserver observer;
        observer.observe(firstWindow, 0, 0, 0, nullptr);
        Require(SDL_SetWindowSize(firstWindow, 430, 270), "genuine external native resize");
        gui->RunFrame();
        RequireGeometry(first, firstWindow, 430, 270);
        Require(observer.count(false) == 1 && observer.count(true) == 0,
                "native external resize is accepted without a retained resubmission");
        Require(SDL_SetWindowMaximumSize(firstWindow, 330, 240), "apply real SDL native size constraint");
        gui->RunFrame();
        observer.observe(firstWindow, 0, 0, 0, nullptr);
        first->SetSize(500, 400);
        gui->RunFrame();
        RequireGeometry(first, firstWindow, 330, 240);
        Require(observer.count(false) == 1 && observer.count(true) == 1, "actual SDL clamp resolves once");
        constexpr int StabilityFrames = 3;
        for (int frame = 0; frame < StabilityFrames; ++frame) {
            gui->RunFrame();
        }
        Require(observer.count(false) == 1 && observer.count(true) == 1, "clamp never waits for requested equality");
        observer.observe(firstWindow, 0, 0, 0, nullptr);
        first->SetMinimumSize(400, 280);
        gui->RunFrame();
        Require(first->GetWide() == 400 && first->GetTall() == 280,
                "retained minimum remains authoritative when native maximum is smaller");
        int clampedWide = 0;
        int clampedTall = 0;
        SDL_GetWindowSize(firstWindow, &clampedWide, &clampedTall);
        Require(clampedWide == 330 && clampedTall == 240, "actual native maximum remains accepted");
        for (int frame = 0; frame < StabilityFrames; ++frame) {
            gui->RunFrame();
        }
        Require(observer.count(false) == 1 && observer.count(true) == 1,
                "automatic retained minimum clamp does not create an endless native request loop");
        first->SetMinimumSize(0, 0);
        Require(SDL_SetWindowMaximumSize(firstWindow, 0, 0), "remove real native constraint");
        gui->RunFrame();
        observer.observe(firstWindow, 0, 0, 0, nullptr);
        first->SetSize(420, 280);
        gui->RunFrame();
        RequireGeometry(first, firstWindow, 420, 280);
        constexpr int DoubleDensity = 2;
        observer.density(static_cast<float>(DoubleDensity));
        QueueWindowEvent(firstWindow, SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
        gui->RunFrame();
        RequireGeometry(first, firstWindow, 420, 280, DoubleDensity);
        Require(observer.count(false) == 1 && observer.count(true) == 1,
                "density notification updates backing without logical-size resubmission");
        observer.density(1.0f);
        QueueWindowEvent(firstWindow, SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
        gui->RunFrame();
        RequireGeometry(first, firstWindow, 420, 280);
        observer.observe(nullptr, 0, 0, 0, nullptr);
    } else if (scenario == "resize_backing_failure_recovery") {
        ResizeObserver observer;
        auto blockBacking = reinterpret_cast<void (*)(bool)>(dlsym(RTLD_DEFAULT, "CseTestFailBackingAllocations"));
        auto failures = reinterpret_cast<int (*)()>(dlsym(RTLD_DEFAULT, "CseTestBackingAllocationFailures"));
        Require(blockBacking && failures, "continuous backing-allocation failure boundary loaded");
        constexpr int InitialWidth = 320;
        constexpr int InitialHeight = 200;
        constexpr int RequestedWidth = 410;
        constexpr int RequestedHeight = 270;
        RequireGeometry(first, firstWindow, InitialWidth, InitialHeight);
        observer.observe(firstWindow, 0, 0, 0, nullptr);
        blockBacking(true);
        first->SetSize(RequestedWidth, RequestedHeight);
        // Keep allocation blocked through the production surface's entire resize-event batch.
        gui->RunFrame();
        int nativeWidth = 0;
        int nativeHeight = 0;
        Require(SDL_GetWindowSize(firstWindow, &nativeWidth, &nativeHeight), "sample accepted logical resize");
        Require(first->GetWide() == RequestedWidth && first->GetTall() == RequestedHeight &&
                    nativeWidth == RequestedWidth && nativeHeight == RequestedHeight,
                "native and retained resize finish despite backing allocation failure");
        const auto readBacking = [&](int& width, int& height) {
            SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(firstWindow), nullptr);
            Require(pixels != nullptr, "read actual retained backing dimensions");
            width = pixels->w;
            height = pixels->h;
            SDL_DestroySurface(pixels);
        };
        int backingWidth = 0;
        int backingHeight = 0;
        readBacking(backingWidth, backingHeight);
        std::fprintf(stderr,
                     "BACKING_RECOVERY blocked native=%dx%d retained=%dx%d backing=%dx%d failures=%d set=%d sync=%d\n",
                     nativeWidth, nativeHeight, first->GetWide(), first->GetTall(), backingWidth, backingHeight,
                     failures(), observer.count(false), observer.count(true));
        Require(failures() > 0 && backingWidth == InitialWidth && backingHeight == InitialHeight,
                "complete batch preserves the old backing while its replacement allocation fails");
        Require(observer.count(false) == 1 && observer.count(true) == 1,
                "logical resize submits and completes only once while backing is unavailable");
        Require(!SDL_HasEvent(SDL_EVENT_WINDOW_RESIZED) && !SDL_HasEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED),
                "resize batch is exhausted before allocations recover");
        blockBacking(false);
        // No injected geometry event, new size request, RecreateContext or manual ApplyChanges.
        gui->RunFrame();
        readBacking(backingWidth, backingHeight);
        std::fprintf(stderr, "BACKING_RECOVERY ordinary-frame backing=%dx%d desired=%dx%d set=%d sync=%d\n",
                     backingWidth, backingHeight, RequestedWidth, RequestedHeight,
                     observer.count(false), observer.count(true));
        Require(observer.count(false) == 1 && observer.count(true) == 1,
                "backing-only recovery cannot repeat the terminal native request or completion");
        Require(backingWidth == RequestedWidth && backingHeight == RequestedHeight,
                "ordinary unchanged frame repairs pending backing allocation after resources recover");
        RequireGeometry(first, firstWindow, RequestedWidth, RequestedHeight);
        observer.observe(nullptr, 0, 0, 0, nullptr);
    } else if (scenario == "resize_boundary_outcomes") {
        ResizeObserver observer;
        constexpr int RejectRequest = 1;
        constexpr int TimeoutCompletion = 2;
        constexpr int ClampAtCompletion = 3;
        constexpr int FullscreenPolicy = 4;
        constexpr int StabilityFrames = 3;
        for (int mode : {RejectRequest, TimeoutCompletion, ClampAtCompletion, FullscreenPolicy}) {
            observer.observe(nullptr, 0, 0, 0, nullptr);
            first->SetSize(320, 200);
            gui->RunFrame();
            observer.observe(firstWindow, mode, 370, 230, nullptr);
            first->SetSize(450, 310);
            gui->RunFrame();
            const int acceptedWidth = mode == ClampAtCompletion ? 370 : 320;
            const int acceptedHeight = mode == ClampAtCompletion ? 230 : 200;
            RequireGeometry(first, firstWindow, acceptedWidth, acceptedHeight);
            const int requests = mode == FullscreenPolicy ? 0 : 1;
            const int completions = mode == RejectRequest || mode == FullscreenPolicy ? 0 : 1;
            Require(observer.count(false) == requests && observer.count(true) == completions,
                    "request rejection, finite timeout and delayed clamp each terminate once");
            for (int frame = 0; frame < StabilityFrames; ++frame) {
                gui->RunFrame();
            }
            Require(observer.count(false) == requests && observer.count(true) == completions,
                    "rejected or clamped fallback does not resubmit the old request");
            observer.observe(nullptr, 0, 0, 0, nullptr);
            Require(SDL_SetWindowSize(firstWindow, 380, 250), "external resize after terminal request outcome");
            gui->RunFrame();
            RequireGeometry(first, firstWindow, 380, 250);
            observer.observe(firstWindow, 0, 0, 0, nullptr);
            first->SetSize(450, 310);
            gui->RunFrame();
            RequireGeometry(first, firstWindow, 450, 310);
            Require(observer.count(false) == 1 && observer.count(true) == 1,
                    "same requested dimensions explicitly requested again form a new intent after fallback");
        }
        observer.observe(nullptr, 0, 0, 0, nullptr);
    } else if (scenario == "resize_reentrant_size_callback") {
        ResizeObserver observer;
        for (bool nested : {false, true}) {
            auto* callbackPanel = new ResizeCallbackPanel();
            surface->CreatePopup(callbackPanel->GetVPanel(), false, true, false, true, true);
            surface->SetTitle(callbackPanel->GetVPanel(), L"Reentrant size owner");
            gui->RunFrame();
            auto* window = FindWindow("Reentrant size owner");
            Require(window != nullptr, "actual reentrant size callback owner");
            int callbacks = 0;
            callbackPanel->resizeCallback = [&] {
                ++callbacks;
                callbackPanel->SetSize(460, 300);
                if (nested) {
                    surface->RunFrame();
                }
            };
            observer.observe(window, 0, 0, 0, nullptr);
            Require(SDL_SetWindowSize(window, 350, 240), "native resize invokes real OnSizeChanged");
            gui->RunFrame();
            Require(callbacks == 1, "one-shot actual size callback requests newer intent");
            RequireGeometry(callbackPanel, window, 460, 300);
            Require(observer.count(false) == 2 && observer.count(true) == 1,
                    "callback intent submits once after the one external resize");
            observer.observe(nullptr, 0, 0, 0, nullptr);
            delete callbackPanel;
        }
    } else if (scenario == "resize_sync_reentrancy") {
        ResizeObserver observer;
        constexpr int NewerIntent = 0;
        constexpr int DeleteAndReplace = 1;
        constexpr int ReplaceWindow = 2;
        for (int mode : {NewerIntent, DeleteAndReplace, ReplaceWindow}) {
            auto* target = new FixturePanel("Native completion callback");
            surface->CreatePopup(target->GetVPanel(), false, true, false, true, true);
            surface->SetTitle(target->GetVPanel(), L"Native completion callback");
            gui->RunFrame();
            auto* window = FindWindow("Native completion callback");
            Require(window != nullptr, "real owner before controlled completion callback");
            const auto oldHandle = gui->PanelToHandle(target->GetVPanel());
            const SDL_WindowID oldId = SDL_GetWindowID(window);
            int callbacks = 0;
            resizeSyncAction = [&] {
                ++callbacks;
                if (mode == NewerIntent) {
                    target->SetSize(470, 320);
                } else if (mode == DeleteAndReplace) {
                    delete target;
                    target = new FixturePanel("Completion replacement");
                    target->SetSize(370, 210);
                    surface->CreatePopup(target->GetVPanel(), false, true, false, true, true);
                } else {
                    surface->ReleasePanel(target->GetVPanel());
                    target->SetSize(370, 210);
                    surface->CreatePopup(target->GetVPanel(), false, true, false, true, true);
                }
                Require(SDL_GetWindowFromID(oldId) == window,
                        "retired native owner remains leased across the actual SDL completion call");
                surface->SetTitle(target->GetVPanel(), L"Native completion callback");
                surface->RunFrame();
            };
            observer.observe(window, 0, 0, 0, ResizeSyncCallback);
            target->SetSize(410, 270);
            gui->RunFrame();
            Require(callbacks == 1, "completion boundary invokes one nested frame/callback");
            auto* currentWindow = FindWindow("Native completion callback");
            Require(currentWindow != nullptr, "completion callback leaves a live current owner");
            if (mode == NewerIntent) {
                RequireGeometry(target, currentWindow, 470, 320);
                Require(observer.count(false) == 2 && observer.count(true) == 2,
                        "nested in-flight frame never resubmits; newer intent completes afterward");
            } else {
                RequireGeometry(target, currentWindow, 370, 210);
                Require(SDL_GetWindowID(currentWindow) != oldId, "replacement has a distinct native ID");
                Require(SDL_GetWindowFromID(oldId) == nullptr, "retired native owner closes after completion returns");
                Require((gui->HandleToPanel(oldHandle) != 0) == (mode == ReplaceWindow),
                        "window generation replacement and panel retirement remain distinct");
            }
            observer.observe(nullptr, 0, 0, 0, nullptr);
            delete target;
        }
    } else if (scenario == "resize_sync_shutdown") {
        ResizeObserver observer;
        const SDL_WindowID oldId = SDL_GetWindowID(firstWindow);
        bool callbackRan = false;
        resizeSyncAction = [&] {
            callbackRan = true;
            surface->Shutdown();
            Require(SDL_WasInit(SDL_INIT_VIDEO) != 0 && SDL_GetWindowFromID(oldId) == firstWindow,
                    "video subsystem and in-call owner survive surface shutdown until native completion returns");
        };
        observer.observe(firstWindow, 0, 0, 0, ResizeSyncCallback);
        constexpr int RequestedWidth = 410;
        constexpr int RequestedHeight = 270;
        first->SetSize(RequestedWidth, RequestedHeight);
        surface->RunFrame();
        Require(callbackRan && SDL_WasInit(SDL_INIT_VIDEO) == 0,
                "final native completion lease releases video after shutdown");
        observer.observe(nullptr, 0, 0, 0, nullptr);
    } else if (scenario == "resize_old_id_and_pending_move") {
        ResizeObserver observer;
        constexpr int MovedX = 75;
        constexpr int MovedY = 85;
        Require(SDL_SetWindowPosition(firstWindow, MovedX, MovedY), "pending genuine native move");
        first->SetSize(410, 260);
        QueueWindowEvent(firstWindow, SDL_EVENT_WINDOW_MOVED);
        QueueWindowEvent(firstWindow, SDL_EVENT_WINDOW_RESIZED);
        gui->RunFrame();
        int nativeX = 0;
        int nativeY = 0;
        SDL_GetWindowPosition(firstWindow, &nativeX, &nativeY);
        int retainedX = 0;
        int retainedY = 0;
        first->GetPos(retainedX, retainedY);
        Require(nativeX == MovedX && nativeY == MovedY && retainedX == MovedX && retainedY == MovedY,
                "new size intent preserves pending native move");
        RequireGeometry(first, firstWindow, 410, 260);
        RequireGeometry(second, secondWindow, 320, 200);
        const auto retired = gui->PanelToHandle(first->GetVPanel());
        QueueWindowEvent(firstWindow, SDL_EVENT_WINDOW_RESIZED);
        QueueWindowEvent(firstWindow, SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
        delete first;
        first = new FixturePanel("Resize ID replacement");
        first->SetSize(390, 230);
        surface->CreatePopup(first->GetVPanel(), false, true, false, true, true);
        surface->SetTitle(first->GetVPanel(), L"Resize ID replacement");
        gui->RunFrame();
        firstWindow = FindWindow("Resize ID replacement");
        Require(firstWindow && !gui->HandleToPanel(retired), "old serial handle is retired before old events route");
        RequireGeometry(first, firstWindow, 390, 230);
        RequireGeometry(second, secondWindow, 320, 200);
    } else if (scenario == "mid_batch_resize_intent" || scenario == "mid_batch_resize_intent_alternate") {
        constexpr int BrowserWidth = 572;
        constexpr int BrowserInitialHeight = 205;
        constexpr int BrowserExpandedHeight = 471;
        constexpr int AlternateWidth = 438;
        constexpr int AlternateInitialHeight = 173;
        constexpr int AlternateExpandedHeight = 389;
        const bool alternate = scenario == "mid_batch_resize_intent_alternate";
        const int wide = alternate ? AlternateWidth : BrowserWidth;
        const int initialTall = alternate ? AlternateInitialHeight : BrowserInitialHeight;
        const int requestedTall = alternate ? AlternateExpandedHeight : BrowserExpandedHeight;
        auto* frame = new MidBatchResizeFrame(wide, initialTall, requestedTall);
        gui->RunFrame();
        SDL_Window* window = FindWindow("Mid-batch resize");
        Require(window != nullptr, "actual retained Frame owns a native window");
        frame->nativeWindow = window;
        int nativeWide = 0;
        int nativeTall = 0;
        Require(SDL_GetWindowSize(window, &nativeWide, &nativeTall), "sample initial native size");
        Require(frame->GetWide() == wide && frame->GetTall() == initialTall &&
                    nativeWide == wide && nativeTall == initialTall,
                "retained and live native initial size have settled before ordering test");
        frame->armed = true;
        const SDL_WindowID windowId = SDL_GetWindowID(window);
        gui->PostMessage(frame->GetVPanel(), new KeyValues("Command", "command", "ExpandDuringNativeBatch"), 0, 0.0f);
        SDL_Event harmless{};
        harmless.type = SDL_EVENT_WINDOW_EXPOSED;
        harmless.window.windowID = windowId;
        harmless.common.timestamp = SDL_GetTicksNS();
        Require(SDL_PushEvent(&harmless), "queue harmless routed native event before resize");
        SDL_Event stale{};
        stale.type = SDL_EVENT_WINDOW_RESIZED;
        stale.window.windowID = windowId;
        stale.common.timestamp = SDL_GetTicksNS();
        stale.window.data1 = wide;
        stale.window.data2 = initialTall;
        Require(SDL_PushEvent(&stale), "queue later old native-size notification");
        const auto panelHandle = gui->PanelToHandle(frame->GetVPanel());
        std::fprintf(stderr, "RESIZE_ORDER queued window=%u handle=%u events=%u,%u initial=%dx%d desired=%dx%d\n",
                     windowId, panelHandle, harmless.type, stale.type, wide, initialTall, wide, requestedTall);
        // The production surface owns polling and the inter-event VGUI dispatcher. No fixture pump or sync.
        frame->inSurfaceFrame = true;
        surface->RunFrame();
        frame->inSurfaceFrame = false;
        Require(SDL_GetWindowSize(window, &nativeWide, &nativeTall), "sample final live native size");
        std::fprintf(stderr, "RESIZE_ORDER final commands=%d immediate=%d retained=%d native=%d later_callbacks=%zu\n",
                     frame->commands, frame->immediateTall, frame->GetTall(), nativeTall, frame->laterHeights.size());
        Require(frame->commands == 1 && frame->nativeTallAtCommand == initialTall &&
                    frame->immediateTall == requestedTall,
                "ordering setup: command requests new height while actual native height remains old");
        Require(SDL_GetWindowID(window) == windowId && gui->HandleToPanel(panelHandle) == frame->GetVPanel(),
                "resize retains the same native owner and serial panel handle");
        Require(frame->GetTall() == requestedTall,
                "latest retained size intent survives a later old native snapshot in the same batch");
        Require(nativeWide == wide && nativeTall == requestedTall, "native size converges to retained command");
        frame->armed = false;
        delete frame;
    } else if (scenario == "popup_event_generation") {
        first->SetPos(55, 65);
        SDL_Event oldMove{};
        oldMove.type = SDL_EVENT_WINDOW_MOVED;
        oldMove.window.windowID = SDL_GetWindowID(firstWindow);
        oldMove.window.data1 = 0;
        oldMove.window.data2 = 0;
        Require(SDL_PushEvent(&oldMove), "queue old native movement while retained position changes");
        gui->RunFrame();
        int nativeX = 0;
        int nativeY = 0;
        SDL_GetWindowPosition(firstWindow, &nativeX, &nativeY);
        Require(nativeX == 55 && nativeY == 65, "queued geometry does not replace newer retained placement");
        Text(firstWindow, "a");
        Text(secondWindow, "b");
        gui->RunFrame();
        Require(first->keyCount == 1 && first->lastCharacter == L'a', "first window text route");
        Require(second->keyCount == 1 && second->lastCharacter == L'b', "second window text route");
        const auto old = gui->PanelToHandle(first->GetVPanel());
        Text(firstWindow, "x");
        delete first;
        first = new FixturePanel("Replacement");
        surface->CreatePopup(first->GetVPanel(), false, true, false, true, true);
        gui->RunFrame();
        Require(gui->HandleToPanel(old) == 0 && first->keyCount == 0 && second->keyCount == 1,
                "stale queued input cannot reach replacement or other panel");
    } else if (scenario == "focus_capture_and_text") {
        Text(firstWindow, "ą€😀");
        gui->RunFrame();
        Require(first->keyCount == 4 && first->lastCharacter == 0xde00, "retained UTF-16 text input");
        surface->SetModalPanel(first->GetVPanel());
        Text(secondWindow, "blocked");
        Text(firstWindow, "m");
        gui->RunFrame();
        Require(first->keyCount == 5 && second->keyCount == 0, "app modal surface blocks foreign window input");
        surface->SetModalPanel(0);
        Text(secondWindow, "u");
        gui->RunFrame();
        Require(second->keyCount == 1, "clearing modal surface restores other window input");
        vgui2::input()->SetMouseCapture(first->GetVPanel());
        SDL_Event lost{};
        lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        lost.window.windowID = SDL_GetWindowID(firstWindow);
        SDL_PushEvent(&lost);
        gui->RunFrame();
        SDL_Event click{};
        click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        click.button.windowID = SDL_GetWindowID(secondWindow);
        click.button.button = SDL_BUTTON_LEFT;
        click.button.x = 8.0f;
        click.button.y = 8.0f;
        Require(SDL_PushEvent(&click), "queue second window click after capture loss");
        gui->RunFrame();
        Require(first->mouseCount == 0 && second->mouseCount == 1, "focus loss releases captured old panel");
        int routedX = 0;
        int routedY = 0;
        vgui2::input()->GetCursorPos(routedX, routedY);
        int secondX = 0;
        int secondY = 0;
        vgui2::ipanel()->GetAbsPos(second->GetVPanel(), secondX, secondY);
        Require(routedX == secondX + 8 && routedY == secondY + 8,
                "callbacks and frame hover retain the routed event coordinate");
        vgui2::system()->SetClipboardText(L"clipboard żółć", static_cast<int>(std::wcslen(L"clipboard żółć")));
        wchar_t text[64]{};
        vgui2::system()->GetClipboardText(0, text, sizeof(text));
        Require(std::wcscmp(text, L"clipboard żółć") == 0, "actual native clipboard copy");
        auto* editor = new ControlFrame(nullptr, "ShortcutEditor", "Shortcut editor", 40);
        gui->RunFrame();
        editor->entry->RequestFocus(0);
        SDL_Window* editorWindow = FindWindow("Shortcut editor");
        Require(editorWindow != nullptr, "actual TextEntry window");
        Key(editorWindow, SDL_EVENT_KEY_DOWN, SDLK_LCTRL, SDL_KMOD_LCTRL);
        Key(editorWindow, SDL_EVENT_KEY_DOWN, SDLK_A, SDL_KMOD_LCTRL);
        Key(editorWindow, SDL_EVENT_KEY_UP, SDLK_A, SDL_KMOD_LCTRL);
        Key(editorWindow, SDL_EVENT_KEY_UP, SDLK_LCTRL, SDL_KMOD_NONE);
        Text(editorWindow, "Replacement żółć");
        gui->RunFrame();
        char replacement[128]{};
        editor->entry->GetText(replacement, sizeof(replacement));
        std::printf("SHORTCUT_VALUE=%s\n", replacement);
        Require(std::strcmp(replacement, "Replacement żółć") == 0,
                "batched Ctrl+A retains event modifier state through real TextEntry callback");
        delete editor;
    } else if (scenario == "fractional_wheel_controls") {
        auto* firstText = new vgui2::TextEntry(first, "Scrollable first");
        auto* secondText = new vgui2::TextEntry(second, "Scrollable second");
        std::string lines;
        for (int line = 0; line < 100; ++line) {
            lines += "Retained scrolling line\n";
        }
        vgui2::TextEntry* entries[] = {firstText, secondText};
        for (auto* entry : entries) {
            entry->SetBounds(20, 20, 250, 100);
            entry->SetMultiline(true);
            entry->SetVerticalScrollbar(true);
            entry->SetText(lines.c_str());
        }
        gui->RunFrame();
        auto* firstBar = dynamic_cast<vgui2::ScrollBar*>(firstText->FindChildByName("ScrollBar", false));
        auto* secondBar = dynamic_cast<vgui2::ScrollBar*>(secondText->FindChildByName("ScrollBar", false));
        Require(firstBar && secondBar, "actual retained scrolling controls");
        firstBar->SetValue(30);
        secondBar->SetValue(30);
        const auto wheel = [&](SDL_Window* window, float delta, SDL_MouseWheelDirection direction) {
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_WHEEL;
            event.wheel.windowID = SDL_GetWindowID(window);
            event.wheel.y = delta;
            event.wheel.direction = direction;
            event.wheel.mouse_x = 30.0f;
            event.wheel.mouse_y = 30.0f;
            Require(SDL_PushEvent(&event), "queue fractional wheel");
            gui->RunFrame();
        };
        for (int event = 0; event < 4; ++event) {
            wheel(firstWindow, -0.25f, SDL_MOUSEWHEEL_NORMAL);
        }
        Require(firstBar->GetValue() == 33, "four fractional ticks scroll actual TextEntry by one retained detent");
        wheel(firstWindow, -0.75f, SDL_MOUSEWHEEL_NORMAL);
        wheel(secondWindow, -0.25f, SDL_MOUSEWHEEL_NORMAL);
        Require(secondBar->GetValue() == 30, "fractional remainder belongs to one native window");
        SDL_Event lost{};
        lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        lost.window.windowID = SDL_GetWindowID(firstWindow);
        Require(SDL_PushEvent(&lost), "queue wheel focus loss");
        gui->RunFrame();
        wheel(firstWindow, -0.25f, SDL_MOUSEWHEEL_NORMAL);
        Require(firstBar->GetValue() == 33, "focus loss clears unfinished wheel remainder");
        for (int event = 0; event < 3; ++event) {
            wheel(firstWindow, 0.25f, SDL_MOUSEWHEEL_FLIPPED);
        }
        Require(firstBar->GetValue() == 36, "SDL flipped direction is normalized before retained detent accumulation");
        wheel(firstWindow, 1.0f, SDL_MOUSEWHEEL_NORMAL);
        Require(firstBar->GetValue() == 33, "normal reverse direction scrolls back");
    } else if (scenario == "command_edit_accelerators" || scenario == "command_modifier_order") {
        Require(std::strcmp(SDL_GetCurrentVideoDriver(), "dummy") == 0,
                "clipboard regression uses isolated actual SDL dummy provider, never shared Cocoa pasteboard");
        delete first;
        delete second;
        first = nullptr;
        second = nullptr;
        auto* root = new FixturePanel("CommandRoot");
        surface->SetEmbeddedPanel(root->GetVPanel());
        auto* left = new ControlFrame(root, "CommandLeft", "Command editor A", 40);
        auto* right = new ControlFrame(root, "CommandRight", "Command editor B", 510);
        gui->RunFrame();
        SDL_Window* leftWindow = FindWindow("Command editor A");
        SDL_Window* rightWindow = FindWindow("Command editor B");
        Require(leftWindow && rightWindow, "command fixture uses actual retained controls in distinct windows");
        left->entry->SelectAllOnFirstFocus(false);
        right->entry->SelectAllOnFirstFocus(false);
        const auto value = [](vgui2::TextEntry* entry) {
            constexpr int TextCells = 128;
            wchar_t text[TextCells]{};
            entry->GetText(text, sizeof(text));
            return std::wstring(text);
        };
        const auto clipboard = []() {
            char* text = SDL_GetClipboardText();
            Require(text != nullptr, "read actual SDL clipboard provider");
            const std::string result(text);
            SDL_free(text);
            return result;
        };
        const auto selectAll = [&](SDL_Window* window, SDL_Keymod modifiers) {
            Key(window, SDL_EVENT_KEY_DOWN, SDLK_A, modifiers);
            Key(window, SDL_EVENT_KEY_UP, SDLK_A, modifiers);
            gui->RunFrame();
        };
        const auto action = [&](SDL_Window* window, SDL_Keycode key, SDL_Keymod modifiers) {
            Key(window, SDL_EVENT_KEY_DOWN, key, modifiers);
            Key(window, SDL_EVENT_KEY_UP, key, modifiers);
            gui->RunFrame();
        };
        const auto fullySelected = [](vgui2::TextEntry* entry, int length) {
            int start = -1;
            int end = -1;
            return entry->GetSelectedRange(start, end) && start == 0 && end == length;
        };
        constexpr wchar_t Original[] = L"Żółć — ą€";
        constexpr char OriginalUtf8[] = "Żółć — ą€";
        constexpr wchar_t Pasted[] = L"Schowek — żółć";
        constexpr char PastedUtf8[] = "Schowek — żółć";
        constexpr int OriginalLength = static_cast<int>(sizeof(Original) / sizeof(Original[0])) - 1;
        left->entry->SetText(Original);
        left->entry->RequestFocus(0);
        if (scenario == "command_edit_accelerators") {
            struct Modifier {
                SDL_Keycode key;
                SDL_Keymod mask;
                vgui2::KeyCode retained;
            };
            // Control is the established baseline; both native Command sides must
            // reach the same real editing actions without impersonating Control.
            const Modifier modifiers[] = {
                {SDLK_LCTRL, SDL_KMOD_LCTRL, vgui2::KEY_LCONTROL},
                {SDLK_LGUI, SDL_KMOD_LGUI, vgui2::KEY_LWIN},
                {SDLK_RGUI, SDL_KMOD_RGUI, vgui2::KEY_RWIN}
            };
            for (const auto& modifier : modifiers) {
                std::printf("EDIT_CASE modifier=%u retained=%d\n", modifier.mask, modifier.retained);
                std::fflush(stdout);
                left->entry->SetText(Original);
                left->entry->SelectNone();
                Key(leftWindow, SDL_EVENT_KEY_DOWN, modifier.key, modifier.mask);
                selectAll(leftWindow, modifier.mask);
                Require(fullySelected(left->entry, OriginalLength), "native edit modifier+A selects actual Unicode text");
                Require(vgui2::input()->IsKeyDown(modifier.retained), "native modifier keeps its own retained identity");
                Require(vgui2::input()->IsKeyDown(vgui2::KEY_LCONTROL) == (modifier.key == SDLK_LCTRL),
                        "Command editing never fabricates a Control press");
                Require(SDL_SetClipboardText("before copy"), "seed actual clipboard independently of control code");
                action(leftWindow, SDLK_C, modifier.mask);
                Require(clipboard() == OriginalUtf8 && value(left->entry) == Original,
                        "modifier+C publishes exact UTF8 through actual SystemMac/SDL provider without deleting");
                // Retained copy may reset the selection end to the caret. Select
                // explicitly before cut rather than changing that control behavior.
                selectAll(leftWindow, modifier.mask);
                action(leftWindow, SDLK_X, modifier.mask);
                Require(clipboard() == OriginalUtf8 && value(left->entry).empty(),
                        "modifier+X cuts real selection and publishes its exact Unicode contents");
                Require(SDL_SetClipboardText(PastedUtf8), "publish external non-ASCII data through actual SDL provider");
                action(leftWindow, SDLK_V, modifier.mask);
                Require(value(left->entry) == Pasted,
                        "modifier+V reads actual SystemMac/SDL clipboard and inserts exact non-ASCII text");
                Key(leftWindow, SDL_EVENT_KEY_UP, modifier.key, SDL_KMOD_NONE);
                gui->RunFrame();
                Require(!vgui2::input()->IsKeyDown(modifier.retained), "native edit modifier release clears its own state");
                std::printf("PASS_EDIT_CASE modifier=%u actual_copy_cut_paste=1\n", modifier.mask);
            }
        } else {
            // A native key event carries its modifiers even if the modifier key
            // event itself was outside the app/window. SDL live state is batch end.
            SDL_SetModState(SDL_KMOD_NONE);
            selectAll(leftWindow, SDL_KMOD_LGUI);
            Require(fullySelected(left->entry, OriginalLength), "Command snapshot without modifier key event selects all");
            Require(vgui2::input()->IsKeyDown(vgui2::KEY_LWIN) &&
                        !vgui2::input()->IsKeyDown(vgui2::KEY_LCONTROL),
                    "event-ordered Command snapshot preserves true Control state");
            Require(SDL_SetClipboardText(PastedUtf8), "seed real provider for ordered paste");
            Key(leftWindow, SDL_EVENT_KEY_DOWN, SDLK_V, SDL_KMOD_LGUI);
            Key(leftWindow, SDL_EVENT_KEY_UP, SDLK_V, SDL_KMOD_LGUI);
            Key(nullptr, SDL_EVENT_KEY_UP, SDLK_LGUI, SDL_KMOD_NONE);
            Text(leftWindow, "ę");
            gui->RunFrame();
            Require(value(left->entry) == L"Schowek — żółćę",
                    "paste observes Command before later windowless release; following native Unicode text is unchanged");
            Require(!vgui2::input()->IsKeyDown(vgui2::KEY_LWIN), "windowless Command release clears retained state");
            selectAll(leftWindow, SDL_KMOD_RGUI);
            Key(nullptr, SDL_EVENT_KEY_UP, SDLK_RGUI, SDL_KMOD_NONE);
            Key(leftWindow, SDL_EVENT_KEY_DOWN, SDLK_V, SDL_KMOD_NONE);
            Key(leftWindow, SDL_EVENT_KEY_UP, SDLK_V, SDL_KMOD_NONE);
            Text(leftWindow, "ł");
            gui->RunFrame();
            Require(value(left->entry) == L"ł", "released Command cannot paste; actual subsequent text replaces selection");
            Require(!vgui2::input()->IsKeyDown(vgui2::KEY_RWIN), "right windowless Command release clears retained state");
            left->entry->SetText(Original);
            constexpr SDL_Keymod CommandControl = SDL_KMOD_LGUI | SDL_KMOD_RCTRL;
            selectAll(leftWindow, CommandControl);
            Require(fullySelected(left->entry, OriginalLength) && vgui2::input()->IsKeyDown(vgui2::KEY_LWIN) &&
                        vgui2::input()->IsKeyDown(vgui2::KEY_RCONTROL),
                    "mixed Command and real Control retain independent states");
            Key(nullptr, SDL_EVENT_KEY_UP, SDLK_LGUI, SDL_KMOD_RCTRL);
            Key(leftWindow, SDL_EVENT_KEY_DOWN, SDLK_C, SDL_KMOD_RCTRL);
            Key(leftWindow, SDL_EVENT_KEY_UP, SDLK_C, SDL_KMOD_RCTRL);
            gui->RunFrame();
            Require(clipboard() == OriginalUtf8 && !vgui2::input()->IsKeyDown(vgui2::KEY_LWIN) &&
                        vgui2::input()->IsKeyDown(vgui2::KEY_RCONTROL),
                    "Command release preserves an independently held Control copy accelerator");
            Key(nullptr, SDL_EVENT_KEY_UP, SDLK_RCTRL, SDL_KMOD_RGUI);
            selectAll(leftWindow, SDL_KMOD_RGUI);
            Require(!vgui2::input()->IsKeyDown(vgui2::KEY_RCONTROL) &&
                        vgui2::input()->IsKeyDown(vgui2::KEY_RWIN),
                    "Control release preserves independent right Command accelerator");
            SDL_Event focus{};
            focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
            focus.window.windowID = SDL_GetWindowID(leftWindow);
            Require(SDL_PushEvent(&focus), "queue actual native focus loss while Command is held");
            right->entry->SetText(L"second");
            right->entry->RequestFocus(0);
            focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
            focus.window.windowID = SDL_GetWindowID(rightWindow);
            Require(SDL_PushEvent(&focus), "queue second window native focus gain");
            Key(rightWindow, SDL_EVENT_KEY_DOWN, SDLK_V, SDL_KMOD_NONE);
            Key(rightWindow, SDL_EVENT_KEY_UP, SDLK_V, SDL_KMOD_NONE);
            Text(rightWindow, "ą");
            gui->RunFrame();
            Require(value(left->entry) == Original && value(right->entry) == L"secondą",
                    "focus transition cancels old Command and routes native text only to new focused control");
            Require(!vgui2::input()->IsKeyDown(vgui2::KEY_LWIN) && !vgui2::input()->IsKeyDown(vgui2::KEY_RWIN),
                    "focus loss cannot leave either Command key held");
        }
        delete root;
    } else if (scenario == "focused_control_presentation") {
        auto observe = reinterpret_cast<void (*)(SDL_Renderer*)>(dlsym(RTLD_DEFAULT, "CseTestObservePresent"));
        auto count = reinterpret_cast<int (*)()>(dlsym(RTLD_DEFAULT, "CseTestPresentCount"));
        auto pixels = reinterpret_cast<const unsigned char* (*)(int, int*, int*)>(
            dlsym(RTLD_DEFAULT, "CseTestPresentPixels"));
        Require(observe && count && pixels, "actual SDL presentation observer available");
        delete first;
        delete second;
        first = nullptr;
        second = nullptr;
        auto* root = new FixturePanel("TemporalRoot");
        surface->SetEmbeddedPanel(root->GetVPanel());
        auto* editor = new ControlFrame(root, "TemporalEditor", "Temporal controls", 40);
        gui->RunFrame();
        SDL_Window* window = FindWindow("Temporal controls");
        Require(window != nullptr, "temporal test uses actual retained frame/native window");
        editor->entry->SetText(L"");
        editor->entry->RequestFocus(0);
        Key(window, SDL_EVENT_KEY_DOWN, SDLK_END, SDL_KMOD_NONE);
        Key(window, SDL_EVENT_KEY_UP, SDLK_END, SDL_KMOD_NONE);
        gui->RunFrame();
        Require(vgui2::input()->GetFocus() == editor->entry->GetVPanel(), "temporal fixture has real TextEntry focus");
        editor->entry->ResetCursorBlink();
        editor->entry->Repaint();
        auto frame = [&]() {
            observe(SDL_GetRenderer(window));
            gui->RunFrame();
            const int presentations = count();
            Require(presentations > 0, "retained frame publishes actual pixels");
            int width = 0;
            int height = 0;
            const unsigned char* complete = pixels(presentations - 1, &width, &height);
            constexpr int PixelChannels = 4;
            const std::size_t size = static_cast<std::size_t>(width) * height * PixelChannels;
            for (int index = 0; index < presentations; ++index) {
                const unsigned char* presented = pixels(index, &width, &height);
                if (std::memcmp(presented, complete, size) != 0) {
                    std::fprintf(stderr, "PARTIAL_PRESENT index=%d count=%d size=%dx%d\n",
                                 index, presentations, width, height);
                }
                Require(std::memcmp(presented, complete, size) == 0,
                        "every published frame contains all retained controls, never a partial traversal");
            }
            // FNV-1a records the exact observed pixels without deriving the expected frame.
            constexpr std::uint64_t HashOffsetBasis = 14695981039346656037ULL;
            constexpr std::uint64_t HashPrime = 1099511628211ULL;
            std::uint64_t hash = HashOffsetBasis;
            for (std::size_t index = 0; index < size; ++index) {
                hash = (hash ^ complete[index]) * HashPrime;
            }
            std::printf("PRESENT_FRAME count=%d size=%dx%d hash=%016llx\n", presentations, width, height,
                        static_cast<unsigned long long>(hash));
            std::vector<unsigned char> result(complete, complete + size);
            observe(nullptr);
            return result;
        };
        const auto baseline = frame();
        constexpr int IdleFrames = 60;
        constexpr int FrameIntervalMilliseconds = 16;
        constexpr int PixelChannels = 4;
        const int windowWidth = editor->GetWide();
        int entryX = 0;
        int entryY = 0;
        int entryWidth = 0;
        int entryHeight = 0;
        editor->entry->GetBounds(entryX, entryY, entryWidth, entryHeight);
        bool caretChanged = false;
        for (int tick = 0; tick < IdleFrames; ++tick) {
            SDL_Delay(FrameIntervalMilliseconds);
            const auto current = frame();
            Require(current.size() == baseline.size(), "focused idle preserves backing size");
            int changedColumn = -1;
            int changedPixels = 0;
            for (std::size_t offset = 0; offset < baseline.size(); offset += PixelChannels) {
                if (std::memcmp(current.data() + offset, baseline.data() + offset, PixelChannels) == 0) {
                    continue;
                }
                const int x = static_cast<int>(offset / PixelChannels % windowWidth);
                const int y = static_cast<int>(offset / PixelChannels / windowWidth);
                Require(x >= entryX && x < entryX + entryWidth && y >= entryY && y < entryY + entryHeight,
                        "focused idle preserves pixels outside actual TextEntry");
                Require(changedColumn == -1 || x == changedColumn,
                        "only the retained one-pixel caret column animates");
                changedColumn = x;
                ++changedPixels;
            }
            Require(changedPixels <= entryHeight, "caret animation never erases text-entry background");
            caretChanged = caretChanged || changedPixels > 0;
        }
        Require(caretChanged, "actual retained focus ticks keep caret animation alive");
        SDL_Event motion{};
        motion.type = SDL_EVENT_MOUSE_MOTION;
        motion.motion.windowID = SDL_GetWindowID(window);
        constexpr float CheckboxPointerX = 30.0f;
        constexpr float CheckboxPointerY = 149.0f;
        motion.motion.x = CheckboxPointerX;
        motion.motion.y = CheckboxPointerY;
        Require(SDL_PushEvent(&motion), "queue actual hover transition");
        frame();
        Click(window, CheckboxPointerX, CheckboxPointerY);
        frame();
        Require(editor->check->IsSelected(), "temporal fixture keeps actual checkbox input");
        editor->combo->ShowMenu();
        frame();
        Require(editor->combo->IsDropdownVisible(), "temporal fixture opens actual popup");
        editor->combo->HideMenu();
        frame();
        Require(!editor->combo->IsDropdownVisible(), "temporal fixture hides actual popup");
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        focus.window.windowID = SDL_GetWindowID(window);
        Require(SDL_PushEvent(&focus), "queue native focus loss");
        frame();
        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        Require(SDL_PushEvent(&focus), "queue native focus gain");
        frame();
        constexpr int ResizedWidth = 480;
        constexpr int ResizedHeight = 340;
        editor->SetSize(ResizedWidth, ResizedHeight);
        frame();
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window, &width, &height);
        Require(width == ResizedWidth && height == ResizedHeight, "temporal fixture resizes actual native backing");
        delete root;
    } else if (scenario == "resize_hidpi_clip") {
        first->SetSize(400, 240);
        surface->ApplyChanges();
        SDL_Event resize{};
        resize.type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
        resize.window.windowID = SDL_GetWindowID(firstWindow);
        Require(SDL_PushEvent(&resize), "queue actual window density notification");
        auto* child = new vgui2::Panel(first, "Clipped child");
        child->SetBounds(300, 180, 140, 100);
        child->SetPaintEnabled(false);
        child->SetPaintBackgroundEnabled(false);
        child->SetPaintBorderEnabled(false);
        gui->RunFrame();
        surface->SolveTraverse(first->GetVPanel(), false);
        surface->PushMakeCurrent(first->GetVPanel(), false);
        surface->DrawSetColor(0, 0, 0, 255);
        surface->DrawFilledRect(0, 0, 400, 240);
        surface->PushMakeCurrent(child->GetVPanel(), false);
        surface->DrawSetColor(0, 255, 0, 255);
        surface->DrawFilledRect(-4, -4, 144, 104);
        SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(firstWindow), nullptr);
        Require(pixels && pixels->w == 400 && pixels->h == 240, "surface backing follows actual window resize");
        Uint8 r = 0;
        Uint8 g = 0;
        Uint8 b = 0;
        Uint8 a = 0;
        SDL_ReadSurfacePixel(pixels, 299, 180, &r, &g, &b, &a);
        Require(r == 0 && g == 0 && b == 0, "retained child clip excludes left outside edge");
        SDL_ReadSurfacePixel(pixels, 300, 180, &r, &g, &b, &a);
        Require(r == 0 && g == 255 && b == 0, "retained child clip includes inside edge");
        SDL_ReadSurfacePixel(pixels, 399, 239, &r, &g, &b, &a);
        Require(r == 0 && g == 255 && b == 0, "child clip intersects parent right and bottom bounds");
        SDL_DestroySurface(pixels);
        surface->PopMakeCurrent(child->GetVPanel());
        surface->PopMakeCurrent(first->GetVPanel());
        delete child;
    } else if (scenario == "indexed_bitmap_fallback") {
        char directory[] = "/tmp/cse-standalone-bitmap-XXXXXX";
        Require(mkdtemp(directory) != nullptr, "create isolated bitmap asset directory");
        filesystem->AddSearchPath(directory, "SKIN");
        constexpr std::size_t PaletteEntries = 256;
        constexpr std::size_t HeaderBytes = sizeof(cse_bmp::FileHeader) + sizeof(cse_bmp::InfoHeader);
        constexpr std::size_t PixelOffset = HeaderBytes + PaletteEntries * sizeof(cse_bmp::PaletteEntry);
        constexpr int Width = 3;
        constexpr int Height = 2;
        constexpr std::size_t RowBytes = 4;
        std::vector<unsigned char> bitmap(PixelOffset + RowBytes * Height);
        cse_bmp::FileHeader fileHeader{};
        fileHeader.bfType = 0x4d42;
        fileHeader.bfSize = static_cast<std::uint32_t>(bitmap.size());
        fileHeader.bfOffBits = PixelOffset;
        cse_bmp::InfoHeader info{};
        info.biSize = sizeof(info);
        info.biWidth = Width;
        info.biHeight = Height;
        info.biPlanes = 1;
        info.biBitCount = 8;
        info.biSizeImage = RowBytes * Height;
        std::memcpy(bitmap.data(), &fileHeader, sizeof(fileHeader));
        std::memcpy(bitmap.data() + sizeof(fileHeader), &info, sizeof(info));
        // Opaque palette colors; BMP's reserved palette byte is not alpha.
        const cse_bmp::PaletteEntry palette[] = {{0, 0, 0, 0}, {0, 0, 255, 0},
            {0, 255, 0, 0}, {255, 0, 0, 0}};
        std::memcpy(bitmap.data() + HeaderBytes, palette, sizeof(palette));
        const unsigned char indices[] = {3, 1, 2, 0xee, 1, 2, 3, 0xee};
        std::memcpy(bitmap.data() + PixelOffset, indices, sizeof(indices));
        const std::string base = std::string(directory) + "/indexed";
        auto write = [&](const std::string& path, const std::vector<unsigned char>& bytes) {
            FILE* file = std::fopen(path.c_str(), "wb");
            Require(file != nullptr && std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size(),
                "write controlled texture asset");
            Require(std::fclose(file) == 0, "close controlled texture asset");
        };
        write(base + ".bmp", bitmap);
        const int texture = surface->CreateNewTextureID(true);
        surface->DrawSetTextureFile(texture, "indexed", 0, false);
        int wide = 0;
        int tall = 0;
        surface->DrawGetTextureSize(texture, wide, tall);
        Require(wide == Width && tall == Height, "missing TGA loads indexed BMP through retained filesystem");
        surface->SolveTraverse(first->GetVPanel(), false);
        surface->PushMakeCurrent(first->GetVPanel(), false);
        surface->DrawSetColor(0, 0, 0, 255);
        surface->DrawFilledRect(0, 0, 320, 200);
        auto check = [&](int width, int height, const unsigned char* expected) {
            surface->DrawSetTexture(texture);
            surface->DrawSetColor(255, 255, 255, 255);
            surface->DrawTexturedRect(4, 4, 4 + width, 4 + height);
            SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(firstWindow), nullptr);
            Require(pixels != nullptr, "read actual standalone texture pixels");
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    Uint8 red = 0;
                    Uint8 green = 0;
                    Uint8 blue = 0;
                    Uint8 alpha = 0;
                    SDL_ReadSurfacePixel(pixels, 4 + x, 4 + y, &red, &green, &blue, &alpha);
                    const unsigned char* color = expected + (y * width + x) * 3;
                    Require(red == color[0] && green == color[1] && blue == color[2] && alpha == 255,
                        "indexed BMP palette, bottom-up rows and padding reach actual render pixels");
                }
            }
            SDL_DestroySurface(pixels);
        };
        const unsigned char expected[] = {255, 0, 0, 0, 255, 0, 0, 0, 255,
            0, 0, 255, 255, 0, 0, 0, 255, 0};
        check(Width, Height, expected);
        bitmap.pop_back();
        write(base + ".bmp", bitmap);
        surface->DrawSetTextureFile(texture, "indexed", 0, true);
        check(Width, Height, expected);
        // Existing .tga takes precedence over .bmp, including after a cached load.
        const std::vector<unsigned char> tga = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            1, 0, 1, 0, 24, 0x20, 0, 255, 255};
        write(base + ".tga", tga);
        surface->DrawSetTextureFile(texture, "indexed", 0, true);
        surface->DrawGetTextureSize(texture, wide, tall);
        Require(wide == 1 && tall == 1, "TGA keeps precedence over BMP fallback");
        const unsigned char yellow[] = {255, 255, 0};
        check(1, 1, yellow);
        surface->PopMakeCurrent(first->GetVPanel());
        surface->DeleteTextureByID(texture);
        filesystem->RemoveSearchPath(directory);
        Require(std::remove((base + ".bmp").c_str()) == 0 && std::remove((base + ".tga").c_str()) == 0
            && rmdir(directory) == 0, "retire isolated texture assets");
    } else if (scenario == "glyph_texture_pixels") {
        surface->SolveTraverse(first->GetVPanel(), false);
        surface->PushMakeCurrent(first->GetVPanel(), false);
        surface->DrawSetColor(0, 0, 0, 255);
        surface->DrawFilledRect(0, 0, 320, 200);
        const unsigned char red[] = {255, 0, 0, 255};
        const int texture = surface->CreateNewTextureID(true);
        surface->DrawSetTextureRGBA(texture, red, 1, 1, 0, false);
        surface->DrawSetTexture(texture);
        surface->DrawSetColor(255, 255, 255, 255);
        surface->DrawTexturedRect(4, 4, 12, 12);
        surface->DrawSetTextFont(font);
        surface->DrawSetTextColor(255, 255, 255, 255);
        surface->DrawSetTextPos(20, 20);
        surface->DrawPrintText(L"V", 1);
        const auto symbols = surface->CreateFont();
        Require(surface->AddGlyphSetToFont(symbols, "Marlett", 16, 400, 0, 0, 0, 0, 0), "retained Marlett asset");
        surface->DrawSetTextFont(symbols);
        surface->DrawSetTextPos(60, 20);
        surface->DrawPrintText(L"r", 1);
        auto* closeIcon = vgui2::scheme()->GetImage("resource/icon_close", false);
        Require(closeIcon != nullptr, "retained Bitmap close icon");
        closeIcon->SetPos(100, 60);
        closeIcon->SetColor(Color(255, 255, 255, 255));
        surface->DrawSetTexture(0);
        closeIcon->Paint();
        SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(firstWindow), nullptr);
        Require(pixels != nullptr, "read actual raster pixels");
        const std::string executable(argv[0]);
        const std::string helperPath =
            executable.substr(0, executable.find_last_of('/') + 1) + "libglyph_reference.dylib";
        void* reference = dlopen(helperPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        Require(reference != nullptr, "load retained glyph reference");
        auto compare = reinterpret_cast<bool (*)(SDL_Surface*, vgui2::HFont, wchar_t, int, int)>(
            dlsym(reference, "CseTestCompareGlyph"));
        Require(compare && compare(pixels, font, L'V', 20, 20), "all glyph pixels equal retained raw coverage");
        Require(compare(pixels, symbols, L'r', 60, 20), "all retained close symbol glyph pixels");
        dlclose(reference);
        Uint8 r = 0;
        Uint8 g = 0;
        Uint8 b = 0;
        Uint8 a = 0;
        // Retained icon_close.tga: opaque source texel (2,1), BGR=(97,100,103).
        SDL_ReadSurfacePixel(pixels, 102, 61, &r, &g, &b, &a);
        Require(r == 103 && g == 100 && b == 97, "retained Bitmap selects its loaded file texture when painting");
        SDL_ReadSurfacePixel(pixels, 6, 6, &r, &g, &b, &a);
        Require(r == 255 && g == 0 && b == 0, "exact red texture pixel");
        SDL_ReadSurfacePixel(pixels, 13, 6, &r, &g, &b, &a);
        Require(r == 0 && g == 0 && b == 0, "texture edge excludes outside pixel");
        SDL_DestroySurface(pixels);
        surface->PopMakeCurrent(first->GetVPanel());
        const unsigned char blueBgra[] = {255, 0, 0, 255};
        const unsigned char greenBgra[] = {0, 255, 0, 255};
        surface->DrawSetTextureBGRA(texture, blueBgra, 1, 1);
        surface->PushMakeCurrent(first->GetVPanel(), false);
        surface->DrawSetTexture(texture);
        surface->DrawTexturedRect(4, 40, 12, 48);
        pixels = SDL_RenderReadPixels(SDL_GetRenderer(firstWindow), nullptr);
        SDL_ReadSurfacePixel(pixels, 6, 42, &r, &g, &b, &a);
        Require(r == 0 && g == 0 && b == 255, "BGRA replacement refreshes existing renderer texture");
        SDL_DestroySurface(pixels);
        surface->PopMakeCurrent(first->GetVPanel());
        surface->DrawUpdateRegionTextureBGRA(texture, 0, 0, greenBgra, 1, 1);
        surface->SolveTraverse(second->GetVPanel(), false);
        surface->PushMakeCurrent(second->GetVPanel(), false);
        surface->DrawSetTexture(texture);
        surface->DrawTexturedRect(4, 4, 12, 12);
        pixels = SDL_RenderReadPixels(SDL_GetRenderer(secondWindow), nullptr);
        SDL_ReadSurfacePixel(pixels, 6, 6, &r, &g, &b, &a);
        Require(r == 0 && g == 255 && b == 0, "region update reaches distinct renderer generation");
        SDL_DestroySurface(pixels);
        surface->PopMakeCurrent(second->GetVPanel());
        surface->DeleteTextureByID(texture);
    } else if (scenario == "native_help_handoff") {
        auto opened = reinterpret_cast<const char* (*)()>(dlsym(RTLD_DEFAULT, "CseTestLastExternalOpen"));
        Require(opened != nullptr, "native application handoff is intercepted before any GUI launch");
        vgui2::system()->ShellExecute("open", "Admin\\VarEditDialog_String.res");
        const std::string expected = std::string(argv[2]) + "/Admin/VarEditDialog_String.res";
        Require(expected == opened(), "native help-style resource path resolves through real retained filesystem");
        constexpr char HelpUrl[] = "https://example.invalid/help?a=b&c=d";
        vgui2::system()->ShellExecute("open", HelpUrl);
        Require(std::strcmp(opened(), HelpUrl) == 0, "native URL remains one unchanged spawn argument");
        vgui2::system()->ShellExecute("open", "Admin\\CseMissingHelpFixture.html");
        Require(std::strcmp(opened(), HelpUrl) == 0, "missing help resource never fabricates an application handoff");
    } else if (scenario == "ui_sound_lifetime") {
        auto fail = reinterpret_cast<void (*)(int)>(dlsym(RTLD_DEFAULT, "CseTestFailOwnerStage"));
        auto count = reinterpret_cast<int (*)()>(dlsym(RTLD_DEFAULT, "CseTestLiveAudioCount"));
        auto submitted = reinterpret_cast<bool (*)()>(dlsym(RTLD_DEFAULT, "CseTestAudioSubmitted"));
        Require(fail && count && submitted && count() == 0, "actual sound interposer ready");
        fail(4);
        surface->PlaySound("servers/game_ready.wav");
        Require(count() == 0, "failed SDL audio open transfers no stream ownership");
        surface->PlaySound("servers/game_ready.wav");
        Require(count() == 1 && submitted(), "retained WAV samples reach and flush actual SDL stream");
        surface->PlaySound("servers/game_ready.wav");
        Require(count() == 1, "replacement sound retires previous stream");
        delete first;
        delete second;
        first = nullptr;
        second = nullptr;
        surface->Shutdown();
        Require(count() == 0 && SDL_WasInit(SDL_INIT_AUDIO) == 0, "sound stream retires before audio subsystem");
    } else if (scenario == "surface_partial_failure_reopen") {
        delete first;
        delete second;
        surface->Shutdown();
        auto fail = reinterpret_cast<void (*)(int)>(dlsym(RTLD_DEFAULT, "CseTestFailOwnerStage"));
        auto live = reinterpret_cast<int (*)()>(dlsym(RTLD_DEFAULT, "CseTestLiveOwnerCount"));
        Require(fail && live && live() == 0, "interposer owns no leaked SDL objects after close");
        constexpr int CreationStages[] = {1, 2, 3, 5}; // Window, renderer, backing texture, text input.
        for (int stage : CreationStages) {
            auto* failed = new FixturePanel("Failure");
            fail(stage);
            surface->CreatePopup(failed->GetVPanel(), false, true, false, true, true);
            Require(surface->GetPopupCount() == 0 && live() == 0, "real SDL partial creation rolls back");
            surface->CreatePopup(failed->GetVPanel(), false, true, false, true, true);
            Require(surface->GetPopupCount() == 1 && live() > 0, "same actual owner retries after boundary failure");
            delete failed;
            Require(surface->GetPopupCount() == 0 && live() == 0, "textures precede renderer/window retirement");
        }
        first = new FixturePanel("Reopened");
        second = new FixturePanel("Reopened2");
        surface->CreatePopup(first->GetVPanel(), false, true, false, true, true);
        surface->CreatePopup(second->GetVPanel(), false, true, false, true, true);
        Require(surface->GetPopupCount() == 2, "reopen actual SDL owners");
    }
    delete first;
    delete second;
    gui->RunFrame();
    gui->Shutdown();
    vgui2::system()->Shutdown();
    vgui2::scheme()->Shutdown(true);
    surface->Shutdown();
    vgui2::VGuiControls_Shutdown();
    if (scenario == "cursor_ownership") {
        auto count = reinterpret_cast<int (*)(int)>(dlsym(RTLD_DEFAULT, "CseCursorCount"));
        constexpr int LiveCursors = 3;
        Require(count && count(LiveCursors) == 0, "final shutdown retires reopened cursor");
    }
    if (scenario == "surface_partial_failure_reopen") {
        Require(vgui2::VGuiControls_Init("STANDALONE_REOPEN", factories, 2), "reinitialize all retained owners");
        Require(vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", nullptr) != 0,
                "reload retained scheme after complete shutdown");
        gui = vgui2::ivgui();
        surface = vgui2::surface();
        gui->Start();
        auto* reopened = new ControlFrame(nullptr, "Reopened controls", "Reopened controls", 40);
        gui->RunFrame();
        Require(FindWindow("Reopened controls") != nullptr,
                "full retained controls reopen renders a new native window");
        delete reopened;
        gui->RunFrame();
        gui->Shutdown();
        vgui2::system()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        surface->Shutdown();
        vgui2::VGuiControls_Shutdown();
        std::puts("PASS complete retained controls/scheme/font/surface shutdown and reopen");
    }
    filesystem->Unmount();
    dlclose(guiImage);
    dlclose(filesystemImage);
    std::printf("PASS %s\n", argv[3]);
    return 0;
}
