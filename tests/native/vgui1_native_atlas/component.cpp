#include "VGui1NativeAtlas.h"
#include "VGUI_Font.h"
#include <cstdio>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <vector>

extern "C" bool FixtureProviderPlatformFacts(std::uint64_t*, std::uint32_t);
extern "C" bool FixtureConsumerPlatformFacts(std::uint64_t*, std::uint32_t);
extern "C" void FixtureFailFontRelease(int);
extern "C" void FixtureSetRasterCallback(void (*)());
extern "C" std::uint64_t FixtureRasterCalls();
extern "C" std::size_t FixtureAllocationCalls();
namespace {
namespace native = cse_vgui1_native;
native::Runtime runtime;
std::unique_ptr<cse_vgui1_selection::Selection> heldSelection;
bool SelfHostedDefer(void*, void*) noexcept { return false; }
native::Atlas* duringAtlas = nullptr;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED native atlas: %s\n", message);
        std::abort();
    }
}
struct Renderer {
    std::uint64_t epoch = 1;
    bool live = true;
    int next = 100;
    int creates = 0;
    int uploads = 0;
    int failure = 0;
    bool retireFailure = false;
    int failedRetireId = 0;
    bool loseOnUpload = false;
    int liveRetires = 0;
    int lostRetires = 0;
    int x = 13;
    int y = 3;
    std::map<int, std::vector<unsigned char>> images;
    std::vector<native::Quad> draws;
};
std::unique_ptr<Renderer> heldAtlasRenderer;
std::unique_ptr<native::Atlas> heldAtlas;
std::uint64_t Generation(void* value) noexcept {
    return static_cast<Renderer*>(value)->epoch;
}
bool Live(void* value, std::uint64_t epoch) noexcept {
    const auto& renderer = *static_cast<Renderer*>(value);
    return renderer.live && renderer.epoch == epoch;
}
int Create(void* value) noexcept {
    auto& renderer = *static_cast<Renderer*>(value);
    ++renderer.creates;
    const int id = ++renderer.next;
    renderer.images.emplace(id, std::vector<unsigned char>{});
    return id;
}
bool Upload(void* value, int id, const unsigned char* data, std::size_t bytes) noexcept {
    auto& renderer = *static_cast<Renderer*>(value);
    Check(bytes == native::AtlasPageBytes && renderer.images.count(id) == 1, "owned full upload bounds");
    ++renderer.uploads;
    if (renderer.failure && renderer.uploads == renderer.failure) {
        return false;
    }
    renderer.images.at(id).assign(data, data + bytes);
    if (renderer.loseOnUpload) {
        renderer.live = false;
    }
    return true;
}
bool Retire(void* value, int id, std::uint64_t epoch, bool live) noexcept {
    auto& renderer = *static_cast<Renderer*>(value);
    Check(live == Live(value, epoch), "live/lost epoch classified exactly");
    if (renderer.retireFailure || renderer.failedRetireId == id) {
        return false;
    }
    Check(renderer.images.erase(id) == 1, "retire only owned texture once");
    live ? ++renderer.liveRetires : ++renderer.lostRetires;
    return true;
}
void GetPen(void* value, int& x, int& y) noexcept {
    const auto& renderer = *static_cast<Renderer*>(value);
    x = renderer.x;
    y = renderer.y;
}
void SetPen(void* value, int x, int y) noexcept {
    auto& renderer = *static_cast<Renderer*>(value);
    renderer.x = x;
    renderer.y = y;
}
void Draw(void* value, const native::Quad& quad) noexcept {
    auto& renderer = *static_cast<Renderer*>(value);
    Check(quad.texture > 0 && renderer.images.count(quad.texture) == 1, "valid admitted texture before draw");
    for (const float coordinate : quad.uv) {
        Check(coordinate >= 0.0f && coordinate <= 1.0f, "checked UV range");
    }
    renderer.draws.push_back(quad);
}
native::RendererV1 Ops(Renderer& renderer) {
    return {&renderer, Generation, Live, Create, Upload, Retire, GetPen, SetPen, Draw};
}
vgui::Font* Font(const char* name, int height = 20, int width = 0) {
    return new vgui::Font(name, height, width, 0.0f, 400, false, false, false, false);
}
void Delete(vgui::Font* font) {
    font->vgui::Font::~Font();
    ::operator delete(font);
}
void Bind() {
    const char* files[]{"/authored/native-atlas/font"};
    Check(runtime.BindFiles(files, 1, nullptr, 0) == native::Result::Ready, "real root routes actual owner bind");
}
void DuringRaster() {
    Check(runtime.Uses() != 0, "whole-use guard spans actual raster dispatch");
    Check(duringAtlas && duringAtlas->Draw("A", 1) == native::Result::Busy, "same-atlas reentry is explicit Busy");
    Check(runtime.ReleaseFiles(nullptr, 0) == native::Result::Busy, "root rejects mutation during raster callback");
}
}
extern "C" int FixtureOpen(std::uint64_t epoch, const char* root, const char* provider,
    const native::HostReleasesV1* host) {
    Check(host != nullptr, "actual main-image host release queue");
    native::HostReleasesV1 missing{};
    Check(runtime.Open({epoch, root, provider, missing}) == native::Result::InvalidArgument,
        "missing deferred ownership route rejected before admission");
    native::HostReleasesV1 self{sizeof(native::HostReleasesV1), native::HostReleasesVersion,
        &runtime, SelfHostedDefer};
    Check(runtime.Open({epoch, root, provider, self}) == native::Result::InvalidArgument,
        "self-hosted last-reference release route rejected without invoking it");
    const auto unavailable = runtime.Open({epoch, "/owned/no-such-image.dylib", provider, *host});
    Check(unavailable == native::Result::Unavailable && !runtime.HasReferences(), "wrong image cannot pin");
    Check(runtime.Open({0, root, provider, *host}) == native::Result::InvalidArgument, "no fabricated epoch fallback");
    Check(runtime.Open({epoch, root, provider, *host}) == native::Result::Ready,
        "actual root/provider images plus host deferred release ownership");
    Bind();
    return 0;
}

