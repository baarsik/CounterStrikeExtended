// Reuse the genuine owned root/provider fixture and its deferred release host
// contract. Only the exercised text cases are new; Atlas and Font are production.
#include "../vgui1_native_atlas/component.cpp"

extern "C" int FixtureUtf8Suite() {
    Renderer renderer;
    renderer.draws.reserve(100);
    native::Atlas atlas(runtime, Ops(renderer));
    auto* font = Font("UTF8");
    Check(atlas.Select(font) == native::Result::Ready, "select actual owned Unicode atlas");
    constexpr char Russian[] = u8"Привет";
    int width = 0;
    int height = 0;
    {
        native::Scope scope(runtime);
        Check(scope.Status() == native::Result::Ready, "actual Unicode measurement lease");
        font->getTextSize(Russian, width, height);
    }
    Check(width == 30 && height == 20, "actual Font measures six Cyrillic glyphs with provider ABC");
    Check(atlas.Draw(Russian, sizeof(Russian) - 1) == native::Result::Ready
        && renderer.draws.size() == 6 && renderer.x == 13 + width,
        "actual native atlas draws six Unicode glyphs instead of twelve UTF8 bytes");
    Check(atlas.Inspect().admitted == native::AtlasGlyphCount + 6,
        "extra Unicode cells share the actual bounded atlas ownership");
    const auto calls = FixtureAllocationCalls();
    const auto rasters = FixtureRasterCalls();
    const auto uploads = renderer.uploads;
    renderer.x = 13;
    Check(atlas.Draw(Russian, sizeof(Russian) - 1) == native::Result::Ready
        && FixtureAllocationCalls() == calls && FixtureRasterCalls() == rasters && renderer.uploads == uploads,
        "warm Unicode draws allocate and upload nothing");
    const auto count = renderer.draws.size();
    const char legacy[]{static_cast<char>(0xe9)};
    Check(atlas.Draw(legacy, sizeof(legacy)) == native::Result::Ready && renderer.draws.size() == count + 1,
        "malformed single UTF8 byte retains one CP1252 glyph");
    const char mixed[]{static_cast<char>(0xff), static_cast<char>(0xc5), static_cast<char>(0xbc)};
    Check(atlas.Draw(mixed, sizeof(mixed)) == native::Result::Ready && renderer.draws.size() == count + 4,
        "whole invalid run remains legacy bytes even when a suffix resembles UTF8");
    Delete(font);
    Check(atlas.Retire() == native::Result::Ready && renderer.images.empty(),
        "Unicode and byte textures retire through existing owner");
    Check(runtime.ReleaseFiles(nullptr, 0) == native::Result::Ready, "release fixture font registration");
    std::puts("PASS actual VGUI1 UTF8 atlas: glyph count, ABC pen, warm cache, legacy fallback, retirement");
    return 0;
}
