#include "../../../GoldSrc-SDK/engine/model.h"
#include "../../../GoldSrc-SDK/engine/zone.h"
#include "../../../GoldSrc-SDK/engine/engine_studio.h"
#include <dlfcn.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstring>
int main(int argc, char **argv) {
    assert(argc >= 2);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    void *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    assert(module);
    auto init = reinterpret_cast<void (*)(void *, int)>(dlsym(module, "_Z11Memory_InitPvi"));
    auto load = reinterpret_cast<void (*)(model_t *, void *)>(dlsym(module, "_Z19Mod_LoadStudioModelP7model_sPv"));
    auto check = reinterpret_cast<void *(*)(cache_user_t *)>(dlsym(module, "_Z11Cache_CheckP12cache_user_s"));
    auto low = reinterpret_cast<void *(*)(int, const char *)>(dlsym(module, "_Z14Hunk_AllocNameiPKc"));
    assert(init && load && check && low);
    auto *little = reinterpret_cast<int (**)(int)>(dlsym(module, "LittleLong"));
    *little = reinterpret_cast<int (*)(int)>(dlsym(module, "_Z10LongNoSwapi"));
    assert(*little);
    constexpr int HeapBytes = 64 * 1024 * 1024;
    void *heap = std::malloc(HeapBytes);
    assert(heap);
    init(heap, HeapBytes);
    std::vector<model_t> models(argc - 2);
    auto find = reinterpret_cast<model_t *(*)(int, const char *)>(dlsym(module, "_Z12Mod_FindNameiPKc"));
    auto modInit = reinterpret_cast<void (*)()>(dlsym(module, "_Z8Mod_Initv"));
    assert(find && modInit);
    model_t *retained = find(0, "restart-model");
    for (int arg = 2; arg < argc; ++arg) {
        std::ifstream file(argv[arg], std::ios::binary);
        std::vector<byte> bytes((std::istreambuf_iterator<char>(file)), {});
        if (bytes.size() < 244 || std::memcmp(bytes.data(), "IDST", 4) != 0) {
            continue;
        }
        std::printf("LOAD %s\n", argv[arg]);
        model_t &model = models[arg - 2];
        std::snprintf(model.name, sizeof(model.name), "model%d", arg);
        load(&model, bytes.data());
        assert(check(&model.cache));
        low(32768, "relocation");
        for (model_t &previous : models) {
            if (previous.cache.data) {
                check(&previous.cache);
            }
        }
    }
    studiohdr_t synthetic{};
    synthetic.version = STUDIO_VERSION;
    synthetic.length = sizeof(synthetic);
    std::vector<byte> restartBytes(sizeof(synthetic));
    std::memcpy(restartBytes.data(), &synthetic, sizeof(synthetic));
    if (argc > 2) {
        std::ifstream restartFile(argv[2], std::ios::binary);
        restartBytes.assign(std::istreambuf_iterator<char>(restartFile), {});
    }
    load(retained, restartBytes.data());
    assert(retained->cache.data);
    // Host_Init starts a new hunk then calls Mod_Init. No old cache may survive.
    auto commandsShutdown = reinterpret_cast<void (*)()>(dlsym(module, "_Z12Cmd_Shutdownv"));
    assert(commandsShutdown);
    commandsShutdown();
    init(heap, HeapBytes);
    modInit();
    model_t *restarted = find(0, "restart-model");
    assert(restarted->cache.data == nullptr);
    std::puts("PASS actual studio cache heap and model generation reset");
    dlclose(module);
    std::free(heap);
}
