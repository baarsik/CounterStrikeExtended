// Cocoa event scoping and dynamic loading follow CEF's BSD-licensed cefsimple example.
#import <Cocoa/Cocoa.h>

#include "cef_host.h"
#include "include/cef_application_mac.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_library_loader.h"

@interface CseHostApplication : NSApplication <CefAppProtocol> {
    BOOL handlingSendEvent_;
}
@end

@implementation CseHostApplication
- (BOOL)isHandlingSendEvent {
    return handlingSendEvent_;
}
- (void)setHandlingSendEvent:(BOOL)value {
    handlingSendEvent_ = value;
}
- (void)sendEvent:(NSEvent*)event {
    CefScopedSendingEvent eventScope;
    [super sendEvent:event];
}
- (void)terminate:(id)sender {
    CloseCefHost();
}
@end

int main(int argc, char* argv[]) {
    CefScopedLibraryLoader loader;
    if (!loader.LoadInMain()) {
        return 1;
    }
    @autoreleasepool {
        [CseHostApplication sharedApplication];
        if (![NSApp isKindOfClass:[CseHostApplication class]]) {
            return 1;
        }
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        CefRefPtr<CefCommandLine> commandLine = CefCommandLine::CreateCommandLine();
        commandLine->InitFromArgv(argc, argv);
        return RunCefHost(CefMainArgs(argc, argv), commandLine, nullptr);
    }
}

// The wire carries the legacy Windows virtual-key namespace. CEF's macOS adapter ignores that field
// and derives the key from a synthetic NSEvent's native keycode and UTF-16 characters.
#include <Carbon/Carbon.h>

