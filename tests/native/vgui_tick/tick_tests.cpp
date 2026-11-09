#include <VGUI_App.h>
#include <VGUI_Scheme.h>
#include <VGUI_TickSignal.h>

#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

extern long vguiTestClockMillis;
extern "C" std::size_t VguiPublicAppSize();

namespace {

constexpr long SignedClockBoundaryMillis = 2147483648L;
constexpr long EncodedClockPeriodMillis = 4294967296L;

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
}

class TestApp : public vgui::App {
public:
    ~TestApp() {
        // App constructs this exact Scheme type, which has no virtual destructor.
        vgui::Scheme* scheme = getScheme();
        scheme->vgui::Scheme::~Scheme();
        ::operator delete(scheme);
    }

    void main(int, char**) override {
    }
};

class CountTicks : public vgui::TickSignal {
public:
    void ticked() override {
        ++count;
        if (advanceClock) {
            vguiTestClockMillis = completionClock;
            advanceClock = false;
        }
    }

    int count = 0;
    bool advanceClock = false;
    long completionClock = 0;
};

void TickAt(TestApp& app, CountTicks& signal, long clock, int expectedCount, const char* description) {
    vguiTestClockMillis = clock;
    app.externalTick();
    Require(signal.count == expectedCount, description);
}

void Ordinary() {
    vguiTestClockMillis = 100;
    TestApp app;
    CountTicks signal;
    app.addTickSignal(&signal);
    TickAt(app, signal, 99, 0, "initial deadline defers an earlier clock");
    TickAt(app, signal, 100, 1, "initial deadline is immediately eligible");
    TickAt(app, signal, 149, 1, "default interval defers the preceding millisecond");
    TickAt(app, signal, 150, 2, "default interval fires at its exact deadline");
    app.setMinimumTickMillisInterval(0);
    TickAt(app, signal, 200, 3, "changed interval takes effect when a tick finishes");
    TickAt(app, signal, 200, 4, "zero interval permits another tick at the same clock");
}

void CompletionClock() {
    vguiTestClockMillis = 100;
    TestApp app;
    CountTicks signal;
    signal.advanceClock = true;
    signal.completionClock = 500;
    app.addTickSignal(&signal);
    TickAt(app, signal, 100, 1, "callback executes on the first tick");
    TickAt(app, signal, 549, 1, "next deadline uses the clock after callbacks");
    TickAt(app, signal, 550, 2, "completion-based exact deadline fires");
}

void InitialSign() {
    constexpr long InitialClock = SignedClockBoundaryMillis + 10;
    vguiTestClockMillis = InitialClock;
    TestApp app;
    CountTicks signal;
    app.addTickSignal(&signal);
    TickAt(app, signal, InitialClock - 1, 0, "initial negative signed32 clock retains signed ordering");
    TickAt(app, signal, InitialClock, 1, "initial negative signed32 clock fires at equality");
}

void SignedWrap() {
    constexpr long InitialClock = SignedClockBoundaryMillis - 32;
    vguiTestClockMillis = InitialClock;
    TestApp app;
    CountTicks signal;
    app.addTickSignal(&signal);
    TickAt(app, signal, InitialClock, 1, "tick schedules across INT32_MAX");
    TickAt(app, signal, InitialClock + 1, 2, "legacy signed comparison permits a pre-wrap early tick");
    TickAt(app, signal, SignedClockBoundaryMillis, 2, "negative post-wrap clock defers until the signed deadline");
    TickAt(app, signal, SignedClockBoundaryMillis + 19, 3, "post-wrap clock fires at the encoded deadline");
}

void FullWrap() {
    constexpr long InitialClock = EncodedClockPeriodMillis - 16;
    vguiTestClockMillis = InitialClock;
    TestApp app;
    CountTicks signal;
    app.addTickSignal(&signal);
    TickAt(app, signal, InitialClock, 1, "tick schedules from encoded minus sixteen");
    TickAt(app, signal, InitialClock + 1, 1, "negative clock waits before the unsigned32 wrap");
    TickAt(app, signal, EncodedClockPeriodMillis + 33, 1, "encoded thirty-three waits after the unsigned32 wrap");
    TickAt(app, signal, EncodedClockPeriodMillis + 34, 2, "encoded thirty-four fires after the unsigned32 wrap");
}

void NegativeClock() {
    vguiTestClockMillis = -16;
    TestApp app;
    CountTicks signal;
    app.addTickSignal(&signal);
    TickAt(app, signal, -16, 1, "already-signed negative clock starts normally");
    TickAt(app, signal, -15, 1, "already-signed negative clock waits for deadline");
    TickAt(app, signal, 34, 2, "negative clock addition wraps to the same deadline");
}

