#define CSE_ABI_ENGINE_VIEW
#include "../client_abi/fixture_types.h"
#include "../../../GoldSrc-SDK/public/cse_module_abi.h"
#include "../strings/high_offset_buffer.h"
#include "../client_mouse_coordinates/controlled_window.h"
#include <new>
#include <type_traits>

SDL_Window **pmainwindow = &ownedWindow;
cl_enginefunc_dst_t g_engdstAddrs{};
#include "actual_callback.inc"
#include "actual_destination.inc"

template<typename Type, int Identity>
struct UnrelatedCallback;

template<int Identity, typename Result, typename... Arguments>
struct UnrelatedCallback<Result (*)(Arguments...), Identity> {
    static Result Call(Arguments...) {
        std::abort();
    }
};

template<int Identity, typename Result, typename... Arguments>
struct UnrelatedCallback<Result (*)(Arguments..., ...), Identity> {
    static Result Call(Arguments..., ...) {
        std::abort();
    }
};

#include "actual_engine_table.inc"

struct Outputs {
    int before = 23;
    int x = 101;
    int y = 102;
    int centerX = 103;
    int centerY = 104;
    int after = 29;
};

static Outputs *outputs = nullptr;
static int hookCalls = 0;
static bool redirect = false;
static bool invalidate = false;

void ObserveDestination(int **x, int **y, int **centerX, int **centerY) {
    ++hookCalls;
    if (redirect) {
        *x = &outputs->x;
        *y = &outputs->y;
        *centerX = &outputs->centerX;
        *centerY = &outputs->centerY;
    } else {
        CHECK(*x == &outputs->x && *y == &outputs->y);
        CHECK(*centerX == &outputs->centerX && *centerY == &outputs->centerY);
    }
    if (invalidate) {
        *centerY = nullptr;
    }
}

static void Reset() {
    ResetWindow();
    *outputs = Outputs{};
    hookCalls = 0;
    redirect = false;
    invalidate = false;
    pmainwindow = &ownedWindow;
    g_engdstAddrs.pfnGetClientMousePositionAndCenter = ObserveDestination;
}

static bool Query() {
    return cl_enginefuncs.pfnGetClientMousePositionAndCenter(
        &outputs->x, &outputs->y, &outputs->centerX, &outputs->centerY) != 0;
}

static void CheckUntouched() {
    CHECK(outputs->x == 101 && outputs->y == 102);
    CHECK(outputs->centerX == 103 && outputs->centerY == 104);
    CHECK(outputs->before == 23 && outputs->after == 29);
}

int main() {
    using Source = int (*)(int *, int *, int *, int *);
    using Destination = void (*)(int **, int **, int **, int **);
    static_assert(std::is_same<decltype(cl_enginefunc_t::pfnGetClientMousePositionAndCenter), Source>::value,
        "Owned source callback must retain its complete type");
    static_assert(std::is_same<decltype(cl_enginefunc_dst_t::pfnGetClientMousePositionAndCenter), Destination>::value,
        "Owned destination must retain pointer-to-pointer arguments");
    static_assert(cse::abi::RuntimeVersion == 10, "Coordinated owned semantic epoch");
    static_assert(sizeof(cse::abi::Descriptor) == 288, "Envelope is unchanged");
    CHECK(cl_enginefuncs.pfnGetClientMousePositionAndCenter == hudGetClientMousePositionAndCenter);
    CHECK(cl_enginefuncs.pfnFilteredClientCmd == controlled_engine::hudFilteredClientCmd);
    HighOffsetBuffer memory(nullptr);
    CHECK(memory.get() != nullptr);
    outputs = new (memory.get()) Outputs{};
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        CHECK(reinterpret_cast<std::uintptr_t>(outputs) > UINT32_MAX);
    }
    Reset();
    int *x = &outputs->x;
    int *y = &outputs->y;
    int *centerX = &outputs->centerX;
    int *centerY = &outputs->centerY;
    // Invoke the actual default initializer's entry, not a cast of NullDst.
    const cl_enginefunc_dst_t defaults = k_engdstNull;
    CHECK(defaults.pfnGetClientMousePositionAndCenter == NullClientMousePositionAndCenterDst);
    defaults.pfnGetClientMousePositionAndCenter(&x, &y, &centerX, &centerY);
    CHECK(x == &outputs->x && y == &outputs->y);
    CHECK(centerX == &outputs->centerX && centerY == &outputs->centerY);
    CheckUntouched();
    CHECK(Query() && hookCalls == 1);
    CHECK(outputs->x == 1000 && outputs->y == 600 && outputs->centerX == 1000 && outputs->centerY == 600);
    CHECK(controlled.mouseReads == 1 && controlled.forbiddenCalls == 0);
    Reset();
    redirect = true;
    int *inaccessible = reinterpret_cast<int *>(static_cast<std::uintptr_t>(1));
    CHECK(cl_enginefuncs.pfnGetClientMousePositionAndCenter(inaccessible, inaccessible, inaccessible, inaccessible));
    CHECK(outputs->x == 1000 && outputs->y == 600 && hookCalls == 1);
    Reset();
    controlled.windowed = false;
    controlled.mouseX = 250.0f;
    controlled.aspectX = 0.5f;
    CHECK(Query() && hookCalls == 1);
    CHECK(outputs->x == 480 && outputs->y == 480 && outputs->centerX == 640 && outputs->centerY == 480);
    Reset();
    invalidate = true;
    CHECK(!Query() && hookCalls == 1 && controlled.mouseReads == 0);
    CheckUntouched();
    Reset();
    pmainwindow = nullptr;
    CHECK(!Query() && hookCalls == 1 && controlled.mouseReads == 0);
    CheckUntouched();
    Reset();
    ownedWindow = nullptr;
    CHECK(!Query() && controlled.mouseReads == 0);
    CheckUntouched();
    ownedWindow = reinterpret_cast<SDL_Window *>(static_cast<std::uintptr_t>(0x12345678));
    Reset();
    controlled.pixelsSucceed = false;
    CHECK(!Query() && hookCalls == 1);
    CheckUntouched();
    Reset();
    controlled.mouseX = std::numeric_limits<float>::infinity();
    CHECK(!Query() && hookCalls == 1);
    CheckUntouched();
    Reset();
    g_engdstAddrs = defaults;
    CHECK(Query() && hookCalls == 0 && outputs->x == 1000 && outputs->centerX == 1000);
    CHECK(controlled.forbiddenCalls == 0);
    outputs->~Outputs();
    outputs = nullptr;
    std::printf("Owned mouse callback: %d checks, pointer_bits=%zu\n", CheckCount(), sizeof(void *) * 8);
    return 0;
}

// Unrelated legacy destinations are present in the actual table initializer;
// none is called by this fixture.
void NullDst() {}
