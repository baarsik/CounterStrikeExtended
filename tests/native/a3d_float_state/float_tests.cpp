#include "a2d.h"
#include "mixer_filters.h"

#include <array>
#include <cerrno>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <float.h>
#if defined(_WIN32)
#include <crtdbg.h>
#include <intrin.h>
#endif
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace {

int invalidParameterCalls = 0;
int debugAssertionCalls = 0;
int unsupportedPrecisionAssertions = 0;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "A3D floating-state failure: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

#if defined(_WIN32)
void __cdecl RecordInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {
    ++invalidParameterCalls;
}

#if defined(_DEBUG)
int __cdecl RecordDebugReport(int type, wchar_t* message, int* result) {
    if (type == _CRT_ASSERT) {
        ++debugAssertionCalls;
        if (std::wcsstr(message, L"_MCW_DN|_MCW_EM|_MCW_RC")) {
            ++unsupportedPrecisionAssertions;
        }
        *result = 0;
        return TRUE;
    }
    return FALSE;
}
#endif

class InvalidParameterScope {
public:
    InvalidParameterScope() : previous(_set_thread_local_invalid_parameter_handler(RecordInvalidParameter)) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
        _CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, RecordDebugReport);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    }

    ~InvalidParameterScope() {
#if defined(_DEBUG)
        _CrtSetReportHookW2(_CRT_RPTHOOK_REMOVE, RecordDebugReport);
#endif
        _set_thread_local_invalid_parameter_handler(previous);
    }

private:
    _invalid_parameter_handler previous;
};
#endif

struct NativeState {
    std::uint64_t control;
    std::uint64_t status;
};

NativeState ReadNativeState() {
#if defined(_M_ARM64)
    // Encodings from selected Windows SDK ksarm64.h, ARM64_FPCR/FPSR.
    constexpr int fpcrRegister = 0x5a20;
    constexpr int fpsrRegister = 0x5a21;
    return {static_cast<std::uint64_t>(_ReadStatusReg(fpcrRegister)),
        static_cast<std::uint64_t>(_ReadStatusReg(fpsrRegister))};
#elif defined(__aarch64__)
    NativeState state{};
    __asm__ volatile("mrs %0, fpcr" : "=r"(state.control));
    __asm__ volatile("mrs %0, fpsr" : "=r"(state.status));
    return state;
#else
    return {_mm_getcsr(), 0};
#endif
}

void WriteNativeState(NativeState state) {
#if defined(_M_ARM64)
    constexpr int fpcrRegister = 0x5a20;
    constexpr int fpsrRegister = 0x5a21;
    std::int64_t control = 0;
    std::int64_t status = 0;
    std::memcpy(&control, &state.control, sizeof(control));
    std::memcpy(&status, &state.status, sizeof(status));
    _WriteStatusReg(fpcrRegister, control);
    _WriteStatusReg(fpsrRegister, status);
#elif defined(__aarch64__)
    __asm__ volatile("msr fpcr, %0" : : "r"(state.control) : "memory");
    __asm__ volatile("msr fpsr, %0" : : "r"(state.status) : "memory");
#else
    _mm_setcsr(static_cast<unsigned int>(state.control));
#endif
}

