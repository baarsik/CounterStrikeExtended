#include "../../../src/engine/l_studio.cpp"
#include <cassert>
#include <vector>
#include <fstream>
#include <iterator>
#include <cstring>
#include <stdexcept>
byte texgammatable[256];
static std::vector<byte> allocation;
static int allocatedBytes;
void *Cache_Alloc(cache_user_t *cache, int bytes, const char *) {
    allocatedBytes = bytes;
    allocation.assign(static_cast<std::size_t>(bytes) + 16 * 1024 * 1024, 0xa5);
    cache->data = allocation.data();
    return cache->data;
}
void Q_memset(void *target, int value, int bytes) {
    std::memset(target, value, bytes);
}
void Q_memcpy(void *target, const void *source, int bytes) {
    std::memcpy(target, source, bytes);
}
void Q_strcpy(char *target, const char *source) {
    std::strcpy(target, source);
}
static int Identity(int value) {
    return value;
}
int (*LittleLong)(int) = Identity;
int main(int argc, char **argv) {
    for (int i = 0; i < 256; ++i) {
        texgammatable[i] = i;
    }
    for (int arg = 1; arg < argc; ++arg) {
        std::ifstream file(argv[arg], std::ios::binary);
        std::vector<byte> bytes((std::istreambuf_iterator<char>(file)), {});
        if (bytes.size() < sizeof(studiohdr_t) || std::memcmp(bytes.data(), "IDST", 4) != 0) {
            continue;
        }
        model_t model{};
        Mod_LoadStudioModel(&model, bytes.data());
        for (std::size_t i = allocatedBytes; i < allocation.size(); ++i) {
            if (allocation[i] != 0xa5) {
                std::fprintf(stderr, "OVERFLOW %s allocation=%d first=%zu\n", argv[arg], allocatedBytes, i);
                return 1;
            }
        }
    }
}
