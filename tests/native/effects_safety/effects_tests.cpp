#include "client.h"
#include "gl_model.h"
#include "r_efx.h"
#include "pm_movevars.h"
#include "entity_types.h"
#include "beamdef.h"
#include "customentity.h"
#include "triangleapi.h"
#include "render.h"
#include <SDL3/SDL_opengl.h>
#include <initializer_list>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

extern "C" void CseSteamModuleInitialize() {}

client_state_t cl{};
vec3_t vec3_origin{};
movevars_t movevars{};
model_t sprite{};
model_t* cl_sprite_ricochet = &sprite;
TEMPENTITY temps[4]{};
TEMPENTITY* gpTempEntFree = temps;
TEMPENTITY* gpTempEntActive = nullptr;
particle_t particles[4]{};
particle_t* free_particles = particles;
particle_t* gpActiveTracers = nullptr;
particle_t* active_particles = nullptr;
int cl_numvisedicts = 0;
int cl_numbeamentities = 0;
cl_entity_t* cl_visedicts[MAX_VISEDICTS]{};
cl_entity_t* cl_beamentities[MAX_BEAMENTS]{};
int deaths = 0;
int updates = 0;
int beamDraws = 0;
cl_entity_t attachmentEntity{};
triangleapi_t tri{};
cvar_t egon_amplitude{};
vec3_t vup{};
vec3_t vright{};
float gNoise[129]{};
vec3_t vpn{};
vec3_t r_origin{};
vec3_t avelocities[NUMVERTEXNORMALS]{};
float r_avertexnormals[NUMVERTEXNORMALS][3]{};
float beamlength = 16;
unsigned short palette[256 * 4]{};
unsigned short* host_basepal = palette;
qboolean filterMode = FALSE;
float filterColorRed = 1;
float filterColorGreen = 1;
float filterColorBlue = 1;
float filterBrightness = 1;
int particletexture = 0;

void GL_Bind(int) {}
void qglEnable(GLenum) {}
void qglDisable(GLenum) {}
void qglTexEnvf(GLenum, GLenum, GLfloat) {}
void qglBlendFunc(GLenum, GLenum) {}
void qglBegin(GLenum) {}
void qglEnd() {}
void qglColor4ubv(const GLubyte*) {}
void qglTexCoord2f(GLfloat, GLfloat) {}
void qglVertex3fv(const GLfloat*) {}
void qglVertex3f(GLfloat, GLfloat, GLfloat) {}
void R_TracerDraw() {}
void R_BeamDrawList() {}

void Noise(float*, int) {}
void SineNoise(float*, int) {}
cl_entity_t* R_GetBeamAttachmentEntity(int) {
    return &attachmentEntity;
}
float* R_BeamGetAttachmentPoint(cl_entity_t* entity, int) {
    return entity->attachment[0];
}
int R_BeamCull(float*, float*, int) {
    return 1;
}
void R_DrawSegs(float*, float*, float, float, float, float, int, int) {
    ++beamDraws;
}
void R_DrawTorus(float*, float*, float, float, float, float, int) {
    ++beamDraws;
}
void R_DrawDisk(float*, float*, float, float, float, float, int) {
    ++beamDraws;
}
void R_DrawCylinder(float*, float*, float, float, float, float, int) {
    ++beamDraws;
}
void R_DrawRing(float*, float*, float, float, float, float, int) {
    ++beamDraws;
}
void R_DrawBeamFollow(BEAM*) {
    ++beamDraws;
}

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void Con_DPrintf(const char*, ...) {}
void Con_Printf(const char*, ...) {}
int RandomLong(int minimum, int) {
    return minimum;
}
float RandomFloat(float minimum, float) {
    return minimum;
}
int ModelFrameCount(model_t*) {
    return 8;
}
model_t* CL_GetModelByIndex(int) {
    return &sprite;
}
int Draw_DecalIndex(int value) {
    return value;
}
int Draw_DecalIndexFromName(char*) {
    return 0;
}
void R_DecalShoot(int, int, int, float*, int) {}
dlight_t* CL_AllocDlight(int) {
    return nullptr;
}
dlight_t* CL_AllocElight(int) {
    return nullptr;
}
int CL_AddVisibleEntity(cl_entity_t*) {
    return 1;
}
void CL_TempEntPlaySound(TEMPENTITY*, float) {}
void ClientDLL_TempEntUpdate(double, double, double, TEMPENTITY**, TEMPENTITY** active,
    int (*)(cl_entity_t*), void (*)(TEMPENTITY*, float)) {
    Check(*active == &temps[0] && temps[0].next == &temps[2] && temps[2].next == nullptr,
        "Only finite temporary entities reach client simulation in their original order");
    ++updates;
}