void StateTest() {
    std::fenv_t savedEnvironment{};
    Require(std::fegetenv(&savedEnvironment) == 0, "save caller floating environment");
    const auto original = ReadNativeState();
    constexpr int roundingModes = 4;
    constexpr int denormalModes = 2;
    int checkedStates = 0;
    for (int rounding = 0; rounding < roundingModes; ++rounding) {
        for (int flush = 0; flush < denormalModes; ++flush) {
            for (int trap = 0; trap < 2; ++trap) {
                NativeState requested = original;
#if defined(_M_ARM64) || defined(__aarch64__)
                constexpr std::uint64_t roundingMask = UINT64_C(3) << 22;
                constexpr std::uint64_t flushToZero = UINT64_C(1) << 24;
                constexpr std::uint64_t defaultNaN = UINT64_C(1) << 25;
                constexpr std::uint64_t exceptionEnables = 0x9f00;
                constexpr std::uint64_t invalidEnable = 0x100;
                constexpr std::uint64_t cumulativeExceptions = 0x9f;
                requested.control &= ~(roundingMask | flushToZero | defaultNaN | exceptionEnables);
                requested.control |= static_cast<std::uint64_t>(rounding) << 22;
                requested.control |= flush ? flushToZero | defaultNaN : 0;
                requested.control |= trap ? invalidEnable : 0;
                requested.status = trap ? 0 : cumulativeExceptions;
#else
                constexpr std::uint64_t roundingMask = 0x6000;
                constexpr std::uint64_t flushToZero = 0x8000;
                constexpr std::uint64_t exceptionMasks = 0x1f80;
                constexpr std::uint64_t invalidMask = 0x80;
                constexpr std::uint64_t cumulativeExceptions = 0x3f;
                requested.control &= ~(roundingMask | flushToZero | cumulativeExceptions);
                requested.control |= exceptionMasks | (static_cast<std::uint64_t>(rounding) << 13);
                requested.control |= flush ? flushToZero : 0;
                requested.control &= trap ? ~invalidMask : UINT64_MAX;
                requested.control |= trap ? 0 : cumulativeExceptions;
#endif
                WriteNativeState(requested);
                const auto before = ReadNativeState();
                invalidParameterCalls = 0;
                debugAssertionCalls = 0;
                unsupportedPrecisionAssertions = 0;
                errno = 0;
                Mixer_ResetFloatState();
                const int error = errno;
                const int invalid = invalidParameterCalls;
                const int assertions = debugAssertionCalls;
                const auto after = ReadNativeState();
                WriteNativeState(original);
                Require(std::fesetenv(&savedEnvironment) == 0, "restore caller environment before assertions");
                std::printf("state=%d control_before=%llx status_before=%llx control_after=%llx status_after=%llx "
                    "invalid_parameter_calls=%d debug_assertions=%d errno=%d\n", checkedStates,
                    static_cast<unsigned long long>(before.control), static_cast<unsigned long long>(before.status),
                    static_cast<unsigned long long>(after.control), static_cast<unsigned long long>(after.status),
                    invalid, assertions, error);
                Require(invalid == 0 && assertions == 0, "actual reset makes no unsupported CRT precision request");
                Require(before.control == after.control && before.status == after.status,
                    "all readable native SIMD control/status bits preserved");
                ++checkedStates;
            }
        }
    }
#if defined(_M_IX86)
    unsigned int control = 0;
    Require(__control87_2(_PC_24 | _RC_DOWN, _MCW_PC | _MCW_RC, &control, nullptr) != 0,
        "seed separate x87 precision and rounding");
    __asm {
        fldz
        fldz
        fdivp st(1), st(0)
        fstp st(0)
    }
    unsigned int beforeX87 = 0;
    unsigned int beforeSSE = 0;
    _statusfp2(&beforeX87, &beforeSSE);
    Mixer_ResetFloatState();
    unsigned int afterX87 = 0;
    unsigned int afterSSE = 0;
    _statusfp2(&afterX87, &afterSSE);
    Require(__control87_2(0, 0, &control, nullptr) != 0, "query actual reset x87 word");
    Require(std::fesetenv(&savedEnvironment) == 0, "restore caller x87 environment");
    WriteNativeState(original);
    std::printf("x87 status_before=%x status_after=%x control_after=%x SSE_before=%x SSE_after=%x\n",
        beforeX87, afterX87, control, beforeSSE, afterSSE);
    Require(beforeX87 != 0 && afterX87 == 0 && (control & _MCW_PC) == _PC_64 && (control & _MCW_RC) == _RC_NEAR,
        "original Win32 x87 reset/64-bit precision/nearest rounding retained");
#endif
    std::printf("checked_native_states=%d\n", checkedStates);
}

void PrecisionProbe() {
#if defined(_M_X64)
    const auto original = ReadNativeState();
    unsigned int word = 0;
    invalidParameterCalls = 0;
    debugAssertionCalls = 0;
    unsupportedPrecisionAssertions = 0;
    errno = 0;
    const int result = _controlfp_s(&word, _PC_64, _MCW_PC);
    const int error = errno;
    const int invalid = invalidParameterCalls;
    const int assertions = debugAssertionCalls;
    WriteNativeState(original);
    std::printf("actual_x64_controlfp_s_PC64 return=%d errno=%d invalid_parameter_calls=%d debug_assertions=%d\n",
        result, error, invalid, assertions);
#if defined(_DEBUG)
    constexpr int expectedAssertions = 1;
#else
    constexpr int expectedAssertions = 0;
#endif
    Require(result == 0 && error == 0 && invalid == 0 && assertions == expectedAssertions
        && unsupportedPrecisionAssertions == expectedAssertions,
        "pinned CRT returns zero without callback; Debug reports unsupported mask assertion");
#else
    std::puts("precision probe only applies to selected native x64 CRT");
#endif
}

