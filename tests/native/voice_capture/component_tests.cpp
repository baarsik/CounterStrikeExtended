#include "component_api.h"
#include "fake_sdl.h"
#include "ivoicerecord.h"
#include "allocation_fault.h"

#include <cstdio>
#include <array>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <limits>
#include <atomic>
#include <thread>

namespace {

constexpr int sampleRate = 11025;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "voice capture component: %s\n", message);
        std::exit(1);
    }
}

void RequireNativeAddress(const void* address) {
    if (sizeof(void*) > sizeof(std::uint32_t)) {
        Require(reinterpret_cast<std::uintptr_t>(address) > (std::numeric_limits<std::uint32_t>::max)(),
            "64-bit fixture actually exercises object and caller addresses above4GiB");
    }
}

void NoResources() {
    const auto stats = fake_sdl::Observe();
    Require(stats.activeStreams == 0 && stats.activeCallbacks == 0 && stats.references == 0
            && stats.allocations == stats.frees && stats.invalidCalls == 0 && stats.callbackViolations == 0,
        "actual component releases callbacks, streams, allocation and only its audio reference");
    Require(allocation_fault::CallbackOperations() == 0, "actual component callback performs no C++ heap operations");
}

void Factory() {
    fake_sdl::Reset();
    IVoiceRecord* record = cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::DirectSound);
    Require(record != nullptr, "healthy SDL recorder factory must create a usable owned recorder");
    RequireNativeAddress(record);
    auto stats = fake_sdl::Observe();
    Require(stats.references == 1 && stats.activeStreams == 1 && stats.resumes == 1
            && stats.allocatedBytes == 22050 && stats.requestedFormat == SDL_AUDIO_S16LE
            && stats.requestedChannels == 1 && stats.requestedRate == sampleRate,
        "DSound policy owns one active recording stream and exactly one second of PCM storage");
    Require(record->RecordStart(), "healthy recorder can start");
    record->Release();
    NoResources();
    stats = fake_sdl::Observe();
    Require(stats.detaches == 1 && stats.destroys == 1 && stats.quitCalls == 1,
        "Release detaches and destroys before releasing its acquired subsystem reference");

    fake_sdl::Reset();
    fake_sdl::HoldExternalAudioReference();
    record = cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::WaveIn);
    Require(record != nullptr && fake_sdl::Observe().references == 2
            && fake_sdl::Observe().allocatedBytes == 3840,
        "WaveIn policy acquires its own reference even when another audio owner exists");
    record->Release();
    Require(fake_sdl::Observe().references == 1, "component preserves another owner's audio reference");
    fake_sdl::ReleaseExternalAudioReference();
    NoResources();
}

std::vector<short> Samples(unsigned int first, unsigned int count) {
    std::vector<short> result(count);
    for (unsigned int index = 0; index < count; ++index) {
        result[index] = static_cast<short>((first + index) & 0x7FFFu);
    }
    return result;
}

void PushSamples(SDL_AudioStream* stream, unsigned int first, unsigned int count) {
    const auto input = Samples(first, count);
    // Deliberately non-authoritative callback hints; availability/data determine PCM.
    fake_sdl::Push(stream, input.data(), input.size() * sizeof(short), -7, INT_MAX);
}

void CheckSamples(const short* output, unsigned int count, unsigned int first) {
    for (unsigned int index = 0; index < count; ++index) {
        Require(output[index] == static_cast<short>((first + index) & 0x7FFFu),
            "actual capture output retains exact known PCM ordering and values");
    }
}

