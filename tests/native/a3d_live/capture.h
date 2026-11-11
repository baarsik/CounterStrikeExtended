#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

using A3dOpen = decltype(&SDL_OpenAudioDeviceStream);
using A3dPut = decltype(&SDL_PutAudioStreamData);
struct A3dCapture {
    static constexpr std::size_t kCapacityBytes = 262144;
    std::atomic<bool> admitting{false};
    std::atomic<SDL_AudioStream*> stream{nullptr};
    std::atomic<std::size_t> bytes{0};
    std::atomic<std::size_t> nonzero{0};
    std::atomic<bool> recursion{false};
    std::atomic<bool> injectFailure{false};
    std::atomic<unsigned> failures{0};
    std::array<unsigned char, kCapacityBytes> pcm{};
    SDL_AudioSpec input{};
    SDL_AudioSpec conversion{};
    SDL_AudioSpec device{};
    SDL_AudioDeviceID logicalDevice = 0;
    std::array<char, 256> name{};
};
static_assert(std::atomic<std::size_t>::is_always_lock_free);
static_assert(std::atomic<SDL_AudioStream*>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<unsigned>::is_always_lock_free);
A3dCapture& A3dLiveCapture();
A3dOpen A3dLiveOriginalOpen();
A3dPut A3dLiveOriginalPut();
A3dPut A3dLiveObservedPut();