std::uint64_t HashPcm(const std::vector<SHORT>& pcm) {
    constexpr std::uint64_t offset = UINT64_C(14695981039346656037);
    constexpr std::uint64_t prime = UINT64_C(1099511628211);
    std::uint64_t hash = offset;
    const auto* bytes = reinterpret_cast<const unsigned char*>(pcm.data());
    for (std::size_t index = 0; index < pcm.size() * sizeof(SHORT); ++index) {
        hash = (hash ^ bytes[index]) * prime;
    }
    return hash;
}

std::vector<SHORT> MixOwnedPcm(bool sixteenBit, bool stereo, bool filtered, bool looping, WORD sampleRate) {
    constexpr DWORD sourceFrames = 64;
    constexpr std::size_t sourceGuardFrames = 4;
    constexpr DWORD blockFrames = 16;
    constexpr int blocks = 7;
    constexpr std::size_t filterHistorySamples = 4;
    constexpr LONG unityGain = 65536;
    constexpr LONG spatialUnity = 32768;
    constexpr int samplePeriod = 65536;
    constexpr int signedSampleOffset = 32768;
    constexpr int sampleStride = 3259;
    const std::size_t channels = stereo ? 2 : 1;
    // Existing boundary/cubic readers inspect guard samples. Keep every such
    // read in deterministic owned storage; unpadded-buffer safety is not claimed.
    std::vector<SHORT> samples16((sourceFrames + sourceGuardFrames * 2) * channels);
    std::vector<BYTE> samples8((sourceFrames + sourceGuardFrames * 2) * channels);
    for (std::size_t index = 0; index < samples16.size(); ++index) {
        samples16[index] = static_cast<SHORT>((static_cast<int>(index) * sampleStride) % samplePeriod - signedSampleOffset);
        constexpr unsigned int byteStride = 37;
        constexpr unsigned int byteOffset = 71;
        samples8[index] = static_cast<BYTE>((index * byteStride + byteOffset) & UINT8_MAX);
    }
    a3d_mix_input_t input{};
    input.pSamples = sixteenBit ? reinterpret_cast<BYTE*>(samples16.data() + sourceGuardFrames * channels)
        : samples8.data() + sourceGuardFrames * channels;
    input.pCurrent = input.pSamples;
    input.cFrames = sourceFrames;
    input.pEnd = input.pSamples + sourceFrames * channels * (sixteenBit ? sizeof(SHORT) : sizeof(BYTE));
    input.dwFlags = (sixteenBit ? A3D_MIX_16BIT : 0) | (stereo ? A3D_MIX_STEREO : 0)
        | (looping ? A3D_MIX_LOOPING : 0);
    a3d_mix_voice_t voice{};
    Require(AR2Dinit(&voice, &input, sampleRate) == 0, "actual voice initialization");
    const DWORD rate = AR2DSetFrequency(&voice, voice.dwSampleRate);
    voice.lRateCurrent = static_cast<LONG>(rate);
    voice.lRateTarget = static_cast<LONG>(rate);
    voice.lRateRequested = static_cast<LONG>(rate);
    const std::array<SHORT, 4> impulse = {32767, 16384, -8192, 4096};
    for (std::size_t channel = 0; channel < std::size(voice.channels); ++channel) {
        voice.channels[channel].lGainBase = channel ? unityGain / 2 : unityGain;
        voice.channels[channel].lGainSpatial = spatialUnity;
        voice.channels[channel].lGainCurrent = 0;
        if (filtered) {
            Mixer_SetImpulseResponse(&voice.channels[channel], impulse.data(), static_cast<DWORD>(impulse.size()));
        }
    }
    mixer_filter_state_t filter{};
    Mixer_InitializeFilters(nullptr, &filter, sampleRate);
    std::vector<SHORT> result;
    for (int block = 0; block < blocks; ++block) {
        std::array<float, blockFrames * 2 + filterHistorySamples> accumulation{};
        float* output = accumulation.data() + filterHistorySamples;
        Require(AR2Dproc(&input, output, &voice, blockFrames, filtered ? TRUE : FALSE) == 0,
            "actual mix/loop/convolution pipeline");
        if (filtered) {
            Mixer_FilterStereo(output, 1, &filter, blockFrames);
        }
        constexpr SHORT guard = 12345;
        std::array<SHORT, blockFrames * 2 + 2> pcm{};
        pcm.front() = guard;
        pcm.back() = guard;
        Mixer_FloatToPcm16(output, pcm.data() + 1, blockFrames * 2);
        Require(pcm.front() == guard && pcm.back() == guard, "actual PCM conversion stays inside owned output");
        result.insert(result.end(), pcm.begin() + 1, pcm.end() - 1);
    }
    return result;
}

