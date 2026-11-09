#include "VGUI_App.h"
#include "VGUI_Bitmap.h"
#include "VGUI_Scheme.h"
#include "VGUI_Font.h"
#include "VGui1FontServices.h"
#include "fixture.h"
#include <cstdlib>
#include <cstring>
#include <type_traits>

// Exact retained anonymous CursorBitmap definition/constructor, included ONCE.
// No real Surface/drawing service is instantiated by the cold App constructor.
#include CSE_APP_COLD_CURSOR_SOURCE

namespace {
template<class Tag, typename Tag::Type Member> struct Access {
    friend typename Tag::Type Field(Tag) {
        return Member;
    }
};
#define FIELD(tag, owner, type, member) \
    struct tag { using Type = type owner::*; friend Type Field(tag); }; \
    template struct Access<tag, &owner::member>
FIELD(SurfaceDar, vgui::App, vgui::Dar<vgui::SurfaceBase*>, _surfaceBaseDar);
FIELD(TickDar, vgui::App, vgui::Dar<vgui::TickSignal*>, _tickSignalDar);
FIELD(NextTick, vgui::App, int, _nextTickMillis);
FIELD(MinimumTick, vgui::App, int, _minimumTickMillisInterval);
FIELD(FontName, vgui::Font, char*, _name);
FIELD(BitmapRgba, vgui::Bitmap, uchar*, _rgba);
using Surfaces = vgui::Dar<vgui::SurfaceBase*>;
using Ticks = vgui::Dar<vgui::TickSignal*>;
FIELD(SurfaceData, Surfaces, vgui::SurfaceBase**, _data);
FIELD(TickData, Ticks, vgui::TickSignal**, _data);
#undef FIELD

class BorrowedInstanceAccess : public vgui::App {
public:
    static bool Clear(vgui::App* expected) {
        if (_instance != expected) {
            return false;
        }
        _instance = nullptr;
        return true;
    }
};
}

extern "C" bool FixtureObserveProvider(vgui::App* app, ColdObservation* output) {
    if (!app || !output || app != vgui::App::getInstance() || !app->getScheme()) {
        return false;
    }
    auto* scheme = app->getScheme();
    const char* const names[]{"Arial", "FixedSys", "Arial", "Marlett"};
    constexpr int FontCount = sizeof(names) / sizeof(names[0]);
    for (int index = 0; index < FontCount; ++index) {
        auto* font = scheme->getFont(static_cast<vgui::Scheme::SchemeFont>(vgui::Scheme::sf_primary1 + index));
        CseVgui1FontStatusV1 status{};
        status.structSize = sizeof(status);
        if (!font || std::strcmp(font->*Field(FontName{}), names[index]) != 0
            || font->getId() != VGUI_FIRST_FONT_ID + index
            || CseVgui1FontQueryV1(font->getPlat(), &status, nullptr, 0)
                != CseVgui1FontResultV1::PendingResources
            || status.state != CseVgui1FontStateV1::PendingResources || status.capabilities != 0) {
            return false;
        }
    }
    constexpr int CursorCount = vgui::Scheme::scu_hand - vgui::Scheme::scu_none + 1;
    for (int index = 0; index < CursorCount; ++index) {
        auto* cursor = scheme->getCursor(static_cast<vgui::Scheme::SchemeCursor>(vgui::Scheme::scu_none + index));
        int x = -1;
        int y = -1;
        int wide = -1;
        int tall = -1;
        cursor->getHotspot(x, y);
        cursor->getBitmap()->getSize(wide, tall);
        const bool none = index == 0;
        if (x != (none ? 0 : 8) || y != (none ? 0 : 8)
            || wide != (none ? 0 : 16) || tall != (none ? 0 : 16)) {
            return false;
        }
        const uchar* rgba = cursor->getBitmap()->*Field(BitmapRgba{});
        if ((rgba == nullptr) != none) {
            return false;
        }
        for (int row = 0; row < tall; ++row) {
            for (int column = 0; column < wide; ++column) {
                const std::size_t offset = static_cast<std::size_t>(row * wide + column) * VGUI_RGBA_BYTES;
                const bool horizontal = row == 8;
                const bool vertical = column == 8;
                if (rgba[offset] != 0 || rgba[offset + 1] != (horizontal && !vertical ? 255 : 0)
                    || rgba[offset + 2] != (vertical ? 255 : 0)
                    || rgba[offset + 3] != (horizontal || vertical ? 255 : 0)) {
                    return false;
                }
            }
        }
    }
    if ((app->*Field(SurfaceDar{})).getCount() != 0 || (app->*Field(TickDar{})).getCount() != 0) {
        return false;
    }
    *output = {sizeof(vgui::App), FontCount, CursorCount,
        static_cast<std::uint32_t>(app->*Field(NextTick{})),
        static_cast<std::uint32_t>(app->*Field(MinimumTick{})), FixtureColdClockCalls()};
    return FixtureProviderCalls() == 0;
}

extern "C" bool FixtureCleanupProvider(vgui::App* app) {
    ColdObservation observation{};
    if (!FixtureObserveProvider(app, &observation)) {
        return false;
    }
    // FIXTURE ONLY: known pristine Scheme owns these exact construction objects.
    // Real setScheme/setFont/setCursor can borrow external objects; production
    // must never apply this teardown to arbitrary App/Scheme state.
    auto* scheme = app->getScheme();
    for (std::uint32_t index = 0; index < observation.fonts; ++index) {
        auto* font = scheme->getFont(static_cast<vgui::Scheme::SchemeFont>(vgui::Scheme::sf_primary1 + index));
        font->vgui::Font::~Font();
        ::operator delete(font);
    }
    for (std::uint32_t index = 0; index < observation.cursors; ++index) {
        auto* cursor = scheme->getCursor(static_cast<vgui::Scheme::SchemeCursor>(vgui::Scheme::scu_none + index));
        auto* bitmap = static_cast<CursorBitmap*>(cursor->getBitmap());
        delete[] (bitmap->*Field(BitmapRgba{}));
        bitmap->CursorBitmap::~CursorBitmap();
        ::operator delete(bitmap);
        cursor->vgui::Cursor::~Cursor();
        ::operator delete(cursor);
    }
    scheme->vgui::Scheme::~Scheme();
    ::operator delete(scheme);
    delete[] ((app->*Field(SurfaceDar{})).*Field(SurfaceData{}));
    delete[] ((app->*Field(TickDar{})).*Field(TickData{}));
    return BorrowedInstanceAccess::Clear(app);
}