void DirectSoundData() {
    fake_sdl::Reset();
    IVoiceRecord* record = cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::DirectSound);
    Require(record != nullptr, "DSound data recorder creates");
    SDL_AudioStream* stream = fake_sdl::LastStream();
    PushSamples(stream, 0, 1920);
    std::array<short, 4096> output;
    RequireNativeAddress(output.data());
    output.fill(-1);
    Require(record->GetRecordedData(output.data(), 1024) == 512,
        "DSound profile retains the measured byte-limited512sample window");
    CheckSamples(output.data(), 512, 1408);
    Require(output[512] == -1 && record->GetRecordedData(output.data(), 1024) == 0,
        "DSound consumes the window once and stays within its returned output span");
    record->RecordStop();
    PushSamples(stream, 2000, 64);
    Require(record->GetRecordedData(output.data(), 128) == 64 && output[0] == 2000,
        "DSound stop leaves capture active and readable");
    Require(record->RecordStart() && fake_sdl::Observe().resumes == 1 && fake_sdl::Observe().pauses == 0,
        "DSound start/stop do not restart or pause its initialized stream");
    PushSamples(stream, 3000, 64);
    Require(record->GetRecordedData(nullptr, 0) == 0 && record->GetRecordedData(output.data(), 128) == 0,
        "DSound zero capacity consumes pending bytes without dereferencing output");
    record->Release();
    NoResources();
}

void WaveInData() {
    fake_sdl::Reset();
    IVoiceRecord* record = cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::WaveIn);
    Require(record != nullptr, "WaveIn data recorder creates");
    SDL_AudioStream* stream = fake_sdl::LastStream();
    std::array<short, 8192> output;
    RequireNativeAddress(output.data());
    output.fill(-1);
    PushSamples(stream, 0, 64);
    Require(record->GetRecordedData(output.data(), 64) == 0, "WaveIn gate drops converted data before start");
    Require(record->RecordStart(), "WaveIn starts its delivery gate");
    PushSamples(stream, 0, 1920);
    Require(record->GetRecordedData(output.data(), 1024) == 1024,
        "WaveIn profile retains the measured1024sample newest window");
    CheckSamples(output.data(), 1024, 896);
    Require(output[1024] == -1 && output.back() == -1, "WaveIn newest-window copy preserves output canaries");
    PushSamples(stream, 2000, 64);
    record->RecordStop();
    Require(record->GetRecordedData(output.data(), 64) == 64 && output[0] == 2000,
        "WaveIn stop permits the previously buffered pending read");
    PushSamples(stream, 3000, 64);
    record->Idle();
    Require(record->RecordStart() && record->GetRecordedData(output.data(), 64) == 0,
        "stopped WaveIn callbacks discard and stopped Idle flushes");
    PushSamples(stream, 4000, 64);
    record->RecordStop();
    Require(record->RecordStart() && record->GetRecordedData(output.data(), 64) == 64 && output[0] == 4000,
        "WaveIn restart before stopped Idle preserves queued PCM");
    PushSamples(stream, 0, 2048);
    Require(record->GetRecordedData(output.data(), 8192) == 1920,
        "WaveIn bounded overflow retains exactly its3840byte storage");
    CheckSamples(output.data(), 1920, 128);
    PushSamples(stream, 5000, 64);
    Require(record->GetRecordedData(output.data(), -1) == 0 && record->GetRecordedData(nullptr, 64) == 0
            && record->GetRecordedData(output.data(), INT_MAX) == 0,
        "invalid WaveIn arguments cannot consume or overflow pending data");
    Require(record->GetRecordedData(output.data(), 64) == 64 && output[0] == 5000,
        "valid read still sees PCM after rejected argument bounds");
    PushSamples(stream, 6000, 64);
    Require(record->GetRecordedData(nullptr, 0) == 0 && record->GetRecordedData(output.data(), 64) == 0,
        "WaveIn zero capacity discards all pending data without output");
    record->Release();
    NoResources();
}