void PcmTest(const std::filesystem::path& reference, bool capture) {
    std::fenv_t savedEnvironment{};
    Require(std::fegetenv(&savedEnvironment) == 0, "save PCM caller environment");
    const auto original = ReadNativeState();
    NativeState nearest = original;
#if defined(_M_ARM64) || defined(__aarch64__)
    constexpr std::uint64_t defaultControlMask = (UINT64_C(3) << 22) | (UINT64_C(1) << 24)
        | (UINT64_C(1) << 25) | UINT64_C(0x9f00);
    nearest.control &= ~defaultControlMask;
    nearest.status = 0;
#else
    constexpr std::uint64_t nonDefaultControl = 0x6000 | 0x8000 | 0x40 | 0x3f;
    constexpr std::uint64_t exceptionMasks = 0x1f80;
    nearest.control = (nearest.control & ~nonDefaultControl) | exceptionMasks;
#endif
    WriteNativeState(nearest);
    Require(std::fesetround(FE_TONEAREST) == 0 && std::feclearexcept(FE_ALL_EXCEPT) == 0,
        "establish recorded default nearest/masked/denormal-preserving PCM environment");
    invalidParameterCalls = 0;
    debugAssertionCalls = 0;
    unsupportedPrecisionAssertions = 0;
    if (capture) {
        std::filesystem::create_directories(reference);
    }
    int cases = 0;
    for (WORD sampleRate : {static_cast<WORD>(22), static_cast<WORD>(44)}) {
        for (bool sixteenBit : {false, true}) {
            for (bool stereo : {false, true}) {
                for (bool filtered : {false, true}) {
                    for (bool looping : {false, true}) {
                        const auto pcm = MixOwnedPcm(sixteenBit, stereo, filtered, looping, sampleRate);
                        const std::string name = std::string(sixteenBit ? "s16" : "u8")
                            + (stereo ? "-stereo" : "-mono") + (filtered ? "-filtered" : "-direct")
                            + (looping ? "-loop" : "-tail") + "-" + std::to_string(sampleRate);
                        const auto path = reference / (name + ".pcm");
                        if (capture) {
                            std::ofstream file(path, std::ios::binary);
                            file.write(reinterpret_cast<const char*>(pcm.data()),
                                static_cast<std::streamsize>(pcm.size() * sizeof(SHORT)));
                            Require(file.good(), "write owned original mixer PCM receipt");
                        } else {
                            std::ifstream file(path, std::ios::binary);
                            Require(file.good(), "open pinned original PCM receipt");
                            const std::vector<char> expected(std::istreambuf_iterator<char>(file), {});
                            Require(expected.size() == pcm.size() * sizeof(SHORT)
                                && std::memcmp(expected.data(), pcm.data(), expected.size()) == 0,
                                "actual pipeline PCM bytes match original recorded output");
                        }
                        std::printf("PCM %s samples=%zu hash=%016llx\n", name.c_str(), pcm.size(),
                            static_cast<unsigned long long>(HashPcm(pcm)));
                        ++cases;
                    }
                }
            }
        }
    }
    const int invalid = invalidParameterCalls;
    const int assertions = debugAssertionCalls;
    const int precisionAssertions = unsupportedPrecisionAssertions;
    Require(std::fesetenv(&savedEnvironment) == 0, "restore PCM caller environment");
    WriteNativeState(original);
    std::printf("PCM cases=%d invalid_parameter_calls=%d debug_assertions=%d capture=%d\n",
        cases, invalid, assertions, capture ? 1 : 0);
    Require(invalid == 0 && precisionAssertions == assertions && (capture || assertions == 0),
        "capture accepts only recorded precision-mask assertions; qualified pipeline reports none");
}

