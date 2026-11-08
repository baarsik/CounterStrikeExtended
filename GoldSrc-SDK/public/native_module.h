#ifndef CSE_NATIVE_MODULE_H
#define CSE_NATIVE_MODULE_H

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace cse_platform {
constexpr char InterfaceFactorySymbol[] = "CreateInterface";
enum class ModuleFallback {
    None,
    NativeExtension
};

enum class ModuleNullPolicy {
    Reject,
    MainExecutable
};

namespace module_detail {
inline const char* NativeExtension() {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

inline char ExtensionByte(char value) {
    constexpr int LowercaseOffset = 'a' - 'A';
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + LowercaseOffset) : value;
}

inline bool HasExtension(const char* path, const char* suffix) {
    const std::size_t pathBytes = std::strlen(path);
    const std::size_t suffixBytes = std::strlen(suffix);
    if (pathBytes < suffixBytes) {
        return false;
    }
    for (std::size_t index = 0; index < suffixBytes; ++index) {
        if (ExtensionByte(path[pathBytes - suffixBytes + index]) != suffix[index]) {
            return false;
        }
    }
    return true;
}

inline char* WithNativeExtension(const char* path) {
    std::size_t prefixBytes = std::strlen(path);
#if !defined(_WIN32)
    // Existing callers also supply Windows module names on native POSIX targets.
    constexpr const char* ForeignExtensions[] = {".dll", ".so", ".dylib"};
    for (const char* extension : ForeignExtensions) {
        if (HasExtension(path, extension)) {
            if (std::strcmp(extension, NativeExtension()) == 0) {
                return nullptr;
            }
            prefixBytes -= std::strlen(extension);
            break;
        }
    }
#endif
    const std::size_t suffixBytes = std::strlen(NativeExtension());
    if (prefixBytes > (std::numeric_limits<std::size_t>::max)() - suffixBytes - 1) {
        return nullptr;
    }
    auto* result = static_cast<char*>(std::malloc(prefixBytes + suffixBytes + 1));
    if (!result) {
        return nullptr;
    }
    std::memcpy(result, path, prefixBytes);
    std::memcpy(result + prefixBytes, NativeExtension(), suffixBytes + 1);
    return result;
}

#if !defined(_WIN32)
inline char* CurrentDirectory() {
    constexpr std::size_t InitialDirectoryBytes = 1024;
    std::size_t bytes = InitialDirectoryBytes;
    for (;;) {
        auto* path = static_cast<char*>(std::malloc(bytes));
        if (!path) {
            return nullptr;
        }
        if (getcwd(path, bytes)) {
            return path;
        }
        const int error = errno;
        std::free(path);
        if (error != ERANGE || bytes > (std::numeric_limits<std::size_t>::max)() / 2) {
            errno = error;
            return nullptr;
        }
        bytes *= 2;
    }
}

inline char* AbsoluteName(const char* name) {
    const std::size_t nameBytes = std::strlen(name);
    if (nameBytes == (std::numeric_limits<std::size_t>::max)()) {
        return nullptr;
    }
    if (name[0] == '/') {
        auto* path = static_cast<char*>(std::malloc(nameBytes + 1));
        if (path) {
            std::memcpy(path, name, nameBytes + 1);
        }
        return path;
    }
    char* directory = CurrentDirectory();
    if (!directory) {
        return nullptr;
    }
    const std::size_t directoryBytes = std::strlen(directory);
    constexpr std::size_t SeparatorAndTerminator = 2;
    if (nameBytes > (std::numeric_limits<std::size_t>::max)() - directoryBytes - SeparatorAndTerminator) {
        std::free(directory);
        return nullptr;
    }
    auto* path = static_cast<char*>(std::malloc(directoryBytes + nameBytes + SeparatorAndTerminator));
    if (path) {
        std::memcpy(path, directory, directoryBytes);
        const bool separator = directoryBytes != 0 && directory[directoryBytes - 1] != '/';
        if (separator) {
            path[directoryBytes] = '/';
        }
        std::memcpy(path + directoryBytes + (separator ? 1 : 0), name, nameBytes + 1);
    }
    std::free(directory);
    return path;
}

inline void* BorrowCandidate(const char* name) {
    // Queries are ref-counted. RTLD_LOCAL also prevents promoting a hidden image to global visibility.
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/dlopen.3.html
    void* module = dlopen(name, RTLD_NOW | RTLD_NOLOAD | RTLD_LOCAL);
    if (module) {
        dlclose(module);
    }
    return module;
}

inline void* FindLoadedCandidate(const char* name) {
    void* module = BorrowCandidate(name);
    if (module || name[0] == '/') {
        return module;
    }
    char* absolute = AbsoluteName(name);
    if (!absolute) {
        return nullptr;
    }
    module = BorrowCandidate(absolute);
    std::free(absolute);
    return module;
}
#endif
} // namespace module_detail

inline void* LoadModule(const char* name, ModuleFallback fallback = ModuleFallback::NativeExtension) {
    if (!name || !name[0]) {
        return nullptr;
    }
#if defined(_WIN32)
    HMODULE module = ::LoadLibraryA(name);
    if (!module && fallback == ModuleFallback::NativeExtension) {
        char* alternate = module_detail::WithNativeExtension(name);
        if (!alternate) {
            ::SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return nullptr;
        }
        module = ::LoadLibraryA(alternate);
        const DWORD error = ::GetLastError();
        std::free(alternate);
        ::SetLastError(error);
    }
    return module;
#else
    char* absolute = module_detail::AbsoluteName(name);
    if (!absolute) {
        return nullptr;
    }
    void* module = dlopen(absolute, RTLD_NOW);
    if (!module && fallback == ModuleFallback::NativeExtension) {
        char* alternate = module_detail::WithNativeExtension(absolute);
        if (alternate) {
            module = dlopen(alternate, RTLD_NOW);
            std::free(alternate);
        }
    }
    std::free(absolute);
    return module;
#endif
}

inline void UnloadModule(void* module) {
    if (!module) {
        return;
    }
#if defined(_WIN32)
    ::FreeLibrary(static_cast<HMODULE>(module));
#else
    dlclose(module);
#endif
}

// A borrowed handle is valid only while the caller retains an owning load.
// Queries never load a missing library or retain an additional reference.
inline void* FindLoadedModule(const char* name, ModuleNullPolicy nullName = ModuleNullPolicy::Reject) {
    if (!name) {
        if (nullName == ModuleNullPolicy::Reject) {
            return nullptr;
        }
#if defined(_WIN32)
        return ::GetModuleHandleA(nullptr);
#else
        void* module = dlopen(nullptr, RTLD_NOW);
        if (module && module != RTLD_DEFAULT) {
            dlclose(module);
        }
        return module;
#endif
    }
    if (!name[0]) {
        return nullptr;
    }
#if defined(_WIN32)
    return ::GetModuleHandleA(name);
#else
    void* module = module_detail::FindLoadedCandidate(name);
    if (!module) {
        char* alternate = module_detail::WithNativeExtension(name);
        if (alternate) {
            module = module_detail::FindLoadedCandidate(alternate);
            std::free(alternate);
        }
    }
    return module;
#endif
}

inline void* GetSymbol(void* module, const char* name, ModuleNullPolicy nullModule = ModuleNullPolicy::Reject) {
    if (!module && nullModule == ModuleNullPolicy::MainExecutable) {
        module = FindLoadedModule(nullptr, ModuleNullPolicy::MainExecutable);
    }
    if (!module) {
        return nullptr;
    }
#if defined(_WIN32)
    // Windows also accepts low-word ordinals here; do not dereference the name.
    return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(module), name));
#else
    constexpr std::uintptr_t MaximumWindowsOrdinal = 0xffff;
    if (reinterpret_cast<std::uintptr_t>(name) <= MaximumWindowsOrdinal) {
        return nullptr;
    }
    return dlsym(module, name);
#endif
}
} // namespace cse_platform
#endif