void DirectSoundOddAndCursor() {
    fake_sdl::Reset();
    IVoiceRecord* record = cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::DirectSound);
    Require(record != nullptr, "DSound odd-window recorder creates");
    SDL_AudioStream* stream = fake_sdl::LastStream();
    std::array<short, 4> output;
    std::memset(output.data(), 0x7E, sizeof(output));
    PushSamples(stream, 0, 3);
    Require(record->GetRecordedData(output.data(), 3) == 1, "odd DS limit returns floor(bytes/2)");
    const auto* bytes = reinterpret_cast<const unsigned char*>(output.data());
    Require(bytes[0] == 0 && bytes[1] == 2 && bytes[2] == 0 && bytes[3] == 0x7E,
        "nonwrapped odd DS copy retains its three-byte effect");
    PushSamples(stream, 3, 11023);
    std::memset(output.data(), 0x7E, sizeof(output));
    Require(record->GetRecordedData(output.data(), 3) == 1 && output[0] == 11025 && bytes[2] == 0x7E,
        "wrapped odd DS copy retains its short-offset overwrite effect");
    record->Release();
    NoResources();

    fake_sdl::Reset();
    record = cse::CreateVoiceRecordSDL(8, cse::VoiceCapturePolicy::DirectSound);
    Require(record != nullptr, "small reference cursor recorder creates");
    stream = fake_sdl::LastStream();
    PushSamples(stream, 0, 7);
    Require(record->GetRecordedData(output.data(), 8) == 4 && output[0] == 3,
        "DS cursor consumes initial latest data");
    PushSamples(stream, 7, 17);
    Require(record->GetRecordedData(output.data(), 8) == 1 && output[0] == 23,
        "multiple callback wraps coalesce until owner Idle just like the legacy event");
    record->Release();
    NoResources();

    fake_sdl::Reset();
    record = cse::CreateVoiceRecordSDL(8, cse::VoiceCapturePolicy::DirectSound);
    stream = fake_sdl::LastStream();
    PushSamples(stream, 0, 8);
    record->Idle();
    PushSamples(stream, 8, 8);
    record->Idle();
    std::array<short, 32> guarded;
    guarded.fill(-1);
    Require(record->GetRecordedData(guarded.data(), 32) == 0 && guarded[0] == -1 && guarded.back() == -1,
        "invalid DS cursor range cannot read beyond the ring or mutate destination");
    Require(record->GetRecordedData(guarded.data(), 8) == 4 && guarded[0] == 12,
        "rejected DS range cannot commit the last-read cursor");
    record->Release();
    NoResources();
}

void FactoryFailures() {
    fake_sdl::Reset();
    allocation_fault::FailNext();
    Require(cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::WaveIn) == nullptr
            && fake_sdl::Observe().initCalls == 0 && fake_sdl::Observe().allocations == 0,
        "object allocation failure returns null before touching SDL or queue storage");
    NoResources();
    const fake_sdl::Failure failures[] = {fake_sdl::Failure::Allocation, fake_sdl::Failure::Init,
        fake_sdl::Failure::Open, fake_sdl::Failure::FormatQuery, fake_sdl::Failure::WrongFormat,
        fake_sdl::Failure::WrongChannels, fake_sdl::Failure::WrongRate, fake_sdl::Failure::Resume};
    for (auto failure : failures) {
        fake_sdl::Reset();
        fake_sdl::Fail(failure);
        Require(cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::DirectSound) == nullptr,
            "each failed acquisition/format/resume must reject the real recorder factory");
        NoResources();
    }
    fake_sdl::Reset();
    Require(cse::CreateVoiceRecordSDL(0, cse::VoiceCapturePolicy::WaveIn) == nullptr
            && cse::CreateVoiceRecordSDL(-1, cse::VoiceCapturePolicy::DirectSound) == nullptr
            && cse::CreateVoiceRecordSDL(sampleRate, static_cast<cse::VoiceCapturePolicy>(7)) == nullptr,
        "invalid pure factory arguments fail before acquiring a device or subsystem");
    Require(fake_sdl::Observe().initCalls == 0 && fake_sdl::Observe().allocations == 0,
        "invalid factory parameters have no external effects");
    Require(cse::CreateVoiceRecordSDL(INT_MAX, cse::VoiceCapturePolicy::DirectSound) == nullptr
            && fake_sdl::Observe().allocatedBytes == 4294967294ull,
        "largest positive int retains its checked unsigned32 byte extent without a huge fixture allocation");
    NoResources();
    IVoiceRecord* record = cse::CreateVoiceRecordSDL(INT_MAX, cse::VoiceCapturePolicy::WaveIn);
    Require(record != nullptr && fake_sdl::Observe().allocatedBytes == 3840,
        "WaveIn fixed storage does not invent a guessed sample-rate cap");
    record->Release();
    NoResources();
}

