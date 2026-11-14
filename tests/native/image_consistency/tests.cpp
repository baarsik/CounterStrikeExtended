#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using byte = unsigned char;
using qboolean = int;
using CRC32_t = unsigned int;
constexpr qboolean TRUE = 1;
constexpr qboolean FALSE = 0;
constexpr int CL_UPDATE_BACKUP = 2;
constexpr int MAX_CLIENTS = 2;
constexpr int MAX_CONSISTENCY_FILES = 16;
constexpr int SIGNONS = 2;
enum { force_exactfile = 0, force_model_samebounds = 1 };
constexpr int t_generic = 0;
constexpr int t_sound = 1;
constexpr int t_model = 2;
constexpr int ca_active = 3;
constexpr int ca_disconnected = 0;
constexpr int clc_stringcmd = 1;
struct resource_t {
    resource_t* pNext;
    resource_t* pPrev;
    char szFileName[128];
    int type;
    byte rguc_reserved[32];
};
struct consistency_t {
    char* filename;
    int issound;
    int orig_index;
    int value;
    int check_type;
};
struct model_t { const char* name; };
struct TestClient {
    struct Frame { struct { void* entities; int num_entities; } packet_entities; } frames[CL_UPDATE_BACKUP];
    struct Player { int customdata; } players[MAX_CLIENTS];
    resource_t resourcesneeded;
    resource_t resourcesonhand;
    char downloadUrl[32];
    consistency_t consistency_list[MAX_CONSISTENCY_FILES];
    int num_consistency;
    bool need_force_consistency_response;
    int servercount;
    int maxclients;
    model_t* model_precache[2];
    model_t* worldmodel;
    CRC32_t mapCRC;
};
TestClient cl{};
struct TestConnection {
    struct { bool custom; } dl;
    int signon;
    int state;
    bool demoplayback;
    bool passive;
} cls{};
struct TestCvar { float value; };
static TestCvar fs_startup_timings{};
#ifdef TEST_HARDWARE
static TestCvar fs_perf_warnings{};
#endif
static bool noclip_anglehack;
struct sizebuf_t {};
struct TestEntity { model_t* model; };
static TestEntity entities[1];
static TestEntity* cl_entities = entities;
void CL_ClearResourceLists();
void Mem_Free(void* pointer) { std::free(pointer); }
void COM_ClearCustomizationList(int*, int) {}
void CL_ClearCaches() {}
void CL_CreateResourceList() {}
void AddStartupTiming(const char*) {}
void ContinueLoadingProgressBar(const char*, int, float) {}
void CL_ReallocateDynamicData(int) {}
void CL_PrecacheBSPModels(const char*) {}
void CL_Disconnect() {}
void CL_InitSpectator() {}
void R_NewMap() {}
void Hunk_Check() {}
void COM_ExplainDisconnection(int, const char*) {}
void COM_Munge2(byte*, int, byte) {}
void MSG_WriteByte(sizebuf_t*, int) {}
void MSG_WriteString(sizebuf_t*, const char*) {}
const char* va(const char*, ...) { return ""; }
static std::function<void()> observeConsistencySend;
void CL_SendConsistencyInfo(sizebuf_t*) {
    if (observeConsistencySend) {
        observeConsistencySend();
    }
    cl.need_force_consistency_response = false;
}
void Con_Printf(const char*, ...) {}
void Con_DPrintf(const char*, ...) {}
void Host_Error(const char* message, ...) { throw std::runtime_error(message); }
void Sys_Error(const char* message, ...) { throw std::runtime_error(message); }
void Mod_NeedCRC(const char*, bool) {}
void COM_UnMunge(byte*, std::size_t, int) {}
static std::vector<unsigned int> bits;
static std::size_t bitIndex;
unsigned int MSG_ReadBits(int) { return bits.at(bitIndex++); }
#define Q_memset std::memset
#define Q_memcmp std::memcmp
#define Q_memcpy std::memcpy
#define _snprintf snprintf

