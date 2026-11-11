#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "multisource_constants.inc"

using BOOL = int;
enum USE_TYPE { USE_OFF, USE_ON, USE_SET, USE_TOGGLE };
enum GLOBALESTATE { GLOBAL_OFF, GLOBAL_ON, GLOBAL_DEAD };
enum ALERT_TYPE { at_console, at_aiconsole };

// Engine string/alert seam: borrowed stable strings, with real diagnostics retained.
#define STRING(value) (value)
void ALERT(ALERT_TYPE, const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
}

struct Entvars {
    const char *classname = "multisource";
    const char *targetname = "tele_start_ms";
    const char *target = nullptr;
    int spawnflags = 0;
};

struct CBaseEntity {
    Entvars vars;
    Entvars *pev = &vars;
};

// EHANDLE engine-resolution seam: stable, live registered entities, pointer identity comparison.
struct EntityHandleSeam {
    CBaseEntity *entity = nullptr;
    operator CBaseEntity *() {
        return entity;
    }
};

constexpr std::uint64_t kArrayCanary = UINT64_C(0x192837465abcdef0);
template <typename T> struct ObservedArray {
    std::uint64_t before = kArrayCanary;
    std::array<T, MS_MAX_TARGETS> values{};
    std::uint64_t after = kArrayCanary;
    int accesses = 0;
    T &operator[](int index) {
        ++accesses;
        return values.at(static_cast<std::size_t>(index));
    }
};

struct GlobalStateSeam {
    GLOBALESTATE state = GLOBAL_OFF;
    int reads = 0;
    GLOBALESTATE EntityGetState(const char *) {
        ++reads;
        return state;
    }
} gGlobalState;

struct Dispatch {
    int count = 0;
    CBaseEntity *activator = nullptr;
    USE_TYPE type = USE_OFF;
    float value = -1;
};

struct CMultiSource : CBaseEntity {
    ObservedArray<EntityHandleSeam> m_rgEntities;
    ObservedArray<int> m_rgTriggered;
    int m_iTotal = 0;
    const char *m_globalstate = nullptr;
    Dispatch dispatch;
    void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
    BOOL IsTriggered(CBaseEntity *pActivator);
    // Engine target-dispatch seam: observe the exact output of the real decision body.
    void SUB_UseTargets(CBaseEntity *activator, USE_TYPE type, float value) {
        ++dispatch.count;
        dispatch.activator = activator;
        dispatch.type = type;
        dispatch.value = value;
    }
};

#include "multisource_bodies.inc"

