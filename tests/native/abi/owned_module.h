#pragma once

#include <string>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Test-only loader for explicit paths to owned rebuilt modules.
class GameModule {
public:
    explicit GameModule(const char *path) {
#if defined(_WIN32)
        handle_ = LoadLibraryA(path);
        if (!handle_) {
            error_ = "Windows error " + std::to_string(GetLastError());
        }
#else
        handle_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!handle_) {
            const char *error = dlerror();
            error_ = error ? error : "Unknown dynamic loader error";
        }
#endif
    }

    ~GameModule() {
        if (handle_) {
#if defined(_WIN32)
            FreeLibrary(handle_);
#else
            dlclose(handle_);
#endif
        }
    }

    explicit operator bool() const {
        return handle_ != nullptr;
    }

    const char *Error() const {
        return error_.c_str();
    }

    template<typename Function>
    Function Find(const char *name) const {
#if defined(_WIN32)
        return reinterpret_cast<Function>(GetProcAddress(handle_, name));
#else
        return reinterpret_cast<Function>(dlsym(handle_, name));
#endif
    }

    GameModule(const GameModule &) = delete;
    GameModule &operator=(const GameModule &) = delete;

private:
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#else
    void *handle_ = nullptr;
#endif
    std::string error_;
};
