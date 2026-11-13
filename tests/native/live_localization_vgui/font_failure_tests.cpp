#include "FontManager.h"
#include "LinuxFont.h"
#include "LocalizationFontBackend.h"
#include "../../../src/public/cse_module_abi.h"
#include <cassert>
#include <cstdio>

void CSE_ABI_CALL ResetCallback(std::uint32_t font) {
    FontManager().ResetFont(font);
}

int main() {
    auto& manager = FontManager();
    const auto font = manager.CreateFont();
    assert(manager.AddGlyphSetToFont(font, "Helvetica", 20, 400, 0, 0, 0));
    CLinuxFont* oldFace = manager.GetFontForChar(font, L'A');
    const int oldHeight = manager.GetFontTall(font);
    assert(oldFace && oldHeight > 0);
    const auto reset = &ResetCallback;
    reset(font);
    assert(manager.GetFontForChar(font, L'A') == oldFace);
    assert(!manager.AddGlyphSetToFont(font, "CSEFontThatDoesNotExist_5927", 18, 400, 0, 0, 0));
    assert(manager.GetFontForChar(font, L'A') == oldFace && manager.GetFontTall(font) == oldHeight);
    const auto* backend = cse::localization::LocalFontBackend();
    assert(backend->Begin("russian"));
    assert(!backend->Stage(font, "CSEFontThatDoesNotExist_5927", 18, 400, 0, 0, 0));
    assert(manager.GetFontForChar(font, L'A') == oldFace && manager.GetFontTall(font) == oldHeight);
    backend->Finish();
    backend->Discard();
    assert(backend->Begin("russian"));
    assert(backend->Stage(font, "Helvetica", 18, 400, 0, 0, 0));
    backend->Finish();
    assert(manager.GetFontForChar(font, L'A') == oldFace && manager.GetFontTall(font) == oldHeight);
    backend->Discard();
    assert(manager.GetFontForChar(font, L'A') == oldFace);
    assert(backend->Begin("russian"));
    assert(backend->Stage(font, "Helvetica", 18, 400, 0, 0, 0));
    backend->Finish();
    manager.SetLanguage("russian");
    backend->Commit();
    assert(manager.GetFontTall(font) < oldHeight);
    const auto replacement = manager.GetFontForChar(font, L'A');
    manager.ResetFont(0);
    manager.ResetFont(0xffffffffu);
    assert(manager.GetFontForChar(font, L'A') == replacement);
    std::puts("PASS: actual font reset failure preserves ranges/metrics; successful replacement keeps HFont");
}