void TerminalFailures() {
    const fake_sdl::Failure failures[] = {fake_sdl::Failure::Available, fake_sdl::Failure::Read,
        fake_sdl::Failure::ReadOdd, fake_sdl::Failure::ReadOversized};
    const cse::VoiceCapturePolicy policies[] = {cse::VoiceCapturePolicy::DirectSound, cse::VoiceCapturePolicy::WaveIn};
    for (auto policy : policies) {
        for (auto failure : failures) {
            fake_sdl::Reset();
            IVoiceRecord* record = cse::CreateVoiceRecordSDL(sampleRate, policy);
            Require(record != nullptr && record->RecordStart(), "runtime-failure recorder starts");
            SDL_AudioStream* stream = fake_sdl::LastStream();
            PushSamples(stream, 0, 8);
            fake_sdl::Fail(failure);
            PushSamples(stream, 100, 8);
            Require(fake_sdl::QueuedBytes(stream) == 0, "terminal callback failure discards queued SDK data");
            for (unsigned int index = 0; index < 8; ++index) {
                PushSamples(stream, 200, 8);
                Require(fake_sdl::QueuedBytes(stream) == 0,
                    "later callbacks cannot accumulate queued audio while owner Idle is delayed");
            }
            Require(fake_sdl::Observe().diagnostics == 0 && fake_sdl::Observe().callbackViolations == 0,
                "failure callback only latches/clears and never logs or tears down the device");
            std::array<short, 64> output;
            output.fill(-1);
            Require(record->GetRecordedData(output.data(), 64) == 0 && output[0] == -1 && output.back() == -1,
                "terminal capture exposes neither old data nor malformed/partial converted frames");
            Require(!record->RecordStart(), "terminally failed capture cannot report a successful restart");
            record->Idle();
            record->RecordStop();
            record->Idle();
            Require(fake_sdl::Observe().diagnostics == 1 && fake_sdl::Observe().destroys == 1,
                "owner reports and closes terminal capture exactly once");
            record->Release();
            NoResources();
        }
    }
}

void PartialFramesAndSnapshot() {
    fake_sdl::Reset();
    IVoiceRecord* record = cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::DirectSound);
    Require(record != nullptr, "partial-frame recorder creates");
    SDL_AudioStream* stream = fake_sdl::LastStream();
    const unsigned char first = 0x34;
    const unsigned char second = 0x12;
    short output[8] = {};
    fake_sdl::Push(stream, &first, 1, 1, 1);
    Require(record->GetRecordedData(output, 2) == 0 && fake_sdl::Observe().readCalls == 0,
        "one converted byte cannot fabricate a mono16 sample");
    fake_sdl::Push(stream, &second, 1, 1, 1);
    Require(record->GetRecordedData(output, 2) == 1 && output[0] == 0x1234,
        "a completed available frame retains both bytes");
    fake_sdl::Fail(fake_sdl::Failure::ReadZero);
    PushSamples(stream, 0, 2);
    Require(record->GetRecordedData(output, 8) == 0, "zero progress ends the callback without false terminal failure");
    PushSamples(stream, 2, 2);
    Require(record->GetRecordedData(output, 8) == 4, "a later callback resumes the still-valid queued conversion");
    CheckSamples(output, 4, 0);
    fake_sdl::NextAvailable(2);
    PushSamples(stream, 100, 2);
    Require(record->GetRecordedData(output, 4) == 1 && output[0] == 100,
        "a callback drains its finite availability snapshot rather than chasing future output");
    const unsigned char unused = 0;
    fake_sdl::Push(stream, &unused, 0);
    Require(record->GetRecordedData(output, 4) == 1 && output[0] == 101,
        "remaining conversion data is retained for the next callback");
    record->Release();
    NoResources();

    const cse::VoiceCapturePolicy policies[] = {cse::VoiceCapturePolicy::DirectSound, cse::VoiceCapturePolicy::WaveIn};
    for (auto policy : policies) {
        fake_sdl::Reset();
        record = cse::CreateVoiceRecordSDL(sampleRate, policy);
        Require(record != nullptr && record->RecordStart(), "signed-range recorder starts");
        stream = fake_sdl::LastStream();
        const short signedInput[] = {-32768, -1, 0, 32767};
        fake_sdl::Push(stream, signedInput, sizeof(signedInput));
        const int capacity = policy == cse::VoiceCapturePolicy::DirectSound ? 8 : 4;
        Require(record->GetRecordedData(output, capacity) == 4
                && std::memcmp(output, signedInput, sizeof(signedInput)) == 0,
            "capture preserves every signed16 boundary without gain, offset or clipping");
        record->Release();
        NoResources();
    }
}