void MultipleWraps() {
    constexpr long InitialClock = 2 * EncodedClockPeriodMillis + 100;
    vguiTestClockMillis = InitialClock;
    TestApp app;
    CountTicks signal;
    app.addTickSignal(&signal);
    TickAt(app, signal, InitialClock, 1, "high clock bits do not change initial eligibility");
    TickAt(app, signal, InitialClock + 49, 1, "high clock bits do not bypass the deadline");
    TickAt(app, signal, InitialClock + 50, 2, "multiple wraps preserve the low32 exact deadline");
}

void NegativeInterval() {
    vguiTestClockMillis = 100;
    TestApp app;
    CountTicks signal;
    app.setMinimumTickMillisInterval(-50);
    app.addTickSignal(&signal);
    TickAt(app, signal, 100, 1, "negative interval is retained");
    TickAt(app, signal, 99, 2, "negative interval remains immediately eligible");
    TickAt(app, signal, 48, 2, "signed comparison still defers before the negative-interval deadline");
    TickAt(app, signal, 49, 3, "negative-interval exact deadline fires");
}

void MaximumInterval() {
    vguiTestClockMillis = 1;
    TestApp app;
    CountTicks signal;
    app.setMinimumTickMillisInterval(INT_MAX);
    app.addTickSignal(&signal);
    TickAt(app, signal, 1, 1, "maximum interval addition encodes INT32_MIN");
    TickAt(app, signal, 2, 2, "maximum interval retains the legacy signed-order quirk");
    TickAt(app, signal, SignedClockBoundaryMillis, 2, "maximum interval waits at the preceding encoded value");
    TickAt(app, signal, SignedClockBoundaryMillis + 1, 3, "maximum interval fires at encoded equality");
}

void MinimumInterval() {
    vguiTestClockMillis = 0;
    TestApp app;
    CountTicks signal;
    app.setMinimumTickMillisInterval(INT_MIN);
    app.addTickSignal(&signal);
    TickAt(app, signal, 0, 1, "minimum interval schedules without signed arithmetic overflow");
    TickAt(app, signal, 0, 2, "minimum interval retains immediate signed-order eligibility");
    TickAt(app, signal, SignedClockBoundaryMillis, 3, "minimum interval fires at encoded equality");
    TickAt(app, signal, SignedClockBoundaryMillis + 1, 3, "minimum interval next addition wraps deadline to zero");
}

void LongLimits() {
    vguiTestClockMillis = LONG_MAX;
    TestApp app;
    CountTicks signal;
    app.addTickSignal(&signal);
    TickAt(app, signal, LONG_MAX, 1, "LONG_MAX maps to encoded minus one");
    TickAt(app, signal, LONG_MAX, 1, "LONG_MAX does not bypass its wrapped positive deadline");
    TickAt(app, signal, LONG_MIN, 1, "LONG_MIN maps to encoded zero");
    TickAt(app, signal, LONG_MIN + 48, 1, "LONG_MIN low32 predecessor waits");
    TickAt(app, signal, LONG_MIN + 49, 2, "LONG_MIN low32 exact deadline fires");
}

}

int main(int argc, char** argv) {
    Require(sizeof(long) == 8 && sizeof(int) == 4, "this behavioral fixture requires the actual LP64 profile");
    Require(argc == 2, "provide one clock scenario");
    const char* scenario = argv[1];
    if (std::strcmp(scenario, "ordinary") == 0) {
        Ordinary();
    } else if (std::strcmp(scenario, "completion-clock") == 0) {
        CompletionClock();
    } else if (std::strcmp(scenario, "initial-sign") == 0) {
        InitialSign();
    } else if (std::strcmp(scenario, "signed-wrap") == 0) {
        SignedWrap();
    } else if (std::strcmp(scenario, "full-wrap") == 0) {
        FullWrap();
    } else if (std::strcmp(scenario, "negative-clock") == 0) {
        NegativeClock();
    } else if (std::strcmp(scenario, "multiple-wraps") == 0) {
        MultipleWraps();
    } else if (std::strcmp(scenario, "negative-interval") == 0) {
        NegativeInterval();
    } else if (std::strcmp(scenario, "maximum-interval") == 0) {
        MaximumInterval();
    } else if (std::strcmp(scenario, "minimum-interval") == 0) {
        MinimumInterval();
    } else if (std::strcmp(scenario, "long-limits") == 0) {
        LongLimits();
    } else if (std::strcmp(scenario, "layout") == 0) {
        Require(sizeof(vgui::App) == VguiPublicAppSize(), "public and implementation App timing storage agrees");
    } else {
        Require(false, "unknown clock scenario");
    }
    std::printf("PASS: %s\n", scenario);
    return 0;
}