extern "C" int FixtureOpenV2(std::uint64_t epoch, const char* root, const char* provider,
    const cse_vgui1_host::HostReleasesV2* host) {
    Check(host != nullptr, "actual main-image V2 host release owner");
    const cse_vgui1_host::HostReleasesV2 missing{};
    Check(runtime.Open(native::BindingV2{epoch, root, provider, missing}) == native::Result::InvalidArgument,
        "missing V2 ownership route rejected before admission");
    const cse_vgui1_host::HostReleasesV2 self{sizeof(self), cse_vgui1_host::HostReleasesVersion,
        &runtime, SelfHostedDefer};
    Check(runtime.Open(native::BindingV2{epoch, root, provider, self}) == native::Result::InvalidArgument,
        "foreign dylib V2 callback rejected without invoking it");
    Check(runtime.Open(native::BindingV2{epoch, "/owned/no-such-image.dylib", provider, *host})
        == native::Result::Unavailable && !runtime.HasReferences(), "V2 wrong image cannot pin");
    Check(runtime.Open(native::BindingV2{0, root, provider, *host}) == native::Result::InvalidArgument,
        "V2 no fabricated epoch fallback");
    Check(runtime.Open(native::BindingV2{epoch, root, provider, *host}) == native::Result::Ready,
        "actual root/provider images plus main-image V2 release ownership");
    Bind();
    return 0;
}

