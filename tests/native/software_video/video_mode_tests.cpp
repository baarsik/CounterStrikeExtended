// Actual IVideoMode implementation, controlled engine services and real SDL.
#include "../../../src/engine/sys_getmodes.cpp"
#include <cassert>
#include <stdexcept>
viddef_t vid{};
int com_argc;
char **com_argv;
int COM_CheckParm(const char *) {
    return 0;
}
FileHandle_t FS_Open(const char *, const char *) {
    return nullptr;
}
unsigned int FS_Size(FileHandle_t) {
    return 0;
}
int FS_Read(void *, int, int, FileHandle_t) {
    return 0;
}
void FS_Close(FileHandle_t) {}
char *FS_ParseFile(char *, char *, int *) {
    return nullptr;
}
qboolean LoadTGA(char *, byte *, int, int *, int *) {
    return 0;
}
void Sys_Error(const char *, ...) {
    throw std::runtime_error("engine error");
}
extern "C" void VID_UpdateWindowVars(RECT *, int, int) {}
extern "C" void VOX_Init() {}
extern "C" void Snd_AcquireBuffer() {}
class Registry : public IRegistry {
public:
    int width = 3;
    int height = 2;
    void Init() override {}
    void Shutdown() override {}
    int ReadInt(const char *key, int value) override {
        if (!std::strcmp(key, "DisplayWidth")) {
            return width;
        }
        if (!std::strcmp(key, "DisplayHeight")) {
            return height;
        }
        return value;
    }
    void WriteInt(const char *, int) override {}
    const char *ReadString(const char *, const char *value) override {
        return value;
    }
    void WriteString(const char *, const char *) override {}
};
class Game : public IGame {
public:
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    bool failWindow = false;
    bool failRenderer = false;
    bool Init(void *) override {
        return true;
    }
    bool Shutdown() override {
        // The real CGame seam is validated by the actual image lifecycle harness.
        if (videomode) {
            videomode->ReleaseVideo();
        }
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
        SDL_DestroyWindow(window);
        window = nullptr;
        return true;
    }
    bool CreateGameWindow() override {
        if (failWindow) {
            return false;
        }
        window = SDL_CreateWindow("owner", 3, 2, SDL_WINDOW_HIDDEN);
        if (!failRenderer) {
            renderer = SDL_CreateRenderer(window, "software");
        }
        return window != nullptr;
    }
    void SleepUntilInput(int) override {}
    CseGameWindowHandle GetMainWindow() override {
        return window;
    }
    CseGameWindowHandle *GetMainWindowAddress() override {
        return &window;
    }
    void SetWindowXY(int, int) override {}
    void SetWindowSize(int w, int h) override {
        SDL_SetWindowSize(window, w, h);
    }
    void GetWindowRect(int *x, int *y, int *w, int *h) override {
        *x = 0;
        *y = 0;
        *w = 3;
        *h = 2;
    }
    bool IsActiveApp() override {
        return true;
    }
    bool IsMultiplayer() override {
        return false;
    }
    void PlayStartupVideos() override {}
    void PlayAVIAndWait(const char *) override {}
    void SetCursorVisible(bool) override {}
};
Registry settings;
IRegistry *registry = &settings;
Game controlledGame;
IGame *game = &controlledGame;
int main() {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO));
    for (int failure : {1, 2, 0}) {
        controlledGame.failWindow = failure == 1;
        controlledGame.failRenderer = failure == 2;
        videomode = new CVideoMode_NativeSoftware(true);
        assert(videomode->Init(nullptr) == (failure == 0));
        if (failure == 0) {
            assert(vid.buffer && vid.buffer == vid.direct && vid.buffer == vid.conbuffer);
            assert(vid.bits == 16 && vid.rowbytes == 6 && vid.conrowbytes == 6);
            assert(vid.vidtype == VT_Software && vid.is15bit == 0);
            auto *pixels = vid.buffer;
            videomode->ReleaseVideo();
            videomode->ReleaseVideo();
            videomode->RestoreVideo();
            assert(vid.buffer == pixels);
            assert(videomode->GetInitialized());
        }
        controlledGame.Shutdown();
        videomode->Shutdown();
        assert(!videomode && !vid.buffer);
    }

    // A window can use a framebuffer size absent from SDL's fullscreen modes.
    settings.width = 1376;
    settings.height = 824;
    videomode = new CVideoMode_NativeSoftware(true);
    assert(videomode->Init(nullptr));
    vmode_s* modes = nullptr;
    int count = 0;
    VideoMode_GetVideoModes(&modes, &count);
    int activeModes = 0;
    for (int index = 0; index < count; ++index) {
        if (modes[index].width == settings.width && modes[index].height == settings.height) {
            ++activeModes;
        }
    }
    assert(activeModes == 1);
    controlledGame.Shutdown();
    videomode->Shutdown();
    SDL_Quit();
}
