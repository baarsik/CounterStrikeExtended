# macOS gameplay cursor policy

Build the native client, then configure with `CSE_CLIENT_RUNTIME` pointing to
its runtime output. Run `ctest --test-dir` on the configured fixture directory.
The fixture links the actual SDL provider and actual engine cursor owner and
creates a hidden Cocoa window. It changes only the fixture app's presentation
options; it does not change the user's mouse accessibility preference.

Checks cover gameplay and menu transitions with raw input on/off, alternating
VGUI owners, an abandoned transaction, release without focus, restoration of an
existing assistance bit, preservation of newer unrelated options, and repair
after replacement of the presentation mask. The transaction component separately
checks provider failure, retry, reentry and refusal to discard failed cleanup.
This does not establish physical shake behavior or actual fullscreen gameplay.

The app-local policy uses Apple's
[cursor-location-assistance presentation option](https://developer.apple.com/documentation/appkit/nsapplication/presentationoptions-swift.struct/disablecursorlocationassistance).

The keyboard case loads the actual engine before `Run` and checks its earliest
global UI constructor overrides SDL3.4's press-and-hold accent-picker default.
The real SDL registration method verifies the app-local default. No persistent
macOS preference is changed. Actual AppKit KVO exercises recursive policy calls.

The shown-window image case runs serially and deliberately replaces SDL's
invisible image with an arrow. Refresh must restore it in raw and nonraw
modes without changing SDL visibility, mode or pointer position. Negative
cases cover menus, another key window, pointer outside content, a covering
nonactivating panel, stale hidden intent with visible absolute SDL state,
and focus changing synchronously during presentation-option notification.
The ordinary already-correct image path skips the WindowServer hit test.

Run these focus-dependent tests sequentially across Release/Debug build
folders. This qualifies the application's cursor image repair; it does not
identify the cursor rendered by a remote screenshot tool or prove physical
cursor visibility under every macOS event sequence.
