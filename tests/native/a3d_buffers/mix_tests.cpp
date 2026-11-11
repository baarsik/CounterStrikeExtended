#include "native_buffer.h"
#include "A3dSource.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#define CHECK(value) \
    do { \
        if (!(value)) { \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
            std::exit(1); \
        } \
    } while (0)

struct Owner {
    A3dNativeMixer mixer;
    IDirectSoundBuffer* buffer = nullptr;
    explicit Owner(WORD bits = 16, WORD channels = 1, DWORD frames = 64, bool ignored = false) {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = channels;
        format.nSamplesPerSec = 22050;
        format.wBitsPerSample = bits;
        format.nBlockAlign = bits / 8 * channels;
        format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;
        DSBUFFERDESC desc{};
        desc.dwSize = sizeof(desc);
        desc.dwBufferBytes = frames * format.nBlockAlign;
        desc.lpwfxFormat = &format;
        CHECK(mixer.CreateBuffer(&desc, &buffer) == S_OK);
        void* data = nullptr;
        DWORD bytes = 0;
        CHECK(buffer->Lock(0, desc.dwBufferBytes, &data, &bytes, nullptr, nullptr, 0) == S_OK);
        for (DWORD i = 0; i < bytes; ++i) {
            static_cast<BYTE*>(data)[i] = static_cast<BYTE>(i * 37 + 71);
        }
        CHECK(buffer->Unlock(data, bytes, nullptr, 0) == S_OK);
        IA3dDalBuffer* render = nullptr;
        CHECK(buffer->QueryInterface(IID_A3dNativeRenderBuffer, reinterpret_cast<void**>(&render)) == S_OK);
        A3DCTRL_SRC_SUPER control{};
        control.flPitch = 1;
        control.Left.flGain = 1;
        control.Right.flGain = 0.5f;
        if (ignored) {
            control.flEq = 0.75f;
            control.Reflections[0].dwEnabled = 1;
            control.Reflections[0].flEq = 0.875f;
            control.Reflections[0].Left.flGain = 100;
            control.ReservedBackendData[0] = 31;
        }
        CHECK(render->SetA3dSuperCtrl(&control, 0) == S_OK);
        render->Release();
        std::memset(&control, 0x53, sizeof(control));
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    }
    ~Owner() {
        buffer->Release();
    }
};