bool PrepareCefHostMacKey(CefKeyEvent& event) {
    enum VirtualKey {
        Backspace = 0x08, Tab = 0x09, Return = 0x0d, Shift = 0x10, Control = 0x11, Alt = 0x12,
        Pause = 0x13, CapsLock = 0x14, Escape = 0x1b, Space = 0x20, PageUp = 0x21, PageDown = 0x22,
        End = 0x23, Home = 0x24, Left = 0x25, Up = 0x26, Right = 0x27, Down = 0x28,
        Insert = 0x2d, Delete = 0x2e, FirstDigit = 0x30, FirstLetter = 0x41,
        LeftWindows = 0x5b, RightWindows = 0x5c, Application = 0x5d,
        FirstKeypad = 0x60, Multiply = 0x6a, Add = 0x6b, Separator = 0x6c, Subtract = 0x6d,
        Decimal = 0x6e, Divide = 0x6f, FirstFunction = 0x70, NumLock = 0x90, ScrollLock = 0x91,
        LeftShift = 0xa0, RightShift = 0xa1, LeftControl = 0xa2, RightControl = 0xa3,
        LeftAlt = 0xa4, RightAlt = 0xa5,
        Semicolon = 0xba, Equals = 0xbb, Comma = 0xbc, Minus = 0xbd, Period = 0xbe, Slash = 0xbf,
        Backquote = 0xc0, LeftBracket = 0xdb, Backslash = 0xdc, RightBracket = 0xdd, Quote = 0xde
    };
    constexpr unsigned short Letters[] = {kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E,
        kVK_ANSI_F, kVK_ANSI_G, kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J, kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M,
        kVK_ANSI_N, kVK_ANSI_O, kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T, kVK_ANSI_U,
        kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y, kVK_ANSI_Z};
    constexpr unsigned short Digits[] = {kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
        kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9};
    constexpr unsigned short Keypad[] = {kVK_ANSI_Keypad0, kVK_ANSI_Keypad1, kVK_ANSI_Keypad2,
        kVK_ANSI_Keypad3, kVK_ANSI_Keypad4, kVK_ANSI_Keypad5, kVK_ANSI_Keypad6, kVK_ANSI_Keypad7,
        kVK_ANSI_Keypad8, kVK_ANSI_Keypad9};
    constexpr unsigned short Functions[] = {kVK_F1, kVK_F2, kVK_F3, kVK_F4, kVK_F5, kVK_F6, kVK_F7,
        kVK_F8, kVK_F9, kVK_F10, kVK_F11, kVK_F12};
    const int key = event.windows_key_code;
    if (key == Return && (event.modifiers & EVENTFLAG_IS_KEY_PAD) != 0) {
        event.native_key_code = kVK_ANSI_KeypadEnter;
        // AppKit distinguishes keypad Enter (U+0003) from Return (U+000D). CEF derives DOM key/location
        // from this native NSEvent character as well as its hardware keycode.
        event.character = static_cast<char16_t>(NSEnterCharacter);
        event.unmodified_character = event.character;
        return true;
    }
    if (key >= FirstLetter && key < FirstLetter + static_cast<int>(std::size(Letters))) {
        event.native_key_code = Letters[key - FirstLetter];
        event.unmodified_character = static_cast<char16_t>(key - FirstLetter + 'a');
        event.character = (event.modifiers & EVENTFLAG_SHIFT_DOWN) != 0 ?
            static_cast<char16_t>(key) : event.unmodified_character;
    } else if (key >= FirstDigit && key < FirstDigit + static_cast<int>(std::size(Digits))) {
        event.native_key_code = Digits[key - FirstDigit];
        event.character = static_cast<char16_t>(key);
        event.unmodified_character = event.character;
    } else if (key >= FirstKeypad && key < FirstKeypad + static_cast<int>(std::size(Keypad))) {
        event.native_key_code = Keypad[key - FirstKeypad];
        event.modifiers |= EVENTFLAG_IS_KEY_PAD;
        event.character = static_cast<char16_t>('0' + key - FirstKeypad);
        event.unmodified_character = event.character;
    } else if (key >= FirstFunction && key < FirstFunction + static_cast<int>(std::size(Functions))) {
        event.native_key_code = Functions[key - FirstFunction];
        event.character = static_cast<char16_t>(NSF1FunctionKey + key - FirstFunction);
        event.unmodified_character = event.character;
    } else {
        struct Mapping {
            int key;
            unsigned short native;
            char16_t character;
        };
        constexpr Mapping Special[] = {
            {Backspace, kVK_Delete, 0x7f}, {Tab, kVK_Tab, '\t'}, {Return, kVK_Return, '\r'},
            {Shift, kVK_Shift, 0}, {Control, kVK_Control, 0}, {Alt, kVK_Option, 0},
            {LeftShift, kVK_Shift, 0}, {RightShift, kVK_RightShift, 0},
            {LeftControl, kVK_Control, 0}, {RightControl, kVK_RightControl, 0},
            {LeftAlt, kVK_Option, 0}, {RightAlt, kVK_RightOption, 0},
            {LeftWindows, kVK_Command, 0}, {RightWindows, kVK_RightCommand, 0},
            {Application, kVK_Help, NSMenuFunctionKey}, {Pause, kVK_F15, NSPauseFunctionKey},
            {NumLock, kVK_ANSI_KeypadClear, NSClearLineFunctionKey},
            {ScrollLock, kVK_F14, NSScrollLockFunctionKey},
            {Multiply, kVK_ANSI_KeypadMultiply, '*'}, {Add, kVK_ANSI_KeypadPlus, '+'},
            {Separator, kVK_JIS_KeypadComma, ','}, {Subtract, kVK_ANSI_KeypadMinus, '-'},
            {Decimal, kVK_ANSI_KeypadDecimal, '.'}, {Divide, kVK_ANSI_KeypadDivide, '/'},
            {CapsLock, kVK_CapsLock, 0}, {Escape, kVK_Escape, 0x1b}, {Space, kVK_Space, ' '},
            {PageUp, kVK_PageUp, NSPageUpFunctionKey}, {PageDown, kVK_PageDown, NSPageDownFunctionKey},
            {Home, kVK_Home, NSHomeFunctionKey}, {End, kVK_End, NSEndFunctionKey},
            {Left, kVK_LeftArrow, NSLeftArrowFunctionKey}, {Right, kVK_RightArrow, NSRightArrowFunctionKey},
            {Up, kVK_UpArrow, NSUpArrowFunctionKey}, {Down, kVK_DownArrow, NSDownArrowFunctionKey},
            {Insert, kVK_Help, NSInsertFunctionKey}, {Delete, kVK_ForwardDelete, NSDeleteFunctionKey},
            {Semicolon, kVK_ANSI_Semicolon, ';'}, {Equals, kVK_ANSI_Equal, '='},
            {Comma, kVK_ANSI_Comma, ','}, {Minus, kVK_ANSI_Minus, '-'}, {Period, kVK_ANSI_Period, '.'},
            {Slash, kVK_ANSI_Slash, '/'}, {Backquote, kVK_ANSI_Grave, '`'},
            {LeftBracket, kVK_ANSI_LeftBracket, '['}, {Backslash, kVK_ANSI_Backslash, '\\'},
            {RightBracket, kVK_ANSI_RightBracket, ']'}, {Quote, kVK_ANSI_Quote, '\''}
        };
        for (const auto& mapping : Special) {
            if (key == mapping.key) {
                event.native_key_code = mapping.native;
                event.character = mapping.character;
                event.unmodified_character = mapping.character;
                return true;
            }
        }
        return false;
    }
    return true;
}

void SetCefHostNativeTitle(CefWindowHandle window, const CefString& title) {
    NSView* view = (__bridge NSView*)window;
    [[view window] setTitle:[NSString stringWithUTF8String:title.ToString().c_str()]];
}

bool IsCefHostNativeWindow(CefWindowHandle window) {
    NSView* view = (__bridge NSView*)window;
    return view != nil && [view window] != nil;
}

namespace {
class CocoaParent final : public CefHostNativeParent {
public:
    CocoaParent() {
        constexpr CGFloat DialogParentWidth = 800;
        constexpr CGFloat DialogParentHeight = 600;
        window_ = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, DialogParentWidth, DialogParentHeight)
            styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
        [window_ setReleasedWhenClosed:NO];
        [window_ setTitle:@"CounterStrikeExtended Browser Dialogs"];
        [window_ center];
        [window_ orderOut:nil];
    }

    ~CocoaParent() override {
        [window_ close];
    }

    CefWindowHandle Handle() const override {
        return (__bridge CefWindowHandle)[window_ contentView];
    }

private:
    NSWindow* __strong window_ = nil;
};
}

std::unique_ptr<CefHostNativeParent> CreateCefHostNativeParent() {
    return std::make_unique<CocoaParent>();
}
