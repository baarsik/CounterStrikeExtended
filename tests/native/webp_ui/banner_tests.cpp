#include "image_codec.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#define strcasecmp strcasecmp

struct resource_t {
    char szFileName[260]{};
    unsigned char rgucMD5_hash[16]{};
    unsigned char* data = nullptr;
    int nDownloadSize = 0;
};
constexpr int t_generic = 4;
class TokenLine {
public:
    explicit TokenLine(char* command) {
        char* space = std::strchr(command, ' ');
        if (space) {
            path = space + 1;
        }
    }
    int CountToken() { return path ? 2 : 1; }
    char* GetToken(int) { return path; }
    char* path = nullptr;
};
const char* COM_BinPrintf(const unsigned char*, int) { return "0123456789abcdef0123456789abcdef"; }
struct TestSystem {
    void Printf(const char*, ...) {}
};
struct TestResources {
    void Add(resource_t* resource) { items.push_back(resource); }
    void Remove(resource_t* resource) {
        for (auto iterator = items.begin(); iterator != items.end(); ++iterator) {
            if (*iterator == resource) {
                items.erase(iterator);
                break;
            }
        }
    }
    std::vector<resource_t*> items;
};
class Proxy {
public:
    void CMD_BannerFile(char*);
    resource_t* AddResource(char* path, int type, char*) {
        auto* resource = LoadResourceFromFile(path, type);
        if (resource) {
            m_Resources.Add(resource);
        }
        return resource;
    }
    resource_t* LoadResourceFromFile(char* path, int) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return nullptr;
        }
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), {});
        auto* resource = new resource_t;
        std::snprintf(resource->szFileName, sizeof(resource->szFileName), "%s", path);
        resource->nDownloadSize = static_cast<int>(data.size());
        resource->data = new unsigned char[data.size()];
        std::memcpy(resource->data, data.data(), data.size());
        return resource;
    }
    void FreeResource(resource_t* resource) { delete[] resource->data; delete resource; }
    ~Proxy() {
        for (auto* resource : m_Resources.items) {
            FreeResource(resource);
        }
    }
    resource_t* m_BannerTGA = nullptr;
    TestResources m_Resources;
    TestSystem system;
    TestSystem* m_System = &system;
};

// The runner inserts the exact production CMD_BannerFile function here.
#include "banner_function.inc"

int main(int argc, char** argv) {
    if (argc != 3) {
        return 2;
    }
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), {});
    const std::string base = std::string(argv[2]) + "/banner";
    auto write = [](const std::string& path, const std::vector<unsigned char>& bytes) {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    };
    auto command = [](Proxy& proxy, const std::string& path) {
        std::string value = "bannerfile " + path;
        proxy.CMD_BannerFile(&value[0]);
    };
    write(base + ".webp", data);
    Proxy proxy;
    command(proxy, base + ".tga");
    if (!proxy.m_BannerTGA || !std::strstr(proxy.m_BannerTGA->szFileName, ".webp")
        || proxy.m_BannerTGA->nDownloadSize != static_cast<int>(data.size())
        || std::memcmp(proxy.m_BannerTGA->data, data.data(), data.size())) {
        std::fprintf(stderr, "FAIL: HLTV legacy banner request publishes WebP name and unchanged payload\n");
        return 1;
    }
    command(proxy, base + ".webp");
    if (!proxy.m_BannerTGA || !std::strstr(proxy.m_BannerTGA->szFileName, ".webp")) {
        std::fprintf(stderr, "FAIL: HLTV explicit WebP banner keeps matching resource extension\n");
        return 1;
    }
    data.resize(12);
    write(base + ".webp", data);
    command(proxy, base + ".webp");
    if (proxy.m_BannerTGA) {
        std::fprintf(stderr, "FAIL: HLTV rejects invalid WebP banner payload\n");
        return 1;
    }
    return 0;
}
