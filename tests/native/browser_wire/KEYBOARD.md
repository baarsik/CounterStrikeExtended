# Native browser keyboard boundary

Native HTML consumers convert VGUI keys explicitly to Windows virtual-key values,
the namespace used by CEF on both required platforms. They do not reinterpret SDL
keycodes as virtual keys. Character input retains the separate UTF-16/scalar path.
Alt, Control and Shift retain bits 1, 2 and 4; native keypad keys also set bit 8,
so keypad Enter can share `VK_RETURN` while retaining its location.

The maintained Win32 HTML adapters retain their existing conversion/modifier
path. The common SDL input switch additionally recognizes `SDLK_APPLICATION`,
which was present in the translation table but missing from that switch.

The fixture compiles the real SDL-to-VGUI translation source. It covers every
VGUI enum value, 103 SDL inputs, all eight legacy modifier combinations, keypad
location, toggle keys and invalid values. Isolated tests of both actual HTML
adapter bodies also exercise their native and legacy branches. These are input
boundary checks; real browser DOM events, IME and whole-game input remain
separate acceptance gates.

```sh
cmake -S tests/native/browser_wire -B build/browser-keys -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCSE_TARGET_ARCH=arm64
cmake --build build/browser-keys --target browser_key_tests
ctest --test-dir build/browser-keys -R native-browser-key-namespace --output-on-failure
```

Use the matching MSVC environment and target architecture on Windows. Release
and Debug fixtures passed Windows x86/x64/ARM64; macOS ARM64 also passed
ASan/UBSan. This does not promote any complete native game target.