std::vector<SHORT> Render(Owner& owner, DWORD frames) {
    std::vector<SHORT> output(frames * 2);
    CHECK(owner.mixer.Render(output.data(), frames) == S_OK);
    return output;
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    const std::string name = argv[1];
    if (name == "chunk_boundaries") {
        for (DWORD frames : {0u, 1u, 2u, 3u, 4096u, 4097u, 8195u}) {
            Owner whole;
            Owner chunks;
            const auto expected = Render(whole, frames);
            std::vector<SHORT> actual;
            DWORD remaining = frames;
            while (remaining) {
                const DWORD count = std::min(remaining, DWORD{4096});
                const auto part = Render(chunks, count);
                actual.insert(actual.end(), part.begin(), part.end());
                remaining -= count;
            }
            CHECK(actual == expected);
        }
    } else if (name == "owned_logical_samples") {
        for (WORD bits : {WORD{8}, WORD{16}}) {
            for (WORD channels : {WORD{1}, WORD{2}}) {
                for (DWORD frames : {1u, 2u, 3u, 64u}) {
                    Owner owner(bits, channels, frames);
                    const auto output = Render(owner, 4096);
                    CHECK(output.size() == 8192);
                    DWORD cursor = UINT32_MAX;
                    CHECK(owner.buffer->GetCurrentPosition(&cursor, nullptr) == S_OK);
                    CHECK(cursor < frames * channels * bits / 8);
                }
            }
        }
    } else if (name == "stock_unread_control") {
        Owner baseline;
        Owner unread(16, 1, 64, true);
        CHECK(Render(baseline, 4096) == Render(unread, 4096));
    } else if (name == "exact_owned_pcm") {
        Owner owner(16, 1, 1);
        void* data = nullptr;
        DWORD bytes = 0;
        CHECK(owner.buffer->Lock(0, 2, &data, &bytes, nullptr, nullptr, 0) == S_OK);
        *static_cast<SHORT*>(data) = 12000;
        CHECK(owner.buffer->Unlock(data, bytes, nullptr, 0) == S_OK);
        // At azimuth/elevation zero, retained table indices39/114 are15065/12744.
        // Stock fixed gains are22596/9557, giving nearest PCM4137/1750.
        CHECK(Render(owner, 1) == std::vector<SHORT>({4137, 1750}));
        CHECK(owner.buffer->SetCurrentPosition(0) == S_OK);
        IDirectSoundBuffer* duplicate = nullptr;
        CHECK(owner.mixer.DuplicateBuffer(owner.buffer, &duplicate) == S_OK);
        CHECK(duplicate->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        // Accumulate both floating contributions before rounding, as DAL_A2D does.
        CHECK(Render(owner, 1) == std::vector<SHORT>({8275, 3500}));
        duplicate->Release();
        Owner byteOwner(8, 1, 1);
        // The stock mono8 boundary consumes owned silence after the logical end.
        CHECK(Render(byteOwner, 3) == std::vector<SHORT>(6, 0));
    } else if (name == "stock_scalar_compatibility") {
        Owner owner;
        Owner expected;
        CHECK(owner.buffer->SetVolume(-9000) == S_OK);
        CHECK(owner.buffer->SetPan(9000) == S_OK);
        CHECK(owner.buffer->SetFrequency(11025) == S_OK);
        IA3dDalBuffer* render = nullptr;
        IResManBuffer* resource = nullptr;
        CHECK(owner.buffer->QueryInterface(IID_A3dNativeBufferControl, reinterpret_cast<void**>(&resource)) == S_OK);
        CHECK(resource->Mute() == S_OK);
        CHECK(resource->SetPriority(7) == S_OK);
        DWORD priority = 0;
        CHECK(resource->GetPriority(&priority) == S_OK && priority == 7);
        // Stock getters store these values. A2D consumes frequency only on the
        // next super-control; volume/pan/mute flags are not read by its mix loop.
        CHECK(Render(owner, 64) == Render(expected, 64));
        CHECK(resource->UnMute() == S_OK);
        resource->Release();
        CHECK(owner.buffer->QueryInterface(IID_A3dNativeRenderBuffer, reinterpret_cast<void**>(&render)) == S_OK);
        A3DCTRL_SRC_SUPER control{};
        control.flPitch = 1;
        control.Left.flGain = 1;
        control.Right.flGain = 0.5f;
        CHECK(render->SetA3dSuperCtrl(&control, 0) == S_OK);
        render->Release();
        CHECK(Render(owner, 64) != Render(expected, 64));
    } else if (name == "unsafe_rate_transition") {
        Owner owner;
        IA3dDalBuffer* render = nullptr;
        CHECK(owner.buffer->QueryInterface(IID_A3dNativeRenderBuffer,
            reinterpret_cast<void**>(&render)) == S_OK);
        A3DCTRL_SRC_SUPER control{};
        control.flPitch = 1;
        control.Left.flGain = 1;
        control.Right.flGain = 0.5f;
        control.Left.flDelay = 0.001f;
        CHECK(render->SetA3dSuperCtrl(&control, 0) == S_OK);
        render->Release();
        // A22-frame delay change in one output frame makes the retained rate
        // negative. Native admission must fail before invoking its debug trap.
        std::array<SHORT, 2> output{123, 456};
        CHECK(owner.mixer.Render(output.data(), 1) == E_INVALIDARG);
        CHECK(output[0] == 123 && output[1] == 456);
        DWORD cursor = UINT32_MAX;
        CHECK(owner.buffer->GetCurrentPosition(&cursor, nullptr) == S_OK && cursor == 0);
        CHECK(owner.mixer.Render(output.data(), 0) == S_OK);
        // A sufficiently sized block retains the original arithmetic unchanged.
        CHECK(Render(owner, 64).size() == 128);
    } else if (name == "consumed_control_pcm") {
        Owner baseline;
        const auto expected = Render(baseline, 64);
        for (int field = 0; field < 5; ++field) {
            Owner changed;
            IA3dDalBuffer* render = nullptr;
            CHECK(changed.buffer->QueryInterface(IID_A3dNativeRenderBuffer,
                reinterpret_cast<void**>(&render)) == S_OK);
            A3DCTRL_SRC_SUPER control{};
            control.flPitch = 1;
            control.Left.flGain = 1;
            control.Right.flGain = 0.5f;
            if (field == 0) {
                control.flPitch = 0.5f;
            } else if (field == 1) {
                control.Left.flGain = 0;
            } else if (field == 2) {
                control.Left.flDelay = 0.00002f;
            } else if (field == 3) {
                control.Left.flAzimuth = 1.0f;
            } else {
                control.Left.flElevation = 0.5f;
            }
            CHECK(render->SetA3dSuperCtrl(&control, 0) == S_OK);
            render->Release();
            const auto actual = Render(changed, 64);
            CHECK(actual != expected);
            if (field == 1) {
                for (std::size_t frame = 0; frame < actual.size() / 2; ++frame) {
                    CHECK(actual[frame * 2] == 0 && actual[frame * 2 + 1] == expected[frame * 2 + 1]);
                }
            }
        }
    } else if (name == "consumed_control_domain") {
        Owner owner;
        IA3dDalBuffer* render = nullptr;
        CHECK(owner.buffer->QueryInterface(IID_A3dNativeRenderBuffer, reinterpret_cast<void**>(&render)) == S_OK);
        A3DCTRL_SRC_SUPER control{};
        control.flPitch = 3;
        control.Left.flGain = 1;
        control.Right.flGain = 0.5f;
        constexpr float halfPi = 1.57079637f;
        control.Left.flElevation = halfPi;
        control.Right.flElevation = -halfPi;
        CHECK(render->SetA3dSuperCtrl(&control, 0) == S_OK);
        const auto output = Render(owner, 64);
        CHECK(std::any_of(output.begin(), output.end(), [](SHORT sample) { return sample != 0; }));
        control.flPitch = 0;
        CHECK(render->SetA3dSuperCtrl(&control, 0) == S_OK);
        render->Release();
    } else if (name == "stock_control_flags") {
        Owner owner;
        IA3dDalBuffer* render = nullptr;
        CHECK(owner.buffer->QueryInterface(IID_A3dNativeRenderBuffer, reinterpret_cast<void**>(&render)) == S_OK);
        A3DCTRL_SRC_SUPER control{};
        control.flPitch = 1;
        control.Left.flGain = 1;
        control.Right.flGain = 0.5f;
        CHECK(render->SetA3dSuperCtrl(&control, UINT32_MAX) == S_OK);
        render->Release();
        Owner expected;
        CHECK(Render(owner, 4096) == Render(expected, 4096));
    } else if (name == "multiple_instance_scratch") {
        Owner first;
        Owner second(8, 2);
        Owner expectedFirst;
        Owner expectedSecond(8, 2);
        const auto expectedA = Render(expectedFirst, 8195);
        const auto expectedB = Render(expectedSecond, 8195);
        std::vector<SHORT> a;
        std::vector<SHORT> b;
        std::thread one([&] { a = Render(first, 8195); });
        std::thread two([&] { b = Render(second, 8195); });
        one.join();
        two.join();
        CHECK(a == expectedA && b == expectedB);
    } else {
        CHECK(false);
    }
    std::printf("PASS %s\n", argv[1]);
}
