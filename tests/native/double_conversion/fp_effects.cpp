#include "binary_io.h"
#include "corpus.h"
#include "retained_boundary_results.h"

#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#include <float.h>
#include <intrin.h>
#include "reference_clock.h"
#endif
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

std::int32_t ScalarDoubleCall(double value);
std::int32_t NativeDoubleCall(double value);

namespace {

struct FpState {
    std::uint64_t control = 0;
    std::uint64_t status = 0;
    unsigned int x87Control = 0;
    unsigned int x87Status = 0;
    unsigned int crtSseStatus = 0;
};

#if defined(_M_ARM64) || defined(__aarch64__)
constexpr std::uint64_t ArmExceptionEnables = UINT64_C(0x9f00);
#else
constexpr std::uint64_t X86StatusMask = UINT64_C(0x3f);
constexpr std::uint64_t X86ExceptionMasks = UINT64_C(0x1f80);
#endif

FpState ReadState() {
    FpState state;
#if defined(_M_ARM64)
    // Same selected Windows SDK register encodings as the existing A3D fixture.
    constexpr int FpcrRegister = 0x5a20;
    constexpr int FpsrRegister = 0x5a21;
    state.control = static_cast<std::uint64_t>(_ReadStatusReg(FpcrRegister));
    state.status = static_cast<std::uint64_t>(_ReadStatusReg(FpsrRegister));
#elif defined(__aarch64__)
    __asm__ volatile("mrs %0, fpcr" : "=r"(state.control) : : "memory");
    __asm__ volatile("mrs %0, fpsr" : "=r"(state.status) : : "memory");
#else
    const unsigned int csr = _mm_getcsr();
    state.control = csr & ~X86StatusMask;
    state.status = csr & X86StatusMask;
#endif
#if defined(_M_IX86)
    unsigned int sseControl = 0;
    __control87_2(0, 0, &state.x87Control, &sseControl);
    _statusfp2(&state.x87Status, &state.crtSseStatus);
#endif
    return state;
}

void WriteState(const FpState& state) {
#if defined(_M_ARM64)
    constexpr int FpcrRegister = 0x5a20;
    constexpr int FpsrRegister = 0x5a21;
    std::int64_t control;
    std::int64_t status;
    std::memcpy(&control, &state.control, sizeof(control));
    std::memcpy(&status, &state.status, sizeof(status));
    _WriteStatusReg(FpcrRegister, control);
    _WriteStatusReg(FpsrRegister, status);
#elif defined(__aarch64__)
    __asm__ volatile("msr fpcr, %0" : : "r"(state.control) : "memory");
    __asm__ volatile("msr fpsr, %0" : : "r"(state.status) : "memory");
#else
    _mm_setcsr(static_cast<unsigned int>(state.control | state.status));
#endif
}

void ClearStatus() {
    double_conversion_fixture::Require(std::feclearexcept(FE_ALL_EXCEPT) == 0, "clear diagnostic FP flags");
    FpState state = ReadState();
    state.status = 0;
    WriteState(state);
}

using Conversion = std::int32_t (*)(double);

struct Route {
    const char* name;
    Conversion function;
    bool helper;
};

#if defined(_WIN32)
CSystem referenceClock;

std::int32_t MethodCall(double) {
    return static_cast<std::int32_t>(referenceClock.GetTimeMillis());
}
#endif

}

int main() {
    using namespace double_conversion_fixture;
    std::fenv_t originalEnvironment;
    Require(std::fegetenv(&originalEnvironment) == 0, "save the process-local FP environment");
    const FpState originalState = ReadState();
#if defined(_M_ARM64) || defined(__aarch64__)
    Require((originalState.control & ArmExceptionEnables) == 0, "masked exceptions are a reference precondition");
#else
    Require((originalState.control & X86ExceptionMasks) == X86ExceptionMasks,
        "masked exceptions are a reference precondition");
#endif
    const Route routes[] = {
        {"scalar", ScalarDoubleCall, true},
        {"native", NativeDoubleCall, true},
#if defined(_WIN32)
        {"raw_reference", ReferenceRawDoubleCast, false},
        {"actual_method", MethodCall, false},
#endif
    };
    const int roundingModes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    std::puts("input_bits,route,rounding,seeded,result,control_before,control_after,status_before,status_after,"
        "x87_control_before,x87_control_after,x87_status_before,x87_status_after,crt_sse_before,crt_sse_after");
    for (int rounding : roundingModes) {
        Require(std::fesetround(rounding) == 0, "set a diagnostic rounding mode");
        for (const RetainedDoubleResult& entry : RetainedDoubleResults) {
            const double value = FromBits(entry.inputBits);
#if defined(_WIN32)
            SetReferenceSeconds(value / 1000.0);
#endif
            for (const Route& route : routes) {
                constexpr int StatusSeedModes = 2;
                for (int seeded = 0; seeded < StatusSeedModes; ++seeded) {
                    ClearStatus();
                    if (seeded != 0) {
                        Require(std::feraiseexcept(FE_DIVBYZERO) == 0, "seed an existing sticky exception flag");
                    }
                    const FpState before = ReadState();
                    if (seeded != 0) {
                        Require(before.status != 0 || before.x87Status != 0, "observe the seeded sticky flag");
                    }
                    const std::int32_t result = route.function(value);
                    const FpState after = ReadState();
                    Require(before.control == after.control && before.x87Control == after.x87Control,
                        "conversion preserves control registers");
                    if (route.helper) {
                        Require(static_cast<std::uint32_t>(result) == entry.expectedBits,
                            "helper values match Win32 under each rounding mode");
                        Require((after.status & before.status) == before.status
                            && (after.x87Status & before.x87Status) == before.x87Status,
                            "helper preserves previously set sticky status flags");
                    }
                    std::printf("%016llx,%s,%d,%d,%08x,%016llx,%016llx,%016llx,%016llx,"
                        "%08x,%08x,%08x,%08x,%08x,%08x\n",
                        static_cast<unsigned long long>(entry.inputBits), route.name, rounding, seeded,
                        static_cast<std::uint32_t>(result), static_cast<unsigned long long>(before.control),
                        static_cast<unsigned long long>(after.control), static_cast<unsigned long long>(before.status),
                        static_cast<unsigned long long>(after.status), before.x87Control, after.x87Control,
                        before.x87Status, after.x87Status, before.crtSseStatus, after.crtSseStatus);
                }
            }
        }
    }
    Require(std::fesetenv(&originalEnvironment) == 0, "restore the process-local FP environment");
    WriteState(originalState);
    return 0;
}
