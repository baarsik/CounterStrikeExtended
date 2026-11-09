#include "VGUI_App.h"
#include "VGUI_TextEntry.h"
#include "VGUI_EditPanel.h"
#include "VGUI_TickSignal.h"
#include "VGUI_Scheme.h"
#include "VGUI_FocusChangeSignal.h"
#include "VGUI_InputSignal.h"
#include "VGui1AppPlatform.h"
#include "common/VGui1LegacyTickMillis.h"
#include "tick_vectors.h"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <type_traits>

namespace {
std::uint32_t clockFirst = 100;
std::uint32_t clockSecond = 100;
unsigned int clockReads = 0;
std::uint32_t ReadEncodedClock() {
    return clockReads++ == 0 ? clockFirst : clockSecond;
}
void SetClock(std::uint32_t first, std::uint32_t second) {
    clockFirst = first;
    clockSecond = second;
    clockReads = 0;
}
long SignedInput(std::uint32_t encoded) {
    const long long value = encoded <= INT32_MAX ? static_cast<long long>(encoded)
        : static_cast<long long>(encoded) - 4294967296LL;
    return static_cast<long>(value); // Always representable even in real Windows long32.
}

template<class Tag, class Type, Type Member> struct Access {
    friend auto DataMember(Tag) {
        return Member;
    }
};
template<class Type> struct DarTag {
    friend auto DataMember(DarTag);
};
#define DECLARE_REAL_DAR_ACCESS(Type) \
    template struct Access<DarTag<Type>, decltype(&vgui::Dar<Type>::_data), &vgui::Dar<Type>::_data>;
DECLARE_REAL_DAR_ACCESS(vgui::SurfaceBase*)
DECLARE_REAL_DAR_ACCESS(vgui::TickSignal*)
DECLARE_REAL_DAR_ACCESS(char)
DECLARE_REAL_DAR_ACCESS(vgui::ActionSignal*)
DECLARE_REAL_DAR_ACCESS(vgui::Panel*)
DECLARE_REAL_DAR_ACCESS(vgui::InputSignal*)
DECLARE_REAL_DAR_ACCESS(vgui::RepaintSignal*)
DECLARE_REAL_DAR_ACCESS(vgui::FocusChangeSignal*)
DECLARE_REAL_DAR_ACCESS(vgui::Dar<char>*)
#undef DECLARE_REAL_DAR_ACCESS

template<class Type> void FreeDar(vgui::Dar<Type>& value) {
    delete[] (value.*DataMember(DarTag<Type>{}));
}
struct AppNext {
    friend auto DataMember(AppNext);
};
template struct Access<AppNext, decltype(&vgui::App::_nextTickMillis), &vgui::App::_nextTickMillis>;
using AppNextType = typename std::remove_reference<decltype(std::declval<vgui::App&>().*DataMember(AppNext{}))>::type;

class ReferenceApp : public vgui::App {
public:
    int pumps = 0;
    void main(int, char**) override {
    }
    void platTick() override {
        ++pumps;
    }
    void getCursorPos(int& x, int& y) override {
        x = 0;
        y = 0;
    }
    void Deadline(long value) {
        this->*DataMember(AppNext{}) = static_cast<AppNextType>(value);
    }
    long long Deadline() {
        return static_cast<long long>(this->*DataMember(AppNext{}));
    }
    ~ReferenceApp() {
        FreeDar(_surfaceBaseDar);
        FreeDar(_tickSignalDar);
        auto* scheme = getScheme();
        scheme->vgui::Scheme::~Scheme();
        ::operator delete(scheme);
    }
};

extern "C" void DeleteOwnedTextFocus(vgui::FocusChangeSignal*);
extern "C" void DeleteOwnedEditInput(vgui::InputSignal*);

class ReferenceText : public vgui::TextEntry {
public:
    using DeadlineType = decltype(_cursorNextBlinkTime);
    ReferenceText() : vgui::TextEntry("", 0, 0, 20, 20) {
    }
    int repaints = 0;
    void repaint() override {
        ++repaints;
    }
    void Set(long deadline, int rate) {
        _cursorNextBlinkTime = deadline;
        _cursorBlinkRate = rate;
        _cursorBlink = false;
        repaints = 0;
    }
    void Tick() {
        vgui::TextEntry::keyFocusTicked(nullptr);
    }
    long long Deadline() const {
        return static_cast<long long>(_cursorNextBlinkTime);
    }
    bool Blink() const {
        return _cursorBlink;
    }
    ~ReferenceText() {
        for (int index = 0; index < _focusChangeSignalDar.getCount(); ++index) {
            DeleteOwnedTextFocus(_focusChangeSignalDar[index]);
        }
        FreeDar(_lineDar);
        FreeDar(_actionSignalDar);
        FreeDar(_childDar);
        FreeDar(_inputSignalDar);
        FreeDar(_repaintSignalDar);
        FreeDar(_focusChangeSignalDar);
    }
};

class ReferenceEdit : public vgui::EditPanel {
public:
    using DeadlineType = decltype(_cursorNextBlinkTime);
    ReferenceEdit() : vgui::EditPanel(0, 0, 20, 20) {
    }
    int repaints = 0;
    void repaint() override {
        ++repaints;
    }
    void Set(int deadline) {
        _cursorNextBlinkTime = deadline;
        _cursorBlink = false;
        repaints = 0;
    }
    void Tick() {
        _inputSignalDar[0]->keyFocusTicked(this); // Actual registered anonymous listener, normal dispatch.
    }
    ~ReferenceEdit() {
        for (int index = 0; index < _inputSignalDar.getCount(); ++index) {
            DeleteOwnedEditInput(_inputSignalDar[index]);
        }
        for (int index = 0; index < _lineDarDar.getCount(); ++index) {
            FreeDar(*_lineDarDar[index]);
            delete _lineDarDar[index];
        }
        FreeDar(_lineDarDar);
        FreeDar(_childDar);
        FreeDar(_inputSignalDar);
        FreeDar(_repaintSignalDar);
        FreeDar(_focusChangeSignalDar);
    }
};
}