void ConversionProbe(bool verify = false) {
    std::fenv_t savedEnvironment{};
    Require(std::fegetenv(&savedEnvironment) == 0, "save conversion floating environment");
    const NativeState original = ReadNativeState();
    // IEEE binary32 values: NaN, infinities, int32 edges, large finite,
    // half-way ties, PCM clipping and signed zero. Captured on Windows first.
    constexpr std::uint32_t inputs[] = {
        0x7fc00000, 0x7f800000, 0xff800000, 0x4f000000, 0x4effffff,
        0xcf000000, 0xcf000001, 0x7f7fffff, 0xff7fffff, 0x3f000000,
        0x3fc00000, 0x40200000, 0xbf000000, 0xbfc00000, 0xc0200000,
        0x46fffe00, 0x46ffff00, 0x47000000, 0xc7000000, 0xc7000080,
        0x00000000, 0x80000000
    };
    // All22 literal outputs agree across genuine x86/x64 Release/Debug captures.
    // Provenance and exact input/output pairs: fixtures/conversion-manifest.json.
    constexpr SHORT expected[] = {
        -32768, -32768, -32768, -32768, 32767, -32768, -32768, -32768, -32768,
        0, 2, 2, 0, -2, -2,
        32767, 32767, 32767, -32768, -32768, 0, 0
    };
    std::array<float, std::size(inputs)> samples{};
    std::array<SHORT, std::size(inputs)> output{};
    std::memcpy(samples.data(), inputs, sizeof(inputs));
    Require(std::fesetround(FE_TONEAREST) == 0, "conversion round to nearest");
    const NativeState before = ReadNativeState();
    Mixer_FloatToPcm16(samples.data(), output.data(), static_cast<DWORD>(output.size()));
    const NativeState after = ReadNativeState();
#if defined(_M_ARM64) || defined(__aarch64__)
    Require(after.control == before.control, "conversion preserves native FPCR control");
#else
    constexpr std::uint64_t cumulativeExceptionMask = 0x3f;
    Require((after.control & ~cumulativeExceptionMask) == (before.control & ~cumulativeExceptionMask),
        "conversion preserves native SIMD control");
#endif
    if (verify) {
        for (std::size_t count = 0; count <= std::size(inputs); ++count) {
            constexpr SHORT sentinel = 12345;
            std::array<SHORT, std::size(inputs) + 2> bounded{};
            bounded.fill(sentinel);
            Mixer_FloatToPcm16(samples.data(), bounded.data() + 1, static_cast<DWORD>(count));
            Require(bounded.front() == sentinel && bounded[count + 1] == sentinel,
                "conversion does not touch adjacent output");
            for (std::size_t index = 0; index < count; ++index) {
                Require(bounded[index + 1] == expected[index], "qualified Windows conversion edge");
            }
        }
    }
    for (std::size_t index = 0; index < output.size(); ++index) {
        std::printf("CONVERSION %08x %d\n", inputs[index], static_cast<int>(output[index]));
    }
    Require(std::fesetenv(&savedEnvironment) == 0, "restore conversion floating environment");
    WriteNativeState(original);
    const NativeState restored = ReadNativeState();
    Require(restored.control == original.control && restored.status == original.status,
        "conversion fixture restores complete caller native state");
}

}

int main(int argc, char** argv) {
    Require(argc == 2 || argc == 3, "state/probe or PCM reference case");
#if defined(_WIN32)
    InvalidParameterScope invalidParameterScope;
#endif
    if (argc == 3) {
        Require(std::strcmp(argv[1], "pcm") == 0 || std::strcmp(argv[1], "write-pcm") == 0, "known PCM case");
        PcmTest(argv[2], std::strcmp(argv[1], "write-pcm") == 0);
    } else if (std::strcmp(argv[1], "state") == 0) {
        StateTest();
    } else if (std::strcmp(argv[1], "conversion") == 0) {
        ConversionProbe(true);
    } else if (std::strcmp(argv[1], "conversion-probe") == 0) {
        ConversionProbe();
    } else {
        Require(std::strcmp(argv[1], "precision-probe") == 0, "known fixture mode");
        PrecisionProbe();
    }
}
