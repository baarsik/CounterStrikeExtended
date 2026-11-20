#include "../../../src/cstrike/cl_dll/VGUI/server_form_keys.h"
#include "../../../src/cstrike/cl_dll/VGUI/server_form_text.h"

#include <array>
#include <cstdio>

int main() {
    using cse::forms::FormHotkey;

    constexpr std::array Keypad{
        K_KP_INS, K_KP_END, K_KP_DOWNARROW, K_KP_PGDN, K_KP_LEFTARROW,
        K_KP_5, K_KP_RIGHTARROW, K_KP_HOME, K_KP_UPARROW, K_KP_PGUP,
    };
    for (std::size_t digit = 0; digit < Keypad.size(); ++digit) {
        const char expected = static_cast<char>('0' + digit);
        if (FormHotkey(Keypad[digit]) != expected || FormHotkey(expected) != expected) {
            std::fprintf(stderr, "Main row and keypad must both map to shortcut %c\n", expected);
            return 1;
        }
    }
    for (int index = 0; index <= 'z' - 'a'; ++index) {
        const char expected = static_cast<char>('a' + index);
        if (FormHotkey('A' + index) != expected || FormHotkey('a' + index) != expected) {
            std::fprintf(stderr, "Both letter cases must map to shortcut %c\n", expected);
            return 1;
        }
    }
    for (int key : {K_KP_ENTER, K_KP_DEL, K_SHIFT, K_ESCAPE, K_UPARROW}) {
        if (FormHotkey(key)) {
            std::fprintf(stderr, "Non-letter or digit key %d must not map to a shortcut\n", key);
            return 1;
        }
    }
    if (cse::forms::FormWindowTitle(u8"#settings & \u017c") != L"Server form - #settings && \u017c") {
        std::fprintf(stderr, "A server title must retain the client prefix and remain literal UTF-8 text\n");
        return 1;
    }
}
