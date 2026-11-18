#include "VGui1NativeAtlas.h"
#include "vgui_EngineSurface.h"
#include "utlrbtree.h"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED retirement: %s\n", message);
        std::abort();
    }
}
[[noreturn]] void Unexpected() {
    Check(false, "unrelated real EngineSurface virtual call");
    std::abort();
}
}
#include "upload_fence.inc"
#if defined(CSE_RETIRE_HW)
namespace cse::render {
bool UsingMetal() {
    return false;
}
bool BackendUploadSucceeded() {
    Unexpected();
}
}
namespace {
int currenttexture = -1;
int cnttextures[2] = {-1, -1};
#include "hw_metadata.inc"
int flushes = 0;
int deletes = 0;
int binds = 0;
GLenum error = GL_NO_ERROR;
GLenum nextError = GL_NO_ERROR;
GLenum uploadError = GL_NO_ERROR;
bool persistentError = false;
GLenum APIENTRY ControlledError() {
    const GLenum result = error;
    if (!persistentError) {
        error = nextError;
        nextError = GL_NO_ERROR;
    }
    return result;
}
void ControlledBind(GLenum target, GLuint id) {
    Check(target == GL_TEXTURE_2D && id == 0 && flushes > 0, "unbind follows flush");
    ++binds;
}
void ControlledDelete(GLsizei count, const GLuint* ids) {
    Check(count == 1 && ids && (*ids == 101 || *ids == 201) && flushes > 0 && binds == 1, "delete order and exact owned name");
    ++deletes;
}
auto qglBindTexture = ControlledBind;
auto qglDeleteTextures = ControlledDelete;
class Backend final : public IEngineSurface {
public:
    void drawFlushText() override {
        Check(g_iVertexBufferEntriesUsed == 0 || staticGetTextureById(currenttexture) != nullptr,
            "queued flush precedes metadata removal");
        ++flushes;
    }
    void drawSetTextureRGBA(int id, const char*, int wide, int tall, int, int) override {
        Check(wide == cse_vgui1_native::AtlasSide && tall == cse_vgui1_native::AtlasSide, "actual page dimensions");
        auto* texture = staticAllocTextureForId(id);
        texture->_wide = wide;
        texture->_tall = tall;
        error = uploadError;
    }
    int isTextureIdValid(int id) override {
        return staticGetTextureById(id) != nullptr;
    }
#define ABORT_METHOD(signature) \
    signature override { \
        Unexpected(); \
    }
    ABORT_METHOD(void pushMakeCurrent(int*, int*, int*, bool))
    ABORT_METHOD(void popMakeCurrent())
    ABORT_METHOD(void drawFilledRect(int, int, int, int))
    ABORT_METHOD(void drawOutlinedRect(int, int, int, int))
    ABORT_METHOD(void drawLine(int, int, int, int))
    ABORT_METHOD(void drawPolyLine(int*, int*, int))
    ABORT_METHOD(void drawTexturedPolygon(vgui2::VGuiVertex*, int))
    ABORT_METHOD(void drawSetTexture(int))
    ABORT_METHOD(void drawTexturedRect(int, int, int, int))
    ABORT_METHOD(void drawTexturedRectAdd(int, int, int, int))
    ABORT_METHOD(int createNewTextureID())
    ABORT_METHOD(void drawSetColor(int, int, int, int))
    ABORT_METHOD(void drawSetTextColor(int, int, int, int))
    ABORT_METHOD(void drawSetTextPos(int, int))
    ABORT_METHOD(void drawGetTextPos(int&, int&))
    ABORT_METHOD(void drawPrintChar(int, int, int, int, float, float, float, float))
    ABORT_METHOD(void drawPrintCharAdd(int, int, int, int, float, float, float, float))
    ABORT_METHOD(void drawSetTextureFile(int, const char*, int, bool))
    ABORT_METHOD(void drawGetTextureSize(int, int&, int&))
    ABORT_METHOD(void drawSetSubTextureRGBA(int, int, int, const unsigned char*, int, int))
    ABORT_METHOD(void resetViewPort())
    ABORT_METHOD(void drawSetTextureBGRA(int, const char*, int, int, int, int))
    ABORT_METHOD(void drawUpdateRegionTextureBGRA(int, int, int, const unsigned char*, int, int))
#undef ABORT_METHOD
};
}
extern "C" SDL_FunctionPointer SDL_GL_GetProcAddress(const char* name) {
    Check(std::strcmp(name, "glGetError") == 0, "only injected exact error function requested");
    return reinterpret_cast<SDL_FunctionPointer>(ControlledError);
}
#include "hw_retirement.inc"
int main() {
    Backend backend;
    using namespace cse_vgui1_native;
    std::array<unsigned char, AtlasPageBytes> pixels{};
    Check(UploadEngineFontPage(&backend, 101, pixels.data(), pixels.size()), "actual private upload body");
    currenttexture = 101;
    cnttextures[0] = 101;
    cnttextures[1] = 101;
    staticTextureCurrent = staticGetTextureById(101);
    g_iVertexBufferEntriesUsed = 4;
    Check(RetireEngineFontPage(&backend, 101, true), "actual live retirement");
    Check(deletes == 1 && binds == 1 && staticTextureCurrent == nullptr && currenttexture == -1
        && !staticGetTextureById(101) && cnttextures[0] == -1 && cnttextures[1] == -1,
        "real tree entry and both texture-unit binding caches removed");
    g_iVertexBufferEntriesUsed = 0;
    Texture* other = staticAllocTextureForId(106);
    staticTextureCurrent = other;
    Check(RetireEngineFontPage(&backend, 201, true) && deletes == 2,
        "owned created-but-never-uploaded name is released even without metadata");
    Check(UploadEngineFontPage(&backend, 102, pixels.data(), pixels.size()), "second metadata page");
    Check(RetireEngineFontPage(&backend, 102, false) && staticTextureCurrent == other,
        "retiring one texture preserves the independently selected surface texture");
    Check(UploadEngineFontPage(&backend, 102, pixels.data(), pixels.size()), "recreate retired texture metadata");
    currenttexture = 102;
    staticTextureCurrent = staticGetTextureById(102);
    g_iVertexBufferEntriesUsed = 4;
    Check(RetireEngineFontPage(&backend, 102, false) && deletes == 2 && binds == 1 && flushes == 2,
        "lost epoch invokes no GPU/bind/flush callback");
    Check(g_iVertexBufferEntriesUsed == 0 && !staticGetTextureById(102), "lost queue and real metadata cleared");
    error = GL_INVALID_OPERATION;
    nextError = GL_INVALID_ENUM;
    Check(UploadEngineFontPage(&backend, 103, pixels.data(), pixels.size()) && staticGetTextureById(103),
        "unrelated prior GL error does not reject a successful atlas upload");
    Check(RetireEngineFontPage(&backend, 103, false), "retire atlas after unrelated prior error");
    uploadError = GL_INVALID_OPERATION;
    Check(!UploadEngineFontPage(&backend, 104, pixels.data(), pixels.size()) && staticGetTextureById(104),
        "the atlas upload's own GL error remains a failure");
    Check(RetireEngineFontPage(&backend, 104, false), "failed upload retains metadata for caller rollback");
    uploadError = GL_NO_ERROR;
    error = GL_INVALID_OPERATION;
    persistentError = true;
    Check(!UploadEngineFontPage(&backend, 105, pixels.data(), pixels.size()) && !staticGetTextureById(105),
        "an error source that cannot clear is rejected before upload");
    persistentError = false;
    std::puts("PASS actual HW metadata retirement/upload bodies with injected GL entrypoints");
    return 0;
}
#else
namespace cse::engine_image {
void ForgetFileTexture(IEngineSurface*, int) {}
void ForgetFileTextures(EngineSurface*) {}
}
namespace {
EngineSurface* remembered = nullptr;
}
namespace cse_vgui1_native {
void RememberEngineSurface(EngineSurface* surface) noexcept {
    Check(surface && !remembered, "constructor records actual complete owner");
    remembered = surface;
}
void ForgetEngineSurface(EngineSurface* surface) noexcept {
    Check(remembered == surface, "destructor forgets exact owner");
    remembered = nullptr;
}
}
#if defined(CSE_SW_WIN32_BRANCH)
#undef INTPTR_MAX
#define INTPTR_MAX INT32_MAX
#endif
#include "sw_retirement.inc"
#include "sw_unrelated.inc"
namespace {
template<class Tag, typename Tag::Type Member> struct Access {
    friend typename Tag::Type Field(Tag) {
        return Member;
    }
};
struct PlatTag {
    using Type = EngineSurfaceRenderPlat* EngineSurface::*;
    friend Type Field(PlatTag);
};
template struct Access<PlatTag, &EngineSurface::_renderPlat>;
}
int main() {
    auto* surface = new EngineSurface;
    auto* plat = surface->*Field(PlatTag{});
    constexpr int Side = 512;
    std::array<unsigned char, static_cast<std::size_t>(Side) * Side * 4> pixels{};
    surface->drawSetTextureRGBA(101, reinterpret_cast<const char*>(pixels.data()), Side, Side, 0, 0);
    surface->drawSetTexture(101);
    for (int width : {0, -1, (std::numeric_limits<int>::max)(),
        (std::numeric_limits<int>::max)() / 4 + 1}) {
        surface->drawSetTextureRGBA(101, reinterpret_cast<const char*>(pixels.data()), width, 1, 0, 0);
        Check(plat->current && plat->current->wide == Side && plat->current->tall == Side,
            "invalid upload preserves the existing bound page without allocation");
    }

    Check(plat->current && surface->isTextureIdValid(101), "actual SW owned storage and binding");
    pixels[0] = 19;
    pixels[2] = 73;
    surface->drawSetTextureBGRA(101, reinterpret_cast<const char*>(pixels.data()), Side, Side, 0, 0);
    Check(plat->current == plat->textures->GetTexture(101), "BGRA replacement repairs the bound metadata pointer");
#if defined(CSE_SW_WIN32_BRANCH)
    Check(plat->current->prgba[0] == 19 && plat->current->prgba[2] == 73,
        "Win32 replacement keeps its retained byte interpretation");
#endif
    Check(surface->RetireNativeFontPage(101) && plat->current == nullptr && !surface->isTextureIdValid(101),
        "actual SW retirement unlinks metadata, clears binding and releases array with delete[]");
    Check(surface->RetireNativeFontPage(101) && !surface->RetireNativeFontPage(0), "idempotent/invalid bounds");
    delete surface;
    std::puts("PASS actual SW complete owner and private storage retirement body");
    return 0;
}
#endif