static std::map<std::string, std::vector<byte>> files;
static int liveFiles;
byte* COM_LoadFile(char* path, int, int* length) {
    auto file = files.find(path);
    if (file == files.end()) {
        return nullptr;
    }
    *length = static_cast<int>(file->second.size());
    auto* result = static_cast<byte*>(std::malloc(file->second.size()));
    std::memcpy(result, file->second.data(), file->second.size());
    ++liveFiles;
    return result;
}
void COM_FreeFile(void* file) { --liveFiles; std::free(file); }
void* Hunk_AllocName(int bytes, const char*) { return std::malloc(bytes); }
#include "engine_image.h"
#ifdef TEST_HARDWARE
qboolean LoadTGA2(char*, byte*, int, int*, int*, qboolean);
#include "hardware_loaders.inc"
#else
struct colorVec { unsigned int r; unsigned int g; unsigned int b; unsigned int a; };
static bool is15bit;
constexpr unsigned int COLOR_GREENMASK_555 = 0x03e0;
constexpr unsigned int COLOR_REDMASK_555 = 0x7c00;
constexpr unsigned int COLOR_GREENMASK_565 = 0x07e0;
constexpr unsigned int COLOR_REDMASK_565 = 0xf800;
#include "software_loaders.inc"
#endif
using FileHandle_t = const std::vector<byte>*;
FileHandle_t FS_Open(const char* path, const char*) {
    const auto file = files.find(path);
    return file == files.end() ? nullptr : &file->second;
}
void VGui_LoadBMP(FileHandle_t, byte* pixels, int, int* width, int* height) {
    *width = 1;
    *height = 1;
    const byte green[] = {0, 255, 0, 255};
    std::memcpy(pixels, green, sizeof(green));
}
static std::map<int, std::vector<byte>> pixels;
#ifdef TEST_HARDWARE
static bool IsPowerOfTwo(int value) { return value > 0 && !(value & (value - 1)); }
constexpr unsigned int SURFACE_PIXEL_BYTES = 4;
struct Texture {
    int _id;
    int _wide;
    int _tall;
    float _s0;
    float _t0;
    float _s1;
    float _t1;
};
static_assert(sizeof(Texture) == 28, "Keep hardware texture ABI");
static std::vector<Texture> hardwareTextures;
static Texture* staticTextureCurrent;
static int currenttexture;
static Texture* staticGetTextureById(int id) {
    for (Texture& texture : hardwareTextures) {
        if (texture._id == id) {
            return &texture;
        }
    }
    return nullptr;
}
static Texture* staticAllocTextureForId(int) {
    hardwareTextures.push_back(Texture{});
    return &hardwareTextures.back();
}
static bool g_bSupportsNPOTTextures = true;
using GLfloat = float;
using GLint = int;
constexpr int GL_TEXTURE_2D = 1;
constexpr int GL_LINEAR = 2;
constexpr int GL_NEAREST = 3;
constexpr int GL_TEXTURE_MIN_FILTER = 4;
constexpr int GL_TEXTURE_MAG_FILTER = 5;
constexpr int GL_TEXTURE_WRAP_S = 6;
constexpr int GL_TEXTURE_WRAP_T = 7;
constexpr int GL_CLAMP_TO_EDGE = 8;
constexpr int GL_TEXTURE_ENV = 9;
constexpr int GL_TEXTURE_ENV_MODE = 10;
constexpr int GL_MODULATE = 11;
constexpr int GL_RGBA8 = 12;
constexpr int GL_RGB5_A1 = 13;
constexpr int GL_RGBA = 14;
constexpr int GL_BGRA = 15;
constexpr int GL_UNSIGNED_BYTE = 16;
enum { GL_TEXTURE_BINDING_2D = 17 };
static int gpuBinding;
static bool boundNegativeTexture;
void qglBindTexture(int, int id) {
    boundNegativeTexture = boundNegativeTexture || id < 0;
    gpuBinding = id;
}
void qglGetIntegerv(int, GLint* value) { *value = gpuBinding; }
void qglTexParameterf(int, int, GLfloat) {}
void qglTexParameteri(int, int, int) {}
void qglTexEnvf(int, int, GLfloat) {}
void qglTexImage2D(int, int, int, int width, int height, int, int format, int, const char* source) {
    pixels[gpuBinding].assign(source, source + width * height * 4);
    if (format == GL_BGRA) {
        auto& data = pixels[gpuBinding];
        for (std::size_t offset = 0; offset < data.size(); offset += 4) {
            std::swap(data[offset], data[offset + 2]);
        }
    }
}
void VideoMode_GetCurrentVideoMode(int* width, int* height, int* bpp) {
    *width = 640;
    *height = 480;
    *bpp = 32;
}
#endif
#ifndef TEST_HARDWARE
constexpr std::size_t TextureChannels = 4;
#endif
struct vgui_texture_t { int id; int wide; int tall; };
struct TestTextureStore {
    std::map<int, vgui_texture_t> textures;
    vgui_texture_t* GetTexture(int id) {
        const auto found = textures.find(id);
        return found == textures.end() ? nullptr : &found->second;
    }
    void Insert(int id, int width, int height, const char* source) {
        textures[id] = {id, width, height};
        pixels[id].assign(source, source + width * height * 4);
    }
};
struct EngineSurfaceRenderPlat { TestTextureStore* textures; vgui_texture_t* current; };
class IEngineSurface {};
class EngineSurface : public IEngineSurface {
public:
    EngineSurface() : _renderPlat(new EngineSurfaceRenderPlat{new TestTextureStore, nullptr}) {}
    ~EngineSurface();
    void drawSetTextureFile(int, const char*, int, bool);
    void drawSetTextureRGBA(int, const char*, int, int, int, int);
    void drawSetTextureBGRA(int, const char*, int, int, int, int);
    void drawSetTexture(int);
    void drawFlushText() {}
    void refreshFileTextures();
    void deleteRenderPlat() { delete _renderPlat->textures; delete _renderPlat; }
    EngineSurfaceRenderPlat* _renderPlat;
    static EngineSurface* s_pEngineSurface;
};
EngineSurface* EngineSurface::s_pEngineSurface;
class BaseUISurface {
public:
    bool DeleteTextureByID(int);
    IEngineSurface* _engineSurface;
};
#ifdef TEST_SOFTWARE
#define SW_RENDERER
#endif
using GLuint = unsigned int;
void qglDeleteTextures(int count, const GLuint* ids) {
    for (int index = 0; index < count; ++index) {
        pixels.erase(static_cast<int>(ids[index]));
    }
}
namespace cse_vgui1_native { void ForgetEngineSurface(EngineSurface*) {} }
#include "policy.inc"
#ifdef TEST_HARDWARE
#include "hardware_surface.inc"
#else
#include "software_surface.inc"
#endif
#include "connection.inc"

