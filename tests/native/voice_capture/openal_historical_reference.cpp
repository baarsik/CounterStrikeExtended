// Historical source candidate only. All OpenAL symbols below are fixture-owned;
// no OpenAL library, device, or audio subsystem is loaded.
#include <cstdio>
#include <cstdlib>
#include <deque>

struct ALCdevice {
    std::deque<short> samples;
    bool running = false;
};

namespace {

ALCdevice device;
unsigned int opens = 0;
unsigned int closes = 0;
unsigned int nullCloses = 0;
unsigned int starts = 0;
unsigned int stops = 0;
unsigned int recordedRate = 0;
int recordedCapacity = 0;
bool failOpen = false;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "historical OpenAL reference: %s\n", message);
        std::exit(1);
    }
}

}

extern "C" ALCdevice* alcCaptureOpenDevice(const char* name, unsigned int frequency, int format, int capacity) {
    constexpr int mono16 = 0x1101;
    Require(name == nullptr && format == mono16, "actual old default mono16 capture request");
    ++opens;
    recordedRate = frequency;
    recordedCapacity = capacity;
    return failOpen ? nullptr : &device;
}

extern "C" int alcCaptureCloseDevice(ALCdevice* input) {
    if (input == nullptr) {
        ++nullCloses;
        return 0;
    }
    Require(input == &device, "old source closes its owned fake device");
    ++closes;
    input->running = false;
    input->samples.clear();
    return 1;
}

extern "C" void alcCaptureStart(ALCdevice* input) {
    Require(input == &device, "old source starts its opened fake device");
    ++starts;
    input->running = true;
}

extern "C" void alcCaptureStop(ALCdevice* input) {
    Require(input == &device, "old source stops its opened fake device");
    ++stops;
    input->running = false;
}

extern "C" void alcCaptureSamples(ALCdevice* input, void* output, int count) {
    Require(input == &device && count >= 0 && static_cast<std::size_t>(count) <= input->samples.size(),
        "historical source requests available sample units");
    auto* samples = static_cast<short*>(output);
    for (int index = 0; index < count; ++index) {
        samples[index] = input->samples.front();
        input->samples.pop_front();
    }
}

extern "C" void alcGetIntegerv(ALCdevice* input, int parameter, int count, int* output) {
    constexpr int captureSamples = 0x312;
    Require(input == &device && parameter == captureSamples && count == 1, "historical queue-size query");
    *output = static_cast<int>(input->samples.size());
}

extern "C" int alGetError() {
    return 0;
}

#include CSE_VOICE_OPENAL_SOURCE

int main() {
    constexpr unsigned int sampleRate = 11025;
    constexpr int callerCapacity = 1024;
    IVoiceRecord* record = CreateVoiceRecord(sampleRate);
    Require(record != nullptr && opens == 0 && nullCloses == 1,
        "historical factory returns before opening and calls close on null during initialization");
    Require(record->RecordStart() && opens == 1 && starts == 1
            && recordedRate == sampleRate && recordedCapacity == static_cast<int>(sampleRate) * 20,
        "historical OpenAL lazily opens a20second device queue at first start");
    constexpr int inputSamples = 1920;
    for (int index = 0; index < inputSamples; ++index) {
        device.samples.push_back(static_cast<short>(index));
    }
    short output[callerCapacity] = {};
    const int count = record->GetRecordedData(output, callerCapacity);
    Require(count == callerCapacity && output[0] == 0 && output[count - 1] == callerCapacity - 1,
        "historical OpenAL reads oldest samples rather than the Windows latest-window policy");
    record->RecordStop();
    Require(stops == 1 && closes == 1, "historical stop closes and clears its device");
    failOpen = true;
    Require(!record->RecordStart(), "historical lazy-open failure is reported by RecordStart");
    record->Release();
    Require(nullCloses == 2, "historical destructor closes null after stop/open failure");
    std::printf("historical_openal,capacity%d,caller%d,returned%d,first%d,last%d,opens%u,closes%u,null_closes%u\n",
        recordedCapacity, callerCapacity, count, output[0], output[count - 1], opens, closes, nullCloses);
    return 0;
}
