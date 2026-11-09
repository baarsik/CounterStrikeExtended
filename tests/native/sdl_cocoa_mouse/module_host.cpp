#define SDL_MAIN_HANDLED 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

namespace {
std::atomic<std::uint32_t> destroyed{0};
std::atomic<std::uint32_t> closingEpoch{0};
std::atomic<std::uint32_t> removedEpoch{0};
std::atomic<const mach_header *> observedHeader{nullptr};
void Removed(const mach_header *header, intptr_t) {
    if (header == observedHeader.load()) {
        removedEpoch.store(closingEpoch.load());
    }
}
bool Mapped(std::uintptr_t pointer) {
    mach_vm_address_t address = pointer;
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    const kern_return_t result = mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64,
        reinterpret_cast<vm_region_info_t>(&info), &count, &object);
    if (object != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), object);
    }
    return result == KERN_SUCCESS && address <= pointer && pointer - address < size;
}
bool Listed(const mach_header *header) {
    const std::uint32_t count = _dyld_image_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        if (_dyld_get_image_header(index) == header) {
            return true;
        }
    }
    return false;
}
void Observe(std::uint32_t epoch) {
    destroyed.store(epoch);
}
void Require(bool value, const char *what) {
    if (!value) {
        std::fprintf(stderr, "consumer-unload:%s\n", what);
        std::exit(17);
    }
}
template <typename Function> Function Symbol(void *image, const char *name) {
    void *symbol = dlsym(image, name);
    Require(symbol != nullptr, name);
    Function result = nullptr;
    static_assert(sizeof(result) == sizeof(symbol), "Darwin C function-pointer representation");
    std::memcpy(&result, &symbol, sizeof(result));
    return result;
}
bool Loaded(const char *path) {
    void *image = dlopen(path, RTLD_NOW | RTLD_NOLOAD | RTLD_LOCAL);
    if (image) {
        Require(dlclose(image) == 0, "balance NOLOAD reference");
    }
    return image != nullptr;
}
} // namespace

int main(int argc, char **argv) {
    Require(argc == 3, "exact provider and consumer required");
    void *provider = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    Require(provider != nullptr, "load pinned provider");
    const auto ready = Symbol<decltype(&SDL_SetMainReady)>(provider, "SDL_SetMainReady");
    const auto init = Symbol<decltype(&SDL_InitSubSystem)>(provider, "SDL_InitSubSystem");
    const auto quit = Symbol<decltype(&SDL_Quit)>(provider, "SDL_Quit");
    const auto create = Symbol<decltype(&SDL_CreateWindow)>(provider, "SDL_CreateWindow");
    const auto destroy = Symbol<decltype(&SDL_DestroyWindow)>(provider, "SDL_DestroyWindow");
    const auto id = Symbol<decltype(&SDL_GetWindowID)>(provider, "SDL_GetWindowID");
    const auto lookup = Symbol<decltype(&SDL_GetWindowFromID)>(provider, "SDL_GetWindowFromID");
    _dyld_register_func_for_remove_image(Removed);
    ready();
    for (std::uint32_t epoch = 1; epoch <= 3; ++epoch) {
        Require(init(SDL_INIT_VIDEO), "initialize owned video context");
        SDL_Window *window = create("CSE Consumer Lifecycle", 640, 480, SDL_WINDOW_HIDDEN);
        Require(window != nullptr, "create owned hidden window");
        const auto windowId = id(window);
        void *consumer = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        Require(consumer != nullptr, "load consumer");
        using Attach = std::uint32_t (*)(std::uint32_t, void (*)(std::uint32_t));
        using Bind = std::uint32_t (*)(SDL_Window *);
        using Close = void (*)();
        const auto attach = Symbol<Attach>(consumer, "CseFixture_AttachConsumer");
        const auto bind = Symbol<Bind>(consumer, "CseFixture_BindWindow");
        const auto close = Symbol<Close>(consumer, "CseFixture_CloseConsumer");
        Dl_info imageInfo{};
        Require(dladdr(dlsym(consumer, "CseFixture_AttachConsumer"), &imageInfo) != 0, "identify loaded consumer");
        const auto *header = static_cast<const mach_header *>(imageInfo.dli_fbase);
        Require(header && header->filetype == MH_DYLIB && Listed(header), "actual dylib image listed");
        const std::uint32_t flags = header->flags;
        const auto headerAddress = reinterpret_cast<std::uintptr_t>(header);
        observedHeader.store(header);
        closingEpoch.store(epoch);
        removedEpoch.store(0);
        Require(attach(epoch, Observe) == 1, "fresh consumer data epoch");
        Require(bind(window) == 1, "actual hidden-window binding");
        close();
        destroy(window);
        Require(lookup(windowId) == nullptr, "owned window context closed");
        quit();
        Require(dlclose(consumer) == 0, "close consumer reference");
        const bool mapped = Mapped(headerAddress);
        const bool listed = Listed(header);
        const bool present = Loaded(argv[2]);
        const bool providerPresent = Loaded(argv[1]);
        std::printf("{\"epoch\":%u,\"consumerPresent\":%d,\"mapped\":%d,\"listed\":%d,"
                    "\"destroyedEpoch\":%u,\"removedEpoch\":%u,\"providerPresent\":%d,\"flags\":%u}\n",
                    epoch, present, mapped, listed, destroyed.load(), removedEpoch.load(), providerPresent, flags);
        std::fflush(stdout);
        Require(!present && !mapped && !listed && destroyed.load() == epoch && removedEpoch.load() == epoch &&
                providerPresent && (flags & MH_HAS_TLV_DESCRIPTORS) == 0, "actual consumer unload");
    }
    Require(dlclose(provider) == 0, "close host provider reference");
    return 0;
}