static int failures;
void Check(bool passed, const char* name) {
    if (!passed) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", name);
    }
}
std::vector<byte> Targa(byte red, byte green, byte blue) {
    std::vector<byte> image(18, 0);
    image[2] = 2;
    image[12] = 1;
    image[14] = 1;
    image[16] = 32;
    image[17] = 0x20;
    image.insert(image.end(), {blue, green, red, 255});
    return image;
}
void SetConsistency(const std::vector<std::string>& names, int type = t_generic, int checkType = force_exactfile) {
    CL_ClearClientState();
    bits = {1};
    for (std::size_t index = 0; index < names.size(); ++index) {
        auto* resource = static_cast<resource_t*>(std::calloc(1, sizeof(resource_t)));
        std::strncpy(resource->szFileName, names[index].c_str(), sizeof(resource->szFileName) - 1);
        resource->type = type;
        resource->rguc_reserved[0] = static_cast<byte>(checkType);
        resource->pNext = &cl.resourcesneeded;
        resource->pPrev = cl.resourcesneeded.pPrev;
        resource->pPrev->pNext = resource;
        cl.resourcesneeded.pPrev = resource;
        bits.insert(bits.end(), {1, 0, static_cast<unsigned int>(index)});
    }
    bits.push_back(0);
    bitIndex = 0;
    CL_ParseConsistencyInfo();
}
void Register(bool demo = false) {
    static model_t world{"maps/test.bsp"};
    cl.model_precache[1] = &world;
    cl.maxclients = 1;
    cls.state = 1;
    cls.demoplayback = demo;
    cls.passive = true;
    sizebuf_t message;
    CL_RegisterResources(&message);
}
std::vector<byte> Load(const char* name) {
    byte image[16]{};
    int width = 0;
    int height = 0;
    if (!LoadTGA(const_cast<char*>(name), image, sizeof(image), &width, &height)) {
        return {};
    }
    return {image, image + width * height * 4};
}
int main() {
    const std::vector<byte> red = {255, 0, 0, 255};
    const std::vector<byte> blue = {0, 0, 255, 255};
    const std::vector<byte> green = {0, 255, 0, 255};
    std::vector<byte> webp;
    Check(cse::image::EncodeWebPLossless(red.data(), 1, 1, 4, 4, false, webp), "encode fixture");
    files["scope.tga"] = Targa(0, 0, 255);
    files["scope.webp"] = webp;
    CL_ClearClientState();
    Check(Load("scope.tga") == red, "ordinary TGA request prefers WebP");
    files["Scope.TGA"] = Targa(0, 255, 0);
    SetConsistency({"./Scope.TGA"});
    Check(Load("scope.tga") == green, "case-insensitive match loads actual server-declared TGA spelling");
    files.erase("Scope.TGA");
    Check(Load("scope").empty(), "missing checked case variant cannot fall back to different lowercase TGA");
    std::vector<byte> greenWebP;
    Check(cse::image::EncodeWebPLossless(green.data(), 1, 1, 4, 4, false, greenWebP), "encode case fixture");
    files["Scope.WEBP"] = greenWebP;
    SetConsistency({"Scope.WEBP"});
    Check(Load("scope.tga") == green, "case-insensitive match loads actual server-declared WebP spelling");
    files["Sprites/Scope.TGA"] = Targa(0, 255, 0);
    SetConsistency({"Sprites\\Scope.TGA"});
    Check(Load("sprites/scope") == green, "slash normalization still loads the checked server filename");
    SetConsistency({"unrelated.tga"});
    Check(Load("scope.tga") == red, "unrelated checked image leaves ordinary sidecars enabled");
    SetConsistency({"scope.tga"}, t_sound);
    Check(Load("scope.tga") == red, "sound consistency is not an image policy");
    SetConsistency({"scope.tga"}, t_model, force_model_samebounds);
    Check(Load("scope.tga") == red, "model bounds checks are not exact image checks");
    SetConsistency({"scope.tga"});
    Check(Load("scope.tga") == blue, "exact TGA rejects unchecked sidecar");
    Check(Load("scope") == blue, "extensionless exact TGA rejects unchecked sidecar");
    Check(Load("scope.webp").empty(), "explicit unchecked WebP cannot bypass paired exact TGA");
    cl.need_force_consistency_response = false;
    Check(Load("scope.tga") == blue, "protection survives sending consistency response");
    SetConsistency({"scope.tga", "scope.webp"});
    Check(Load("scope.tga") == red, "independently checked WebP may replace checked TGA");
    SetConsistency({"scope.webp"});
    files.erase("scope.webp");
    Check(Load("scope.tga").empty(), "WebP-only policy cannot use unchecked stock TGA");
    files["scope.webp"] = webp;
    SetConsistency({"scope.tga"});
    files["scope.tga"] = webp;
    Check(Load("scope.tga") == red, "checked TGA bytes may contain WebP payload");
    files["scope.tga"] = Targa(0, 0, 255);
    {
        EngineSurface surface;
        CL_ClearClientState();
        std::string request = "scope";
        surface.drawSetTextureFile(1, request.c_str(), 1, true);
        request = "unrelated";
        surface.drawSetTexture(1);
        Check(pixels[1] == red, "texture cached before joining uses optional WebP");
        SetConsistency({"scope.tga"});
        bool refreshedBeforeResponse = false;
        observeConsistencySend = [&]() { refreshedBeforeResponse = pixels[1] == blue; };
        Register();
        observeConsistencySend = nullptr;
        Check(refreshedBeforeResponse, "cached image is refreshed before sending consistency response");
        Check(pixels[1] == blue, "post-download registration refreshes already bound texture ID");
        surface.drawSetTextureRGBA(10, reinterpret_cast<const char*>(green.data()), 1, 1, 0, 0);
        surface.drawSetTexture(10);
        CL_ClearClientState();
        Register();
#ifdef TEST_HARDWARE
        Check(currenttexture == 10 && gpuBinding == 10, "refresh preserves previously bound hardware ID");
#else
        Check(surface._renderPlat->current && surface._renderPlat->current->id == 10,
              "refresh preserves previously bound software ID");
#endif
        Check(pixels[1] == red, "changing to unrestricted server restores optional WebP");
#ifdef TEST_HARDWARE
        surface.drawSetTexture(10);
        currenttexture = 12345;
        qglBindTexture(GL_TEXTURE_2D, 12345);
        Register();
        Check(staticTextureCurrent && staticTextureCurrent->_id == 10,
              "no-op refresh preserves VGUI selection independently of world GL binding");
        Check(currenttexture == 12345 && gpuBinding == 12345,
              "no-op refresh preserves world GL binding and renderer cache");
        SetConsistency({"scope.tga"});
        staticTextureCurrent = nullptr;
        currenttexture = 10;
        qglBindTexture(GL_TEXTURE_2D, 10);
        Register();
        Check(staticTextureCurrent == nullptr,
              "protected refresh preserves null VGUI selection even when GL binding is a VGUI ID");
        surface.drawSetTexture(10);
        currenttexture = -1;
        qglBindTexture(GL_TEXTURE_2D, 45678);
        boundNegativeTexture = false;
        Register();
        Check(staticTextureCurrent && staticTextureCurrent->_id == 10,
              "protected refresh preserves VGUI selection with unknown renderer cache");
        Check(currenttexture == -1 && gpuBinding == 45678 && !boundNegativeTexture,
              "protected refresh restores actual GL binding without binding renderer sentinel");
        CL_ClearClientState();
        files.erase("late.tga");
        surface.drawSetTextureFile(42, "late", 0, true);
        files["late.tga"] = Targa(0, 0, 255);
        SetConsistency({"scope.tga", "late.tga"});
        hardwareTextures.shrink_to_fit();
        surface.drawSetTexture(10);
        currenttexture = 12345;
        qglBindTexture(GL_TEXTURE_2D, 12345);
        Register();
        Check(staticTextureCurrent && staticTextureCurrent == staticGetTextureById(10),
              "refresh reacquires selected VGUI record after texture storage relocation");
        Check(pixels[42] == blue && currenttexture == 12345 && gpuBinding == 12345,
              "late protected texture insertion preserves renderer GL binding");
        CL_ClearClientState();
        Register();
#endif
        files.erase("scope.webp");
        surface.drawSetTextureFile(4, "scope", 0, true);
        Check(pixels[4] == blue, "unrestricted pre-download texture caches stock TGA");
        SetConsistency({"scope.webp"});
        files["scope.webp"] = webp;
        Register();
        Check(pixels[4] == red, "new WebP-only requirement refreshes unrestricted cached TGA");
        files.erase("scope.webp");
        surface.drawSetTextureFile(2, "scope", 0, true);
        Check(pixels[2] == std::vector<byte>({0, 0, 0, 255}), "required missing WebP fails closed");
        files["scope.webp"] = webp;
        Register(true);
        Check(pixels[2] == red, "demo registration refreshes WebP downloaded after HUD init");
        SetConsistency({"scope.tga"});
        files.erase("scope.tga");
        files["scope.bmp"] = {1};
        Register();
        Check(pixels[1] == std::vector<byte>({0, 0, 0, 255}), "missing protected TGA rejects BMP and stale WebP");
        files["scope.tga"] = Targa(0, 0, 255);
        Register();
        Check(pixels[1] == blue, "failed protected load retains provenance for later retry");
        surface.drawSetTextureRGBA(1, reinterpret_cast<const char*>(red.data()), 1, 1, 0, 0);
        Register();
        Check(pixels[1] == red, "programmatic RGBA upload retires file provenance");
        surface.drawSetTextureFile(3, "scope", 0, true);
        const byte redBgra[] = {0, 0, 255, 255};
        surface.drawSetTextureBGRA(3, reinterpret_cast<const char*>(redBgra), 1, 1, 0, 0);
        Register();
        Check(pixels[3] == red, "programmatic BGRA upload retires file provenance");
#ifdef TEST_HARDWARE
        surface.drawSetTextureFile(5, "scope", 0, true);
        BaseUISurface base{&surface};
        Check(base.DeleteTextureByID(5), "hardware surface deletes texture");
        Register();
        Check(pixels.count(5) == 0, "registration cannot recreate a deleted texture ID");
#endif
        files["logo.bmp"] = {1};
        SetConsistency({"logo.bmp"});
        surface.drawSetTextureFile(6, "logo.bmp", 0, true);
        Check(pixels[6] == green, "literal checked BMP keeps its legacy BMP loader");
    }
    Register();
    CL_ClearClientState();
    Check(liveFiles == 0, "loaded files are released");
    std::printf("image consistency: %d failures\n", failures);
    return failures ? 1 : 0;
}