// Losing platform entries on font switches must fail this real Runtime/Atlas
// workload: three scoreboard fonts, then repeated interleaved rows and frames.
extern "C" int FixtureScoreboardSuite() {
    Renderer renderer;
    native::Atlas atlas(runtime, Ops(renderer));
    auto* scoreboard = Font("Scoreboard", 20);
    auto* header = Font("ScoreboardHeader", 24);
    auto* small = Font("ScoreboardSmall", 16);
    const std::array<vgui::Font*, 3> fonts{{scoreboard, header, small}};
    const std::array<int, 3> heights{{20, 24, 16}};
    const auto rasters = FixtureRasterCalls();
    for (auto* font : fonts) {
        Check(atlas.Select(font) == native::Result::Ready, "scoreboard warms each actual platform");
    }
    Check(FixtureRasterCalls() - rasters == 768 && renderer.creates == 3 && renderer.uploads == 3,
        "three fonts each raster256 bytes and upload one complete page during warmup");
    Check(renderer.liveRetires == 0 && renderer.images.size() == 3 && runtime.Pins() == 3,
        "font switches retain all three leased platform atlases without retiring pages");
    // Keep the authored renderer's recorded quads outside the measured allocation path.
    constexpr int Frames = 3;
    constexpr int Rows = 8;
    constexpr int GlyphsPerCell = 3;
    renderer.draws.reserve(Frames * Rows * fonts.size() * GlyphsPerCell);
    const auto allocations = FixtureAllocationCalls();
    const auto warmRasters = FixtureRasterCalls();
    const auto warmCreates = renderer.creates;
    const auto warmUploads = renderer.uploads;
    for (int frame = 0; frame < Frames; ++frame) {
        for (int row = 0; row < Rows; ++row) {
            for (std::size_t index = 0; index < fonts.size(); ++index) {
                renderer.x = 13;
                Check(atlas.Select(fonts[index]) == native::Result::Ready
                    && atlas.Draw("WiA", 3) == native::Result::Ready && renderer.x == 31,
                    "warm scoreboard selection and draw preserve ABC endpen");
                const auto first = renderer.draws.size() - 3;
                Check(renderer.draws[first].x == 14 && renderer.draws[first].wide == 9
                    && renderer.draws[first].tall == heights[index]
                    && renderer.draws[first + 1].x == 24 && renderer.draws[first + 1].wide == 2
                    && renderer.draws[first + 2].x == 27 && renderer.draws[first + 2].wide == 4,
                    "interleaved scoreboard fonts preserve literal glyph rectangles");
            }
        }
    }
    Check(FixtureAllocationCalls() == allocations,
        "warm scoreboard selections and draws allocate no candidates, leases or atlas pixels");
    Check(FixtureRasterCalls() - rasters == 768 && renderer.creates == 3 && renderer.uploads == 3
        && renderer.liveRetires == 0 && renderer.lostRetires == 0 && renderer.images.size() == 3,
        "steady scoreboard frames perform zero raster, create, upload or retirement work");
    std::printf("PASS scoreboard work: warmup rasters=%llu creates=%d uploads=%d; "
        "steady cells=%zu glyphs=%zu rasters=%llu creates=%d uploads=%d retires=%d allocations=%zu\n",
        static_cast<unsigned long long>(warmRasters - rasters), warmCreates, warmUploads,
        Frames * Rows * fonts.size(), renderer.draws.size(),
        static_cast<unsigned long long>(FixtureRasterCalls() - warmRasters), renderer.creates - warmCreates,
        renderer.uploads - warmUploads, renderer.liveRetires + renderer.lostRetires,
        FixtureAllocationCalls() - allocations);
    const auto oldCatalog = atlas.Inspect().key.catalog;
    const char* additiveFiles[]{"/authored/native-atlas/scoreboard-additive"};
    Check(runtime.BindFiles(additiveFiles, 1, nullptr, 0) == native::Result::Ready,
        "actual catalog advances while all three platform leases remain cached");
    for (auto* font : fonts) {
        Check(atlas.Select(font) == native::Result::Ready && atlas.Inspect().key.catalog != oldCatalog,
            "each cached platform prepares and rebuilds after catalog advancement");
    }
    Check(FixtureRasterCalls() - rasters == 1536 && renderer.uploads == 6 && renderer.liveRetires == 3,
        "catalog change rebuilds exactly one page per cached platform");
    renderer.retireFailure = true;
    Check(atlas.Invalidate() == native::Result::ReleaseFailed && renderer.images.size() == 3,
        "failed invalidation retains all three cached texture owners");
    renderer.retireFailure = false;
    renderer.live = false;
    Check(atlas.Invalidate() == native::Result::Ready && renderer.images.empty() && renderer.lostRetires == 3,
        "invalidation retry retires every cached page without touching a lost device");
    ++renderer.epoch;
    renderer.live = true;
    for (auto* font : fonts) {
        Check(atlas.Select(font) == native::Result::Ready && atlas.Inspect().key.renderer == renderer.epoch,
            "each invalidated platform rebuilds in the next live renderer epoch");
        Delete(font);
    }
    Check(FixtureRasterCalls() - rasters == 2304 && renderer.uploads == 9 && renderer.images.size() == 3,
        "renderer change rebuilds every cached platform exactly once");
    renderer.retireFailure = true;
    Check(atlas.Retire() == native::Result::ReleaseFailed && runtime.Pins() == 3
        && renderer.images.size() == 3 && atlas.Inspect().retirementPending
        && atlas.Draw("A", 1) == native::Result::ReleaseFailed,
        "failed full retirement retains all three leases and blocks further drawing");
    renderer.retireFailure = false;
    renderer.failedRetireId = renderer.images.begin()->first;
    Check(atlas.Retire() == native::Result::ReleaseFailed && renderer.images.size() == 1
        && runtime.Pins() == 1 && atlas.Inspect().retirementPending && !atlas.Inspect().valid,
        "partial retirement releases successful cached entries and retains only the failed owner");
    renderer.failedRetireId = 0;
    Check(atlas.Retire() == native::Result::Ready && renderer.images.empty()
        && renderer.liveRetires == 6 && runtime.Pins() == 0 && runtime.Uses() == 0,
        "all three cached platforms explicitly retire after borrowed facades are deleted");
    std::puts("PASS scoreboard three-font warm768 rasters/3uploads; 72 warm selections/draws no allocation or rebuild");
    return 0;
}

