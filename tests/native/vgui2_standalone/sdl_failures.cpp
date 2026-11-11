#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <spawn.h>
#include <cerrno>

namespace {
int failure = 0;
int hiddenParentAttempts = 0;
std::string externalOpen;
std::set<SDL_Window*> windows;
std::set<SDL_Renderer*> renderers;
std::map<SDL_Texture*, SDL_Renderer*> textures;
std::set<SDL_AudioStream*> streams;
int queuedAudioBytes = 0;
int flushedAudioStreams = 0;
bool Fail(int stage) {
    if (failure != stage) {
        return false;
    }
    failure = 0;
    SDL_SetError("controlled real SDL ownership boundary failure");
    return true;
}
SDL_Window* Window(const char* title, int width, int height, SDL_WindowFlags flags) {
    if (Fail(1)) {
        return nullptr;
    }
    auto* result = SDL_CreateWindow(title, width, height, flags);
    if (result) {
        windows.insert(result);
    }
    return result;
}
SDL_Renderer* Renderer(SDL_Window* window, const char* name) {
    if (Fail(2)) {
        return nullptr;
    }
    auto* result = SDL_CreateRenderer(window, name);
    if (result) {
        renderers.insert(result);
    }
    return result;
}
SDL_Texture* Texture(SDL_Renderer* renderer, SDL_PixelFormat format, SDL_TextureAccess access, int width, int height) {
    if (Fail(3)) {
        return nullptr;
    }
    auto* result = SDL_CreateTexture(renderer, format, access, width, height);
    if (result) {
        textures[result] = renderer;
    }
    return result;
}
void DeleteTexture(SDL_Texture* texture) {
    textures.erase(texture);
    SDL_DestroyTexture(texture);
}
void DeleteRenderer(SDL_Renderer* renderer) {
    if (renderer) {
        for (const auto& entry : textures) {
            if (entry.second == renderer) {
                std::fputs("FAIL renderer destroyed before its texture\n", stderr);
                std::abort();
            }
        }
    }
    renderers.erase(renderer);
    SDL_DestroyRenderer(renderer);
}
bool Parent(SDL_Window* window, SDL_Window* parent) {
    if (parent && (SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN)) {
        ++hiddenParentAttempts;
    }
    return SDL_SetWindowParent(window, parent);
}

bool TextInput(SDL_Window* window) {
    if (Fail(5)) {
        return false;
    }
    return SDL_StartTextInput(window);
}

SDL_AudioStream* Audio(SDL_AudioDeviceID device, const SDL_AudioSpec* specification, SDL_AudioStreamCallback callback,
                       void* context) {
    if (Fail(4)) {
        return nullptr;
    }
    auto* result = SDL_OpenAudioDeviceStream(device, specification, callback, context);
    if (result) {
        streams.insert(result);
    }
    return result;
}
void DeleteAudio(SDL_AudioStream* stream) {
    streams.erase(stream);
    SDL_DestroyAudioStream(stream);
}
bool AudioData(SDL_AudioStream* stream, const void* data, int length) {
    const bool result = SDL_PutAudioStreamData(stream, data, length);
    if (result) {
        queuedAudioBytes += length;
    }
    return result;
}
bool AudioFlush(SDL_AudioStream* stream) {
    const bool result = SDL_FlushAudioStream(stream);
    if (result) {
        ++flushedAudioStreams;
    }
    return result;
}
int ExternalOpen(pid_t* process, const char* path, const posix_spawn_file_actions_t* actions,
                 const posix_spawnattr_t* attributes, char* const arguments[], char* const environment[]) {
    if (std::string(path) == "/usr/bin/open") {
        if (!arguments || !arguments[1] || std::string(arguments[1]) != "--" || !arguments[2] || arguments[3]) {
            std::abort();
        }
        externalOpen = arguments[2];
        return ENOSYS; // Record the real OS boundary without opening a shared GUI application.
    }
    return posix_spawn(process, path, actions, attributes, arguments, environment);
}
void DeleteWindow(SDL_Window* window) {
    windows.erase(window);
    SDL_DestroyWindow(window);
}
} // namespace

extern "C" __attribute__((visibility("default"))) void CseTestFailOwnerStage(int stage) {
    failure = stage;
}
extern "C" __attribute__((visibility("default"))) int CseTestLiveOwnerCount() {
    return static_cast<int>(windows.size() + renderers.size() + textures.size());
}
extern "C" __attribute__((visibility("default"))) int CseTestLiveAudioCount() {
    return static_cast<int>(streams.size());
}
extern "C" __attribute__((visibility("default"))) bool CseTestAudioSubmitted() {
    return queuedAudioBytes > 0 && flushedAudioStreams > 0;
}
extern "C" __attribute__((visibility("default"))) const char* CseTestLastExternalOpen() {
    return externalOpen.c_str();
}
extern "C" __attribute__((visibility("default"))) int CseTestHiddenParentAttempts() {
    return hiddenParentAttempts;
}
#define INTERPOSE(replacement, original)                                                                               \
    __attribute__((used, section("__DATA,__interpose"))) static const struct {                                         \
        const void* replace;                                                                                           \
        const void* originalAddress;                                                                                   \
    } interpose_##original = {reinterpret_cast<const void*>(&replacement), reinterpret_cast<const void*>(&original)}
INTERPOSE(Window, SDL_CreateWindow);
INTERPOSE(Renderer, SDL_CreateRenderer);
INTERPOSE(Texture, SDL_CreateTexture);
INTERPOSE(DeleteTexture, SDL_DestroyTexture);
INTERPOSE(DeleteRenderer, SDL_DestroyRenderer);
INTERPOSE(DeleteWindow, SDL_DestroyWindow);

INTERPOSE(Audio, SDL_OpenAudioDeviceStream);
INTERPOSE(DeleteAudio, SDL_DestroyAudioStream);
INTERPOSE(AudioData, SDL_PutAudioStreamData);
INTERPOSE(AudioFlush, SDL_FlushAudioStream);

INTERPOSE(ExternalOpen, posix_spawn);

INTERPOSE(TextInput, SDL_StartTextInput);

INTERPOSE(Parent, SDL_SetWindowParent);
