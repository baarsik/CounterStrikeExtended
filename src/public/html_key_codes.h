#pragma once

#include "vgui/KeyCode.h"
#include "html_key_modifiers.h"

namespace cse {
namespace html {

constexpr int KeyModifiers(int code, int modifiers) {
    if (code >= vgui2::KEY_PAD_0 && code <= vgui2::KEY_PAD_DECIMAL) {
        return modifiers | KeypadModifier;
    }
    return modifiers;
}

// CEF consumes Windows virtual-key values even when SDL supplies input on
// another platform. Values follow Microsoft's Virtual-Key Codes table:
// https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
// Accept the numeric KeyCode so out-of-range input can be rejected as zero.
constexpr int WindowsVirtualKey(int code) {
    constexpr int Invalid = 0;
    constexpr int FirstDigit = '0';
    constexpr int FirstLetter = 'A';
    constexpr int FirstKeypadDigit = 0x60;
    constexpr int FirstFunctionKey = 0x70;
    constexpr int Backspace = 0x08;
    constexpr int Tab = 0x09;
    constexpr int Return = 0x0d;
    constexpr int Pause = 0x13;
    constexpr int CapsLock = 0x14;
    constexpr int Escape = 0x1b;
    constexpr int Space = 0x20;
    constexpr int PageUp = 0x21;
    constexpr int PageDown = 0x22;
    constexpr int End = 0x23;
    constexpr int Home = 0x24;
    constexpr int Left = 0x25;
    constexpr int Up = 0x26;
    constexpr int Right = 0x27;
    constexpr int Down = 0x28;
    constexpr int Insert = 0x2d;
    constexpr int Delete = 0x2e;
    constexpr int LeftWindows = 0x5b;
    constexpr int RightWindows = 0x5c;
    constexpr int Application = 0x5d;
    constexpr int Multiply = 0x6a;
    constexpr int Add = 0x6b;
    constexpr int Subtract = 0x6d;
    constexpr int Decimal = 0x6e;
    constexpr int Divide = 0x6f;
    constexpr int NumLock = 0x90;
    constexpr int ScrollLock = 0x91;
    constexpr int LeftShift = 0xa0;
    constexpr int RightShift = 0xa1;
    constexpr int LeftControl = 0xa2;
    constexpr int RightControl = 0xa3;
    constexpr int LeftAlt = 0xa4;
    constexpr int RightAlt = 0xa5;
    constexpr int Semicolon = 0xba;
    constexpr int Equals = 0xbb;
    constexpr int Comma = 0xbc;
    constexpr int Minus = 0xbd;
    constexpr int Period = 0xbe;
    constexpr int Slash = 0xbf;
    constexpr int Backquote = 0xc0;
    constexpr int LeftBracket = 0xdb;
    constexpr int Backslash = 0xdc;
    constexpr int RightBracket = 0xdd;
    constexpr int Apostrophe = 0xde;

    if (code >= vgui2::KEY_0 && code <= vgui2::KEY_9) {
        return FirstDigit + code - vgui2::KEY_0;
    }
    if (code >= vgui2::KEY_A && code <= vgui2::KEY_Z) {
        return FirstLetter + code - vgui2::KEY_A;
    }
    if (code >= vgui2::KEY_PAD_0 && code <= vgui2::KEY_PAD_9) {
        return FirstKeypadDigit + code - vgui2::KEY_PAD_0;
    }
    if (code >= vgui2::KEY_F1 && code <= vgui2::KEY_F12) {
        return FirstFunctionKey + code - vgui2::KEY_F1;
    }

    switch (code) {
    case vgui2::KEY_PAD_DIVIDE:
        return Divide;
    case vgui2::KEY_PAD_MULTIPLY:
        return Multiply;
    case vgui2::KEY_PAD_MINUS:
        return Subtract;
    case vgui2::KEY_PAD_PLUS:
        return Add;
    case vgui2::KEY_PAD_ENTER:
    case vgui2::KEY_ENTER:
        return Return;
    case vgui2::KEY_PAD_DECIMAL:
        return Decimal;
    case vgui2::KEY_LBRACKET:
        return LeftBracket;
    case vgui2::KEY_RBRACKET:
        return RightBracket;
    case vgui2::KEY_SEMICOLON:
        return Semicolon;
    case vgui2::KEY_APOSTROPHE:
        return Apostrophe;
    case vgui2::KEY_BACKQUOTE:
        return Backquote;
    case vgui2::KEY_COMMA:
        return Comma;
    case vgui2::KEY_PERIOD:
        return Period;
    case vgui2::KEY_SLASH:
        return Slash;
    case vgui2::KEY_BACKSLASH:
        return Backslash;
    case vgui2::KEY_MINUS:
        return Minus;
    case vgui2::KEY_EQUAL:
        return Equals;
    case vgui2::KEY_SPACE:
        return Space;
    case vgui2::KEY_BACKSPACE:
        return Backspace;
    case vgui2::KEY_TAB:
        return Tab;
    case vgui2::KEY_CAPSLOCK:
    case vgui2::KEY_CAPSLOCKTOGGLE:
        return CapsLock;
    case vgui2::KEY_NUMLOCK:
    case vgui2::KEY_NUMLOCKTOGGLE:
        return NumLock;
    case vgui2::KEY_ESCAPE:
        return Escape;
    case vgui2::KEY_SCROLLLOCK:
    case vgui2::KEY_SCROLLLOCKTOGGLE:
        return ScrollLock;
    case vgui2::KEY_INSERT:
        return Insert;
    case vgui2::KEY_DELETE:
        return Delete;
    case vgui2::KEY_HOME:
        return Home;
    case vgui2::KEY_END:
        return End;
    case vgui2::KEY_PAGEUP:
        return PageUp;
    case vgui2::KEY_PAGEDOWN:
        return PageDown;
    case vgui2::KEY_BREAK:
        return Pause;
    case vgui2::KEY_LSHIFT:
        return LeftShift;
    case vgui2::KEY_RSHIFT:
        return RightShift;
    case vgui2::KEY_LALT:
        return LeftAlt;
    case vgui2::KEY_RALT:
        return RightAlt;
    case vgui2::KEY_LCONTROL:
        return LeftControl;
    case vgui2::KEY_RCONTROL:
        return RightControl;
    case vgui2::KEY_LWIN:
        return LeftWindows;
    case vgui2::KEY_RWIN:
        return RightWindows;
    case vgui2::KEY_APP:
        return Application;
    case vgui2::KEY_UP:
        return Up;
    case vgui2::KEY_LEFT:
        return Left;
    case vgui2::KEY_DOWN:
        return Down;
    case vgui2::KEY_RIGHT:
        return Right;
    default:
        return Invalid;
    }
}

}
}