void InFlightRelease() {
    const cse::VoiceCapturePolicy policies[] = {cse::VoiceCapturePolicy::DirectSound, cse::VoiceCapturePolicy::WaveIn};
    for (auto policy : policies) {
        for (bool pauseFails : {false, true}) {
            fake_sdl::Reset();
            IVoiceRecord* record = cse::CreateVoiceRecordSDL(sampleRate, policy);
            Require(record != nullptr && record->RecordStart(), "in-flight recorder starts");
            SDL_AudioStream* stream = fake_sdl::LastStream();
            const auto input = Samples(0, 64);
            fake_sdl::BlockNextRead();
            fake_sdl::PushFromWorker(stream, input.data(), input.size() * sizeof(short));
            Require(fake_sdl::WaitForBlockedRead(1000), "real capture callback enters a blocked SDK read");
            if (pauseFails) {
                fake_sdl::Fail(fake_sdl::Failure::Pause);
            }
            std::atomic<bool> released{false};
            std::thread owner([record, &released] {
                record->Release();
                released = true;
            });
            Require(fake_sdl::WaitForPauseAttempt(1000) && !released && fake_sdl::CallbackActive()
                    && fake_sdl::Observe().frees == 0,
                "Release reaches the shutdown boundary and waits without freeing active callback userdata");
            fake_sdl::UnblockRead();
            owner.join();
            Require(released && !fake_sdl::CallbackActive(), "Release completes after the callback leaves");
            NoResources();
        }
    }
}

void MultipleOwnersAndRestarts() {
    fake_sdl::Reset();
    fake_sdl::HoldExternalAudioReference();
    IVoiceRecord* first = cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::DirectSound);
    SDL_AudioStream* firstStream = fake_sdl::LastStream();
    IVoiceRecord* second = cse::CreateVoiceRecordSDL(sampleRate, cse::VoiceCapturePolicy::WaveIn);
    SDL_AudioStream* secondStream = fake_sdl::LastStream();
    Require(first != nullptr && second != nullptr && fake_sdl::Observe().references == 3,
        "independent recorders acquire independent SDL references");
    PushSamples(firstStream, 0, 64);
    first->Release();
    Require(fake_sdl::Observe().references == 2 && fake_sdl::Observe().activeStreams == 1,
        "closing one recorder preserves the other recorder and an external audio owner");
    short output[64] = {};
    for (unsigned int index = 0; index < 64; ++index) {
        Require(second->RecordStart(), "WaveIn can repeatedly start after a stopped Idle");
        PushSamples(secondStream, index, 1);
        Require(second->GetRecordedData(output, 1) == 1 && output[0] == static_cast<short>(index),
            "repeated capture cycles keep exact PCM rather than stale data");
        second->RecordStop();
        second->Idle();
        second->RecordStop();
        second->Idle();
    }
    second->Release();
    Require(fake_sdl::Observe().references == 1, "all recorder cycles preserve the external owner's reference");
    fake_sdl::ReleaseExternalAudioReference();
    NoResources();
}

}

int main(int argc, char** argv) {
    if (argc == 1 || std::strcmp(argv[1], "factory") == 0) {
        Factory();
    } else if (std::strcmp(argv[1], "data") == 0) {
        DirectSoundData();
        WaveInData();
        DirectSoundOddAndCursor();
    } else if (std::strcmp(argv[1], "failures") == 0) {
        FactoryFailures();
        TerminalFailures();
    } else if (std::strcmp(argv[1], "frames") == 0) {
        PartialFramesAndSnapshot();
    } else if (std::strcmp(argv[1], "lifetime") == 0) {
        InFlightRelease();
        MultipleOwnersAndRestarts();
    } else {
        return 2;
    }
    std::puts("voice capture component passed");
    return 0;
}
