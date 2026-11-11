#include "native_output.h"
#include "native_mix.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); std::exit(1); } } while (0)
extern std::atomic<int> fixtureRenderCount;
extern std::atomic<bool> fixtureBlockRender;
extern std::atomic<bool> fixtureRenderEntered;
extern std::atomic<bool> fixtureFailRender;
static bool failOpen = false;
static bool failResume = false;
static bool failThread = false;
static bool failPut = false;
static std::atomic<bool> blockPut{false};
static std::atomic<bool> enteredPut{false};
static std::atomic<int> destroyed{0};
static int initialized = 0;
static int quit = 0;
static SDL_AudioStream* stream = nullptr;
static SDL_AudioDeviceID selected = 0;
static SDL_AudioDeviceID liveDevice = 42;
static bool Init(SDL_InitFlags flags) {
    ++initialized;
    return SDL_InitSubSystem(flags);
}
static void Quit(SDL_InitFlags flags) {
    ++quit;
    SDL_QuitSubSystem(flags);
}
static SDL_AudioDeviceID* Devices(int* count) {
    *count = 1;
    auto* devices = static_cast<SDL_AudioDeviceID*>(SDL_malloc(2 * sizeof(SDL_AudioDeviceID)));
    CHECK(devices);
    devices[0] = liveDevice;
    devices[1] = 0;
    return devices;
}
static SDL_AudioStream* Open(SDL_AudioDeviceID device, const SDL_AudioSpec* spec,
        SDL_AudioStreamCallback callback, void* context) {
    selected = device;
    CHECK(spec->freq == 22050 && spec->channels == 2 && spec->format == SDL_AUDIO_S16);
    if (failOpen) {
        return nullptr;
    }
    // Real SDL conversion stream, intentionally unbound to any audio device.
    stream = SDL_CreateAudioStream(spec, spec);
    CHECK(stream && SDL_SetAudioStreamGetCallback(stream, callback, context));
    return stream;
}
static void Destroy(SDL_AudioStream* value) {
    SDL_DestroyAudioStream(value);
    ++destroyed;
}
static bool Resume(SDL_AudioStream*) {
    return !failResume;
}
static bool Put(SDL_AudioStream* value, const void* data, int length) {
    enteredPut = true;
    while (blockPut) {
        SDL_Delay(1);
    }
    return !failPut && SDL_PutAudioStreamData(value, data, length);
}
static SDL_Thread* Thread(SDL_ThreadFunction function, const char* name, void* context) {
    return failThread ? nullptr : SDL_CreateThread(function, name, context);
}
static void Until(const std::atomic<bool>& condition) {
    const Uint64 end = SDL_GetTicks() + 2000;
    while (!condition && SDL_GetTicks() < end) {
        SDL_Delay(1);
    }
    CHECK(condition);
}
static void Prepared() {
    const Uint64 end = SDL_GetTicks() + 2000;
    while (fixtureRenderCount == 0 && SDL_GetTicks() < end) {
        SDL_Delay(1);
    }
    CHECK(fixtureRenderCount > 0);
    SDL_Delay(10);
}
int main(int argc, char** argv) {
    CHECK(argc == 2);
    const char* test = argv[1];
    A3dNativeMixer mixer;
    if (!std::strcmp(test, "manual_device") || !std::strcmp(test, "dummy_stream") ||
        !std::strcmp(test, "enumerated_device")) {
        A3dNativeOutput output;
        GUID guid{};
        const GUID* device = nullptr;
        if (!std::strcmp(test, "enumerated_device")) {
            CHECK(output.DeviceGuid(0, &guid) == S_OK);
            device = &guid;
        }
        CHECK(output.Open(device, mixer) == S_OK);
        CHECK(output.Start() == S_OK);
        SDL_Delay(100);
        CHECK(output.Status() == S_OK);
        output.Close();
        return 0;
    }
    A3dNativeOutputOperations operations = A3dNativeOutputOperations::Default();
    operations.init = Init;
    operations.quit = Quit;
    operations.devices = Devices;
    operations.open = Open;
    operations.destroy = Destroy;
    operations.resume = Resume;
    operations.put = Put;
    operations.thread = Thread;
    A3dNativeOutput output(operations);
    if (!std::strcmp(test, "partial_open_unwinds")) {
        failOpen = true;
        CHECK(FAILED(output.Open(nullptr, mixer)));
        CHECK(initialized == 1 && quit == 1 && destroyed == 0);
        failOpen = false;
        failThread = true;
        CHECK(FAILED(output.Open(nullptr, mixer)));
        CHECK(initialized == 2 && quit == 2 && destroyed == 1);
        failThread = false;
        CHECK(output.Open(nullptr, mixer) == S_OK);
        output.Close();
        CHECK(quit == 3 && destroyed == 2);
    } else if (!std::strcmp(test, "unknown_device")) {
        GUID unknown{};
        CHECK(FAILED(output.Open(&unknown, mixer)));
        CHECK(!stream && initialized == quit);
        GUID guid{};
        CHECK(output.DeviceGuid(0, &guid) == S_OK);
        liveDevice = 43;
        CHECK(FAILED(output.Open(&guid, mixer)));
        CHECK(!stream && initialized == quit);
    } else if (!std::strcmp(test, "copied_guid")) {
        GUID guid{};
        CHECK(output.DeviceGuid(0, &guid) == S_OK);
        CHECK(output.Open(&guid, mixer) == S_OK);
        std::memset(&guid, 0, sizeof(guid));
        CHECK(output.Start() == S_OK && selected == 42);
        output.Close();
    } else if (!std::strcmp(test, "other_sdl_user_survives")) {
        CHECK(SDL_InitSubSystem(SDL_INIT_AUDIO));
        CHECK(output.Open(nullptr, mixer) == S_OK);
        output.Close();
        CHECK(SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    } else if (!std::strcmp(test, "producer_quiescence")) {
        fixtureBlockRender = true;
        CHECK(output.Open(nullptr, mixer) == S_OK);
        Until(fixtureRenderEntered);
        std::atomic<bool> closed{false};
        std::thread closer([&] { output.Close(); closed = true; });
        SDL_Delay(20);
        CHECK(!closed && destroyed == 0);
        fixtureBlockRender = false;
        closer.join();
        CHECK(closed && destroyed == 1);
    } else if (!std::strcmp(test, "callback_quiescence")) {
        CHECK(output.Open(nullptr, mixer) == S_OK);
        Prepared();
        blockPut = true;
        std::thread callback([] { SHORT data[512]; SDL_GetAudioStreamData(stream, data, sizeof(data)); });
        Until(enteredPut);
        std::atomic<bool> closed{false};
        std::thread closer([&] { output.Close(); closed = true; });
        SDL_Delay(20);
        CHECK(!closed && destroyed == 0);
        blockPut = false;
        callback.join();
        closer.join();
        CHECK(closed && destroyed == 1);
    } else if (!std::strcmp(test, "bounded_pcm")) {
        CHECK(output.Open(nullptr, mixer) == S_OK);
        Prepared();
        const int before = fixtureRenderCount;
        SDL_Delay(30);
        CHECK(fixtureRenderCount == before); // paused transport cannot grow without bound
        SHORT data[512]{};
        CHECK(SDL_GetAudioStreamData(stream, data, sizeof(data)) == sizeof(data));
        CHECK(data[0] == 1234 && data[1] == -2345 && data[511] == -2345);
        output.Close();
    } else if (!std::strcmp(test, "put_failure")) {
        CHECK(output.Open(nullptr, mixer) == S_OK);
        Prepared();
        failPut = true;
        SHORT data[512];
        SDL_GetAudioStreamData(stream, data, sizeof(data));
        CHECK(FAILED(output.Status()));
        output.Close();
    } else if (!std::strcmp(test, "render_failure")) {
        fixtureFailRender = true;
        CHECK(output.Open(nullptr, mixer) == S_OK);
        Until(fixtureRenderEntered);
        SDL_Delay(10);
        CHECK(output.Status() == E_INVALIDARG);
        CHECK(output.Start() == E_INVALIDARG);
        output.Close();
    } else {
        CHECK(output.Open(nullptr, mixer) == S_OK);
        if (!std::strcmp(test, "resume_failure")) {
            failResume = true;
            CHECK(FAILED(output.Start()));
            CHECK(destroyed == 1 && quit == 1);
        } else {
            CHECK(!std::strcmp(test, "double_close"));
            CHECK(output.Start() == S_OK);
        }
        output.Close();
        output.Close();
        CHECK(destroyed == 1 && quit == 1);
    }
}