namespace cse_vgui1_app {
bool ReadColdClock(std::uint32_t& output) noexcept {
    output = ReadEncodedClock();
    return true;
}
}
#if defined(_WIN32)
unsigned long GetTickCount() {
    return ReadEncodedClock();
}
#endif

int main() {
    for (const auto& vector : TickVectors) {
        const long clock = SignedInput(vector.clock);
        if (vgui::detail::NormalizeTickMillis(clock) != clock
            || vgui::detail::AddTickMillis(clock, vector.interval) != vector.expected) {
            std::fputs("integer helper differs from independent signed32 oracle\n", stderr);
            return 1;
        }
    }
    constexpr std::int32_t MaximumLongExpected = sizeof(long) == sizeof(std::int32_t) ? INT32_MAX : -1;
    constexpr std::int32_t MinimumLongExpected = sizeof(long) == sizeof(std::int32_t) ? INT32_MIN : 0;
    if (vgui::detail::NormalizeTickMillis((std::numeric_limits<long>::max)()) != MaximumLongExpected
        || vgui::detail::NormalizeTickMillis((std::numeric_limits<long>::min)()) != MinimumLongExpected) {
        std::fputs("long input normalization boundary failure\n", stderr);
        return 1;
    }
    ReferenceApp app;
    ReferenceText text;
    ReferenceEdit edit;
    std::printf("{\"kind\":\"facts\",\"long_bytes\":%zu,\"app_deadline_bytes\":%zu,"
        "\"text_deadline_bytes\":%zu,\"edit_deadline_bytes\":%zu,\"text_signed\":%d}\n",
        sizeof(long), sizeof(AppNextType), sizeof(ReferenceText::DeadlineType), sizeof(ReferenceEdit::DeadlineType),
        std::numeric_limits<ReferenceText::DeadlineType>::is_signed ? 1 : 0);
    constexpr std::uint32_t Values[] = {0u, 1u, 0x7ffffe6fu, 0x7ffffffeu, 0x7fffffffu,
        0x80000000u, 0x80000001u, 0xfffffffeu, 0xffffffffu};
    constexpr int Intervals[] = {0, 50, 400, -1, INT32_MAX, INT32_MIN};
    for (auto now : Values) {
        for (auto deadline : Values) {
            for (auto interval : Intervals) {
                app.Deadline(SignedInput(deadline));
                app.setMinimumTickMillisInterval(interval);
                app.pumps = 0;
                SetClock(now, now + 1u);
                app.externalTick();
                std::printf("{\"kind\":\"app\",\"now\":%u,\"old\":%u,\"interval\":%d,"
                    "\"completion\":%u,\"deadline\":%lld,\"acted\":%d,\"reads\":%u}\n",
                    now, deadline, interval, now + 1u, app.Deadline(), app.pumps, clockReads);
            }
            text.Set(SignedInput(deadline), 400);
            SetClock(now, now + 1u);
            text.Tick();
            std::printf("{\"kind\":\"text-tick\",\"now\":%u,\"old\":%u,\"deadline\":%lld,"
                "\"acted\":%d,\"repaints\":%d,\"reads\":%u}\n",
                now, deadline, text.Deadline(), text.Blink() ? 1 : 0, text.repaints, clockReads);
            edit.Set(static_cast<int>(SignedInput(deadline)));
            SetClock(now, now + 1u);
            edit.Tick();
            bool blink = false;
            int result = 0;
            edit.getCursorBlink(blink, result);
            std::printf("{\"kind\":\"edit-tick\",\"now\":%u,\"old\":%u,\"completion\":%u,"
                "\"deadline\":%d,\"acted\":%d,\"repaints\":%d,\"reads\":%u}\n",
                now, deadline, now + 1u, result, blink ? 1 : 0, edit.repaints, clockReads);
        }
        text.Set(0, 400);
        SetClock(now, now + 1u);
        text.resetCursorBlink();
        std::printf("{\"kind\":\"text-reset\",\"now\":%u,\"deadline\":%lld,\"acted\":%d,\"reads\":%u}\n",
            now, text.Deadline(), text.Blink() ? 1 : 0, clockReads);
        edit.Set(0);
        SetClock(now, now + 1u);
        edit.setCursorBlink(true);
        bool blink = false;
        int result = 0;
        edit.getCursorBlink(blink, result);
        std::printf("{\"kind\":\"edit-set\",\"now\":%u,\"deadline\":%d,\"acted\":%d,"
            "\"repaints\":%d,\"reads\":%u}\n", now, result, blink ? 1 : 0, edit.repaints, clockReads);
    }
    for (auto reset : Values) {
        for (auto now : Values) {
            text.Set(0, 400);
            SetClock(reset, reset);
            text.resetCursorBlink();
            SetClock(now, now + 1u);
            text.Tick();
            std::printf("{\"kind\":\"text-after-reset\",\"reset\":%u,\"now\":%u,\"deadline\":%lld,"
                "\"acted\":%d,\"repaints\":%d,\"reads\":%u}\n",
                reset, now, text.Deadline(), text.Blink() ? 1 : 0, text.repaints, clockReads);
        }
    }
    return 0;
}
