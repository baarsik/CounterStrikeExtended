#pragma once

#include "client.h"
#include "sys.h"
#if !defined(SW_RENDERER)
#include "glquake.h"
#endif

#include <memory>
#include <cassert>
#include <stdexcept>

model_t mod_known[MAX_KNOWN_MODELS];
model_t* loadmodel;
byte* mod_base;
char loadname[32] = "fixture";
char* wadpath;
char com_gamedir[GAME_DIRECTORY_BYTES];
client_static_t cls;
static texture_t fallbackTexture{};
texture_t* r_notexture_mip = &fallbackTexture;
static std::vector<std::unique_ptr<byte[]>> hunkAllocations;
struct ModelInfo {
    ModelVisibilityInfo visibility;
};
static ModelInfo mod_known_info[MAX_KNOWN_MODELS];
static constexpr int MIPSCALE = 64 + 16 + 4 + 1;
static constexpr int TEXTUREDATA_SIZE = MIPSCALE * (512 * 512) / 64 + 256 * 3 + 2 + sizeof(texture_t);
static int tested = 1;
static int ad_enabled;
byte texgammatable[256];
cvar_t r_wadtextures{};

void* Hunk_AllocName(int bytes, const char*) {
    assert(bytes >= 0);
    hunkAllocations.emplace_back(new byte[bytes ? bytes : 1]{});
    return hunkAllocations.back().get();
}

void Q_memcpy(void* output, const void* input, int count) {
    std::memcpy(output, input, count);
}

void Q_strncpy(char* output, const char* input, int count) {
    std::strncpy(output, input, count);
}

int Q_strcasecmp(const char* left, const char* right) {
    return strcasecmp(left, right);
}

int Q_strncmp(const char* left, const char* right, int count) {
    return std::strncmp(left, right, count);
}

int Q_strcmp(const char* left, const char* right) {
    return std::strcmp(left, right);
}

int Q_stricmp(const char* left, const char* right) {
    return strcasecmp(left, right);
}

int Q_strnicmp(const char* left, const char* right, int count) {
    return strncasecmp(left, right, count);
}

float Length(const vec_t* value) {
    return std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
}

float RadiusFromBounds(vec_t*, vec_t*) {
    return 0;
}

void Sys_Error(const char*, ...) {
    throw std::runtime_error("rejected");
}

void CM_InvalidatePAS(const model_t*) {}
void CL_Disconnect() {}
void COM_ExplainDisconnection(qboolean, const char*, ...) {}

char com_token[COM_TOKEN_SIZE];
const char* COM_Parse(const char* input) {
    if (!input || !*input) {
        return nullptr;
    }
    com_token[0] = '}';
    return input + std::strlen(input);
}
void Mem_Free(void* pointer) {
    std::free(pointer);
}
char* Mem_Strdup(const char* text) {
    return strdup(text);
}
void Con_DPrintf(const char*, ...) {}
double Sys_FloatTime() {
    return 0;
}
void Mod_AdInit() {}
qboolean TEX_InitFromWad(char*) {
    return FALSE;
}
void TEX_AddAnimatingTextures() {}
void TEX_CleanupWadInfo() {}
int TEX_LoadLump(char*, byte*, int) {
    return 0;
}
void R_InitSky() {}

model_t* Mod_FindName(qboolean, const char*) {
    assert(false); // The synthetic fixtures have one submodel.
    return nullptr;
}

#if !defined(SW_RENDERER)
int gl_filter_min;
int GL_LoadTexture(char*, GL_TEXTURETYPE, int, int, byte*, qboolean, int, byte*) {
    return 1;
}
void DT_LoadDetailTexture(char*, int) {}
void Mod_AdSwap(texture_t*, int, byte**, byte**, int) {}
void GL_SubdivideSurface(msurface_t*) {}
#else
void Mod_AdSwap(texture_t*, int, int) {}
#endif

#include "brush_loader.inc"