extern "C" int FixtureSuite() {
    std::uint64_t providerFacts[10]{};
    std::uint64_t consumerFacts[10]{};
    Check(FixtureProviderPlatformFacts(providerFacts, 10) && FixtureConsumerPlatformFacts(consumerFacts, 10)
        && std::memcmp(providerFacts, consumerFacts, sizeof(providerFacts)) == 0,
        "actual typed private provider/consumer record facts agree");
    {
        native::Scope outer(runtime);
        native::Scope inner(runtime);
        Check(outer.Status() == native::Result::Ready && inner.Status() == native::Result::Ready
            && runtime.Uses() == 2 && runtime.Close() == native::Result::Busy
            && runtime.ReleaseFiles(nullptr, 0) == native::Result::Busy,
            "genuine baseline image ownership spans nested layout/paint scopes");
    }
    Check(runtime.Uses() == 0, "nested layout/paint scopes drain");
    Renderer renderer;
    native::Atlas atlas(runtime, Ops(renderer));
    Check(atlas.Draw(nullptr, 0) == native::Result::Ready && atlas.Draw("A", 1) == native::Result::Ready
        && renderer.draws.empty() && renderer.x == 13, "inherited null/no-selected-font no-op");
    auto* first = Font("First");
    auto* cached = Font("First");
    auto* second = Font("Second");
    auto* secondCached = Font("Second");
    Check(cached->getId() == -1 && secondCached->getId() == -1
        && cached->getPlat() != secondCached->getPlat(), "actual owner distinct cache-hit ID minus-one platforms");
    duringAtlas = &atlas;
    FixtureSetRasterCallback(DuringRaster);
    Check(atlas.Select(cached) == native::Result::Ready, "actual owner first atlas");
    FixtureSetRasterCallback(nullptr);
    duringAtlas = nullptr;
    Check(atlas.Inspect().admitted == native::AtlasGlyphCount && atlas.Inspect().pages == 1,
        "all256 admitted and all slots initialized");
    int measured = 0;
    int measuredTall = 0;
    {
        native::Scope layout(runtime);
        Check(layout.Status() == native::Result::Ready, "actual facade measurement has whole-use layout scope");
        cached->getTextSize("WiA", measured, measuredTall);
    }
    Check(measured == 18 && measuredTall == 20, "actual common Font measurement bytes");
    const auto firstKey = atlas.Inspect().key;
    Check(firstKey.moduleEpoch == runtime.ModuleEpoch(), "key retains process-owner module epoch");
    Check(atlas.Draw(nullptr, 9) == native::Result::Ready && atlas.Draw("", -1) == native::Result::Ready
        && renderer.draws.empty() && renderer.x == 13, "inherited null and nonpositive-length behavior");
    const auto uploads = renderer.uploads;
    std::thread other([&] {
        Check(atlas.Retire() == native::Result::Unavailable, "wrong-thread retirement cannot touch renderer");
    });
    other.join();
    cse_vgui1_selection::Selection nestedOne;
    cse_vgui1_selection::Selection nestedTwo;
    Check(nestedOne.Capture(first, runtime.Owner()) == cse_vgui1_selection::Result::Captured
        && nestedTwo.Capture(first, runtime.Owner()) == cse_vgui1_selection::Result::Captured, "real nested pins");
    cse_vgui1_selection::PreparedFont one{};
    cse_vgui1_selection::PreparedFont two{};
    Check(nestedOne.BeginUse(renderer.epoch, one) == cse_vgui1_selection::Result::Ready
        && nestedTwo.BeginUse(renderer.epoch, two) == cse_vgui1_selection::Result::Ready
        && runtime.Uses() == 2 && runtime.ReleaseFiles(nullptr, 0) == native::Result::Busy,
        "real nested whole-use guards exclude root mutation");
    Check(nestedTwo.EndUse(two.useSerial) == cse_vgui1_selection::Result::Captured
        && nestedOne.EndUse(one.useSerial) == cse_vgui1_selection::Result::Captured
        && nestedTwo.Retire() == cse_vgui1_selection::Result::Retired
        && nestedOne.Retire() == cse_vgui1_selection::Result::Retired, "nested reverse release");
    Check(atlas.Select(first) == native::Result::Ready && renderer.uploads == uploads,
        "same retained platform shares atlas regardless of numeric ID");
    Delete(cached);
    Delete(first);
    Check(atlas.Draw("WiA", 3) == native::Result::Ready && renderer.x == 13 + measured,
        "deleted facade never reread; actual ABC widths preserve endpen");
    Check(renderer.draws.size() == 3 && renderer.draws[0].wide == 9 && renderer.draws[1].wide == 2,
        "per-glyph ABC B geometry");
    const auto draws = renderer.draws.size();
    renderer.x = (std::numeric_limits<int>::max)();
    Check(atlas.Draw("W", 1) == native::Result::Capacity && renderer.draws.size() == draws
        && renderer.x == (std::numeric_limits<int>::max)(), "complete pen overflow check before output writes");
    renderer.x = 13;
    Check(atlas.Select(secondCached) == native::Result::Ready && atlas.Inspect().key.platform != firstKey.platform,
        "ID minus-one collision cannot share atlas");
    Delete(secondCached);
    Delete(second);
    const auto catalog = atlas.Inspect().key.catalog;
    FixtureFailFontRelease(1);
    Check(runtime.ReleaseFiles(nullptr, 0) == native::Result::ReleaseFailed, "actual owner release failure injection");
    Check(atlas.Draw("A", 1) == native::Result::ReleaseFailed && !atlas.Inspect().valid,
        "release failure authoritative with no stale cached draw");
    Check(atlas.Inspect().key.catalog == catalog, "release failure does not fabricate catalog advancement");
    Check(runtime.Pins() == 2 && runtime.Close() == native::Result::ReleaseFailed,
        "retain both cached platform image leases/context");
    Check(atlas.Retire() == native::Result::ReleaseFailed && runtime.Pins() == 2,
        "failed unpin retains both complete cached platform leases");
    Check(runtime.ReleaseFiles(nullptr, 0) == native::Result::Ready, "explicit successful file release retry");
    Check(atlas.Retire() == native::Result::Ready && runtime.Pins() == 0, "explicit failed unpin retry");
    Bind();
    auto* rebound = Font("Second");
    Check(atlas.Select(rebound) == native::Result::Ready, "retained old record prepared after rebind");
    Delete(rebound);
    Check(atlas.Inspect().key.catalog != catalog, "additive bind generation observed");
    renderer.live = false;
    Check(atlas.Invalidate() == native::Result::Ready && renderer.lostRetires > 0,
        "lost epoch retirement removes metadata without live-device action");
    ++renderer.epoch;
    renderer.live = true;
    Check(atlas.Draw("A", 1) == native::Result::Ready && atlas.Inspect().key.renderer == renderer.epoch,
        "real renderer epoch triggers rebuild");
    Check(atlas.Retire() == native::Result::Ready, "retire after repeated use");
    auto* eight = Font("Eight", 511, 15);
    Check(atlas.Select(eight) == native::Result::Ready && atlas.Inspect().pages == 8,
        "all8 initialized pages accepted");
    Check(renderer.images.size() == 8, "exactly8 owned texture pages");
    std::vector<int> pageIds;
    std::size_t pageIndex = 0;
    for (const auto& image : renderer.images) {
        std::vector<unsigned char> expected(native::AtlasPageBytes, 0);
        constexpr std::size_t GlyphsPerPage = 32;
        constexpr std::size_t CellStride = 16;
        constexpr std::size_t Channels = 4;
        for (std::size_t cell = 0; cell < GlyphsPerPage; ++cell) {
            const auto byte = pageIndex * GlyphsPerPage + cell;
            const auto offset = cell * CellStride * Channels;
            expected[offset] = static_cast<unsigned char>(byte);
            expected[offset + 3] = 255;
        }
        Check(image.second == expected, "complete known-byte upload content and zero padding for all8 pages");
        pageIds.push_back(image.first);
        ++pageIndex;
    }
    renderer.draws.clear();
    renderer.x = 0;
    renderer.y = 0;
    char allBytes[native::AtlasGlyphCount];
    for (std::size_t byte = 0; byte < native::AtlasGlyphCount; ++byte) {
        allBytes[byte] = static_cast<char>(static_cast<unsigned char>(byte));
    }
    Check(atlas.Draw(allBytes, static_cast<int>(native::AtlasGlyphCount)) == native::Result::Ready
        && renderer.draws.size() == native::AtlasGlyphCount && renderer.x == 3840,
        "all256 explicit byte values draw with exact original ABC pen arithmetic");
    for (std::size_t byte = 0; byte < native::AtlasGlyphCount; ++byte) {
        const auto& quad = renderer.draws[byte];
        const auto column = byte % 32;
        Check(quad.texture == pageIds[byte / 32] && quad.x == static_cast<int>(byte * 15)
            && quad.y == 0 && quad.wide == 15 && quad.tall == 511
            && quad.uv[0] == static_cast<float>(column * 16) / 512.0f && quad.uv[1] == 0.0f
            && quad.uv[2] == static_cast<float>(column * 16 + 15) / 512.0f && quad.uv[3] == 511.0f / 512.0f,
            "exact glyph page/UV/winding-independent rectangle bounds across all8 pages");
    }
    Delete(eight);
    Check(atlas.Retire() == native::Result::Ready, "retire all8 pages");
    auto* ninth = Font("Nine", 511, 16);
    const int creates = renderer.creates;
    Check(atlas.Select(ninth) == native::Result::Capacity && renderer.creates == creates
        && !atlas.Inspect().valid, "reject ninth page before create/upload/slot read/publication");
    Delete(ninth);
    Check(atlas.Retire() == native::Result::Ready, "retire rejected selection");
    auto* lost = Font("LostDuringUpload");
    renderer.loseOnUpload = true;
    Check(atlas.Select(lost) == native::Result::Unavailable && renderer.images.empty() && !atlas.Inspect().valid,
        "known loss during upload cannot publish or issue further device work");
    renderer.loseOnUpload = false;
    renderer.live = true;
    ++renderer.epoch;
    Check(atlas.Draw("A", 1) == native::Result::Ready, "next live epoch rebuilds after loss");
    Check(atlas.Retire() == native::Result::Ready, "lost-upload recovery retires");
    Delete(lost);
    auto* upload = Font("Upload", 511, 15);
    renderer.failure = renderer.uploads + 3;
    Check(atlas.Select(upload) == native::Result::UploadFailed && renderer.images.empty(),
        "partial upload rollback owns and retires every created image");
    renderer.failure = 0;
    Check(atlas.Draw("A", 1) == native::Result::Ready, "defined upload retry rebuilds whole atlas");
    renderer.retireFailure = true;
    Check(atlas.Retire() == native::Result::ReleaseFailed && runtime.Pins() == 1 && !renderer.images.empty(),
        "failed resource retirement retains metadata and module identity");
    renderer.retireFailure = false;
    Check(atlas.Retire() == native::Result::Ready && renderer.images.empty(), "resource retirement retry");
    Delete(upload);
    Check(runtime.Pins() == 0 && runtime.Uses() == 0, "every real owned selection lease retired");
    Check(runtime.ReleaseFiles(nullptr, 0) == native::Result::Ready, "normal final file release");
    std::puts("PASS actual owner/Selection/Root + controlled renderer all256, 8/9pages, rollback/lifetime");
    return 0;
}
extern "C" int FixtureHoldAtlas() {
    Check(!heldAtlas && !heldAtlasRenderer, "one held atlas control");
    Bind();
    heldAtlasRenderer.reset(new Renderer);
    heldAtlas.reset(new native::Atlas(runtime, Ops(*heldAtlasRenderer)));
    auto* first = Font("PendingCaptureFirst");
    auto* second = Font("PendingCaptureSecond");
    Check(heldAtlas->Select(first) == native::Result::Ready
        && heldAtlas->Select(second) == native::Result::Ready && runtime.Pins() == 2,
        "two actual platform leases precede capture rollback failure");
    Delete(first);
    Delete(second);
    return 0;
}
extern "C" int FixtureRejectAtlasCapture() {
    unsigned char invalidBitmap = 0;
    auto* invalid = new vgui::Font("InvalidBitmap", &invalidBitmap, 1, 20, 0, 0.0f, 400,
        false, false, false, false);
    Check(invalid->getPlat() == nullptr, "actual malformed bitmap owner yields no platform");
    Check(heldAtlas->Select(invalid) == native::Result::ReleaseFailed && runtime.Pins() == 3
        && heldAtlas->Inspect().retirementPending && heldAtlas->Draw("A", 1) == native::Result::ReleaseFailed,
        "failed capture rollback retains pending candidate plus both cached platform leases");
    Delete(invalid);
    Check(heldAtlas->Retire() == native::Result::ReleaseFailed && runtime.Pins() == 3
        && heldAtlasRenderer->images.empty(),
        "failed host queue releases all cached pixels but retains every pending code lease");
    return 0;
}
extern "C" int FixtureRetryAtlasRetire() {
    Check(heldAtlas->Retire() == native::Result::Ready && runtime.Pins() == 0 && runtime.Uses() == 0,
        "accepted host queue retires pending candidate and all cached entries exactly once");
    heldAtlas.reset();
    heldAtlasRenderer.reset();
    Check(runtime.ReleaseFiles(nullptr, 0) == native::Result::Ready, "atlas rollback control normal file release");
    return 0;
}
extern "C" int FixtureHoldSelection() {
    Check(!heldSelection, "one held selection control");
    Bind();
    auto* font = Font("HostQueueFailure");
    heldSelection.reset(new cse_vgui1_selection::Selection);
    Check(heldSelection->Capture(font, runtime.Owner()) == cse_vgui1_selection::Result::Captured,
        "genuine additional root/provider ref for deferred unpin control");
    Delete(font);
    return 0;
}
extern "C" int FixtureRejectUnpin() {
    Check(heldSelection->Retire() == cse_vgui1_selection::Result::ReleaseFailed && runtime.Pins() == 1
        && runtime.ReleasePending(), "host queue rejection retains whole selection/code references");
    native::Scope scope(runtime);
    Check(scope.Status() == native::Result::ReleaseFailed, "no new UI use after failed code retirement");
    return 0;
}
extern "C" int FixtureRetryUnpin() {
    Check(heldSelection->Retire() == cse_vgui1_selection::Result::Retired && runtime.Pins() == 0,
        "accepted deferred unpin transfers genuine root ref only once");
    heldSelection.reset();
    Check(runtime.ReleaseFiles(nullptr, 0) == native::Result::Ready, "retry control normal file release");
    return 0;
}
extern "C" int FixtureRejectClose() {
    Check(runtime.Close() == native::Result::ReleaseFailed && runtime.HasReferences()
        && runtime.ReleasePending(), "last root ref cannot be released when host queue refuses");
    return 0;
}
extern "C" int FixtureClose() {
    Check(runtime.Close() == native::Result::Closed && !runtime.HasReferences(),
        "root close transfers last code reference to host until after this return");
    return 0;
}
extern "C" void FixtureFailNextAllocation();
extern "C" void FixtureFaultRuntime(const char* root, const char* provider, const native::HostReleasesV1* host) {
    FixtureFailNextAllocation();
    (void)runtime.Open({99, root, provider, *host});
    std::abort();
}
extern "C" void FixtureFaultAtlas() {
    Renderer renderer;
    native::Atlas atlas(runtime, Ops(renderer));
    auto* font = Font("AllocationFaultAtlas");
    FixtureFailNextAllocation();
    (void)atlas.Select(font);
    std::abort();
}
extern "C" void FixtureFaultPin() {
    auto* font = Font("AllocationFaultPin");
    cse_vgui1_selection::Selection selected;
    FixtureFailNextAllocation();
    (void)selected.Capture(font, runtime.Owner());
    std::abort();
}
extern "C" int FixtureConcurrentGuards() {
    const auto owner = runtime.Owner();
    std::atomic<bool> stop{false};
    std::atomic<bool> bad{false};
    std::atomic<std::uint64_t> rejected{0};
    std::thread other([&] {
        while (!stop.load(std::memory_order_acquire)) {
            constexpr std::uint64_t Unchanged = 0x123456789abcdef0ULL;
            std::uint64_t token = Unchanged;
            if (owner.pinModule(owner.context, &token) || token != Unchanged
                || owner.unpinModule(owner.context, 0) || owner.enterUse(owner.context, 0)) {
                bad.store(true, std::memory_order_release);
            }
            rejected.fetch_add(1, std::memory_order_relaxed);
        }
    });
    while (rejected.load(std::memory_order_acquire) == 0) {
        std::this_thread::yield();
    }
    constexpr int OwnerMutations = 256;
    for (int index = 0; index < OwnerMutations; ++index) {
        std::uint64_t token = 0;
        Check(owner.pinModule(owner.context, &token), "owner real pin during concurrent invalid callbacks");
        Check(owner.unpinModule(owner.context, token), "owner deferred unpin during concurrent invalid callbacks");
    }
    stop.store(true, std::memory_order_release);
    other.join();
    Check(!bad.load(std::memory_order_acquire) && rejected.load(std::memory_order_relaxed) != 0
        && runtime.Pins() == 0 && runtime.Uses() == 0, "wrong-thread callbacks reject before mutable owner state");
    std::puts("PASS concurrent invalid-thread rejection against256 real owner pin/unpin mutations");
    return 0;
}