#include "r_efx.cpp"
#define Q_memset std::memset
#include "effects.inc"

void Reset() {
    cl.time = 10;
    for (std::size_t index = 0; index < 4; ++index) {
        temps[index] = {};
        temps[index].next = index + 1 < 4 ? &temps[index + 1] : nullptr;
        particles[index] = {};
        particles[index].next = index + 1 < 4 ? &particles[index + 1] : nullptr;
    }
    gpTempEntFree = temps;
    gpTempEntActive = nullptr;
    free_particles = particles;
    active_particles = nullptr;
    gpActiveTracers = nullptr;
}

void Api() {
    for (const float invalid : {std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        for (int axis = 0; axis < 3; ++axis) {
            Reset();
            float origin[3] = {10, 20, 30};
            origin[axis] = invalid;
            efx.R_SparkEffect(origin, 2, -100, 100);
            Check(gpActiveTracers == nullptr && gpTempEntActive == nullptr &&
                free_particles == particles && gpTempEntFree == temps,
                "Invalid effect coordinates must not consume particle or temporary-entity pools");
            float direction[3] = {1, 2, 3};
            origin[axis] = 30;
            direction[axis] = invalid;
            Check(efx.R_TempSprite(origin, direction, 0.5f, 1, 5, 0, 1, 1, 0) == nullptr &&
                gpTempEntActive == nullptr, "Reject invalid velocity before allocating a sprite");
            direction[axis] = 1;
            Check(efx.R_TempSprite(origin, direction, invalid, 1, 5, 0, 1, 1, 0) == nullptr &&
                gpTempEntActive == nullptr, "Reject invalid scalar parameters before allocating a sprite");
        }
    }
    Reset();
    float origin[3] = {10, 20, 30};
    efx.R_SparkEffect(origin, 2, -100, 100);
    Check(gpActiveTracers == &particles[1] && particles[1].next == &particles[0] &&
        particles[0].next == nullptr && gpTempEntActive == &temps[0], "Finite sparks retain both effect types");
    Check(particles[0].org[2] == 30 && particles[0].vel[0] == -100 &&
        temps[0].entity.origin[0] == 10 && temps[0].entity.curstate.scale == 0.5f &&
        temps[0].entity.curstate.rendermode == kRenderGlow, "Finite effect parameters remain unchanged");
}

void Death(particle_t*) {
    ++deaths;
}

void Particles() {
    Reset();
    auto* valid = R_AllocParticle(nullptr);
    auto* bad = R_AllocParticle(nullptr);
    valid->die = 20;
    bad->die = 20;
    bad->org[1] = std::numeric_limits<float>::quiet_NaN();
    bad->deathfunc = Death;
    R_FreeDeadParticles(&active_particles);
    Check(active_particles == valid && valid->next == nullptr && free_particles == bad && deaths == 1,
        "Corrupted particles retire normally without removing live neighbors");
    R_FreeDeadParticles(&active_particles);
    Check(deaths == 1, "Particle death callback runs exactly once");
    valid->vel[2] = std::numeric_limits<float>::infinity();
    R_FreeDeadParticles(&active_particles);
    Check(active_particles == nullptr && free_particles == valid, "Invalid velocities also retire before simulation");
}

void Temporary() {
    Reset();
    float origin[3] = {0, 0, std::numeric_limits<float>::infinity()};
    Check(CL_TempEntAlloc(origin, &sprite) == nullptr && CL_TempEntAllocNoModel(origin) == nullptr &&
        CL_TempEntAllocHigh(origin, &sprite) == nullptr && gpTempEntFree == temps && gpTempEntActive == nullptr,
        "Shared allocators reject invalid origins without consuming or evicting entities");
    origin[2] = 1;
    Check(CL_TempEntAlloc(origin, &sprite) == &temps[0], "Finite direct allocation remains available");
    temps[0].next = &temps[1];
    temps[1].next = &temps[2];
    temps[2].next = nullptr;
    gpTempEntFree = &temps[3];
    temps[1].entity.baseline.origin[0] = std::numeric_limits<float>::infinity();
    cl.worldmodel = &sprite;
    CL_TempEntUpdate();
    Check(updates == 1 && gpTempEntFree == &temps[1] && temps[1].next == &temps[3],
        "Invalid callback output is reclaimed before the next client simulation");
}

void Visibility() {
    cl_entity_t entity{};
    Check(CL_AddVisibleEntity(0, &entity) == 1 && cl_numvisedicts == 1,
        "Finite entities remain visible");
    entity.origin[0] = std::numeric_limits<float>::infinity();
    Check(CL_AddVisibleEntity(0, &entity) == 0 && cl_numvisedicts == 1,
        "Invalid callback positions cannot poison the visible entity list");
    entity.origin[0] = 0;
    entity.attachment[0][0] = std::numeric_limits<float>::infinity();
    Check(CL_AddVisibleEntity(0, &entity) == 1 && cl_numvisedicts == 2,
        "A bad attachment must not hide an otherwise valid model");
}

void Beams() {
    cl.time = 10;
    attachmentEntity.model = &sprite;
    tri.RenderMode = [](int) {};
    tri.SpriteTexture = [](model_s*, int) {
        return 1;
    };
    tri.Color4f = [](float, float, float, float) {};
    tri.Begin = [](int) {};
    tri.End = []() {};
    BEAM beam{};
    beam.modelIndex = 1;
    beam.frameCount = 8;
    beam.die = 20;
    beam.type = TE_BEAMPOINTS;
    beam.target[0] = 100;
    beam.flags = FBEAM_STARTENTITY;
    attachmentEntity.attachment[0][0] = std::numeric_limits<float>::infinity();
    R_BeamDraw(&beam, 0.01f);
    Check(beamDraws == 0, "An invalid animated attachment cannot reach beam segmentation or drawing");
    attachmentEntity.attachment[0][0] = 10;
    R_BeamDraw(&beam, 0.01f);
    Check(beamDraws == 1 && beam.source[0] == 10 && beam.delta[0] == 90 && beam.segments == 9,
        "A valid animated attachment recovers the beam with its original segmentation");
    float start[3] = {0, 0, 0};
    float end[3] = {1.0e20f, 0, 0};
    R_BeamSetup(&beam, start, end, 1, 1, 1, 1, 1, 1);
    Check(beam.segments > 0 && beam.segments <= 1024,
        "Finite long beams must saturate existing tessellation limits without undefined integer conversion");
}

void Ramp() {
    for (const auto type : {pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2}) {
        for (const float invalid : {std::numeric_limits<float>::quiet_NaN(),
            -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()}) {
            Reset();
            cl.oldtime = cl.time;
            auto* valid = R_AllocParticle(nullptr);
            auto* bad = R_AllocParticle(nullptr);
            valid->die = 20;
            bad->die = 20;
            bad->type = type;
            bad->ramp = invalid;
            R_DrawParticles();
            Check(active_particles == valid && valid->next == nullptr && free_particles == bad,
                "The actual particle simulation must reject nonfinite ramps before integer indexing");
        }
    }
}

void Reuse() {
    Reset();
    auto* bad = R_AllocParticle(nullptr);
    bad->vel[0] = std::numeric_limits<float>::infinity();
    bad->ramp = std::numeric_limits<float>::quiet_NaN();
    bad->die = 20;
    R_FreeDeadParticles(&active_particles);
    cl_entity_t entity{};
    R_EntityParticles(&entity); // Legacy path initializes position/type but inherits velocity and ramp.
    cl.oldtime = cl.time;
    R_DrawParticles();
    int remaining = 0;
    for (auto* particle = active_particles; particle; particle = particle->next) {
        Check(std::isfinite(particle->org[0]) && std::isfinite(particle->vel[0]) && std::isfinite(particle->ramp),
            "Reclaimed nonfinite fields cannot contaminate new entity particles");
        ++remaining;
    }
    Check(remaining == 4, "Reusing a corrupt pool slot must not remove a later finite effect");
}

int main(int argc, char** argv) {
    Check(argc == 2, "Specify scenario");
    if (std::strcmp(argv[1], "api") == 0) {
        Api();
    } else if (std::strcmp(argv[1], "particles") == 0) {
        Particles();
    } else if (std::strcmp(argv[1], "temporary") == 0) {
        Temporary();
    } else if (std::strcmp(argv[1], "visibility") == 0) {
        Visibility();
    } else if (std::strcmp(argv[1], "beams") == 0) {
        Beams();
    } else if (std::strcmp(argv[1], "ramp") == 0) {
        Ramp();
    } else if (std::strcmp(argv[1], "reuse") == 0) {
        Reuse();
    } else {
        Check(false, "Unknown scenario");
    }
    std::puts("PASS shared effects safety");
}