void Check(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

struct Fixture {
    CMultiSource master;
    std::array<CBaseEntity, MS_MAX_TARGETS> members;
    CBaseEntity delayed;
    CBaseEntity activator;
    explicit Fixture(int total) {
        master.m_iTotal = total;
        delayed.pev->classname = "DelayedUse";
        for (std::size_t i = 0; i < members.size(); ++i) {
            members[i].pev->classname = "trigger_relay";
            master.m_rgEntities.values[i].entity = &members[i];
        }
        gGlobalState = {};
    }
};

void ChapterDelayedRelay() {
    // Preserved c1a0c: fade_in trigger_relay, delay=1, target=tele_start_ms;
    // live master has one registered relay and bit 0. DelayThink dispatches as DelayedUse.
    Fixture f(1);
    f.members[0].pev->targetname = "fade_in";
    f.members[0].pev->target = "tele_start_ms";
    f.master.Use(&f.activator, &f.delayed, USE_TOGGLE, 0);
    std::printf("chapter c1a0c tele_start_ms: total=1 triggered=%d IsTriggered=%d dispatch=%d\n",
        f.master.m_rgTriggered.values[0], f.master.IsTriggered(&f.activator), f.master.dispatch.count);
    Check(f.master.m_rgTriggered.values[0] == 1, "DelayedUse must toggle the single registered relay bit to 1");
    Check(f.master.IsTriggered(&f.activator) == 1, "DelayedUse must enable tele_start_ms");
    Check(f.master.dispatch.count == 1, "enabled chapter master must dispatch once");
    Check(f.master.dispatch.type == USE_TOGGLE && f.master.dispatch.activator == nullptr &&
        f.master.dispatch.value == 0, "ungated output must preserve null activator, USE_TOGGLE and zero value");
}

void SlotSelection() {
    constexpr std::array<int, 3> kCounts = {1, 2, 32};
    for (int total : kCounts) {
        for (int slot = 0; slot < total; ++slot) {
            Fixture f(total);
            f.master.Use(&f.activator, &f.members[slot], USE_OFF, 7);
            for (int i = 0; i < MS_MAX_TARGETS; ++i) {
                Check(f.master.m_rgTriggered.values[i] == (i == slot ? 1 : 0), "member must toggle only its own slot");
            }
        }
        for (bool nullCaller : {false, true}) {
            Fixture f(total);
            CBaseEntity outsider;
            outsider.pev->classname = "unregistered_trigger_relay";
            f.master.Use(&f.activator, nullCaller ? nullptr : &outsider, USE_ON, 7);
            for (int i = 0; i < MS_MAX_TARGETS; ++i) {
                Check(f.master.m_rgTriggered.values[i] == (i == total - 1 ? 1 : 0),
                    "unmatched or null caller must toggle only the final registered slot");
            }
        }
        Fixture completing(total);
        for (int i = 0; i < total - 1; ++i) {
            completing.master.m_rgTriggered.values[i] = 1;
        }
        completing.master.Use(nullptr, &completing.delayed, USE_TOGGLE, 0);
        Check(completing.master.IsTriggered(nullptr) == 1 && completing.master.dispatch.count == 1,
            "final-slot fallback must complete every valid count including full capacity");
    }
    Fixture duplicate(2);
    duplicate.master.m_rgEntities.values[1].entity = &duplicate.members[0];
    duplicate.master.Use(nullptr, &duplicate.members[0], USE_TOGGLE, 0);
    Check(duplicate.master.m_rgTriggered.values[0] == 1 && duplicate.master.m_rgTriggered.values[1] == 0,
        "duplicate handles must select first match");
}

void InvalidCounts() {
    for (int total : {0, -1, 33}) {
        for (int callerKind : {0, 1, 2}) {
            Fixture f(total);
            f.master.m_rgTriggered.values.fill(7);
            const auto originalHandles = f.master.m_rgEntities.values;
            const auto originalTriggers = f.master.m_rgTriggered.values;
            CBaseEntity *caller = callerKind == 0 ? nullptr : callerKind == 1 ? &f.members[0] : &f.delayed;
            f.master.Use(&f.activator, caller, USE_TOGGLE, 0);
            Check(f.master.m_rgEntities.accesses == 0 && f.master.m_rgTriggered.accesses == 0,
                "invalid count must perform no array access");
            Check(f.master.m_rgTriggered.values == originalTriggers, "invalid count must preserve trigger values");
            for (std::size_t i = 0; i < originalHandles.size(); ++i) {
                Check(f.master.m_rgEntities.values[i].entity == originalHandles[i].entity,
                    "invalid count must preserve handles");
            }
            Check(f.master.m_rgEntities.before == kArrayCanary && f.master.m_rgEntities.after == kArrayCanary &&
                f.master.m_rgTriggered.before == kArrayCanary && f.master.m_rgTriggered.after == kArrayCanary,
                "invalid count must preserve array canaries");
            Check(f.master.dispatch.count == 0 && gGlobalState.reads == 0,
                "invalid count must not dispatch or query global state");
        }
    }
}

void GatingAndToggling() {
    Fixture f(2);
    f.master.Use(&f.activator, &f.members[0], USE_ON, 3);
    Check(f.master.IsTriggered(nullptr) == 0 && f.master.dispatch.count == 0, "partial master must stay disabled");
    f.master.Use(&f.activator, &f.members[1], USE_OFF, 3);
    Check(f.master.IsTriggered(nullptr) == 1 && f.master.dispatch.count == 1, "all inputs must enable master");
    f.master.Use(&f.activator, &f.members[1], USE_ON, 3);
    Check(f.master.m_rgTriggered.values[1] == 0 && f.master.IsTriggered(nullptr) == 0 &&
        f.master.dispatch.count == 1, "repeated input must toggle off without dispatch");
    f.master.Use(&f.activator, &f.members[1], USE_OFF, 3);
    Check(f.master.m_rgTriggered.values[1] == 1 && f.master.dispatch.count == 2, "third input must toggle on again");

    Fixture initializing(1);
    initializing.master.pev->spawnflags = SF_MULTI_INIT;
    initializing.master.Use(nullptr, &initializing.delayed, USE_TOGGLE, 0);
    Check(initializing.master.m_rgTriggered.values[0] == 1 && initializing.master.IsTriggered(nullptr) == 0 &&
        initializing.master.dispatch.count == 0, "initializing master may toggle but cannot enable or dispatch");

    for (GLOBALESTATE state : {GLOBAL_OFF, GLOBAL_ON, GLOBAL_DEAD}) {
        Fixture gated(1);
        gated.master.m_globalstate = "chapter_gate";
        gGlobalState.state = state;
        gated.master.Use(&gated.activator, &gated.delayed, USE_OFF, 9);
        const bool enabled = state == GLOBAL_ON;
        Check(gated.master.m_rgTriggered.values[0] == 1 && (gated.master.IsTriggered(nullptr) != 0) == enabled,
            "global state must gate completion without preventing toggle");
        Check(gated.master.dispatch.count == (enabled ? 1 : 0), "only GLOBAL_ON may dispatch");
        if (enabled) {
            Check(gated.master.dispatch.type == USE_ON && gated.master.dispatch.activator == nullptr &&
                gated.master.dispatch.value == 0, "global-gated output must use USE_ON, null activator and zero value");
        }
    }
}

int main() {
    ChapterDelayedRelay();
    SlotSelection();
    InvalidCounts();
    GatingAndToggling();
    std::puts("PASS: chapter delayed relay; counts 1/2/32; all member slots; unmatched/null fallback; invalid "
        "counts 0/-1/33 without access; repeated toggles; initialization; global state; dispatch arguments");
}
