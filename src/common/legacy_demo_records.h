#ifndef CSE_LEGACY_DEMO_RECORDS_H
#define CSE_LEGACY_DEMO_RECORDS_H

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error The retained Win32 demo codec requires a little-endian target
#endif

namespace cse {
namespace demo {

constexpr std::size_t LegacyDemoPathBytes = 260;
constexpr std::size_t LegacyDemoDescriptionBytes = 64;

// Demo protocol 5 uses these Win32 records on every target. Native pointers,
// host path limits and compiler-selected padding never determine file strides.
#pragma pack(push, 4)
struct LegacyDemoHeader {
    char szFileStamp[8];
    std::int32_t nDemoProtocol;
    std::int32_t nNetProtocolVersion;
    char szMapName[LegacyDemoPathBytes];
    char szDllDir[LegacyDemoPathBytes];
    std::uint32_t mapCRC;
    std::int32_t nDirectoryOffset;
};

struct LegacyDemoEntry {
    std::int32_t nEntryType;
    char szDescription[LegacyDemoDescriptionBytes];
    std::int32_t nFlags;
    std::int32_t nCDTrack;
    float fTrackTime;
    std::int32_t nFrames;
    std::int32_t nOffset;
    std::int32_t nFileLength;
};

struct LegacyRefParams {
    float vieworg[3];
    float viewangles[3];
    float forward[3];
    float right[3];
    float up[3];
    float frametime;
    float time;
    std::int32_t intermission;
    std::int32_t paused;
    std::int32_t spectator;
    std::int32_t onground;
    std::int32_t waterlevel;
    float simvel[3];
    float simorg[3];
    float viewheight[3];
    float idealpitch;
    float cl_viewangles[3];
    std::int32_t health;
    float crosshairangle[3];
    float viewsize;
    float punchangle[3];
    std::int32_t maxclients;
    std::int32_t viewentity;
    std::int32_t playernum;
    std::int32_t max_entities;
    std::int32_t demoplayback;
    std::int32_t hardware;
    std::int32_t smoothing;
    std::uint32_t cmdPointerPlaceholder;
    std::uint32_t movevarsPointerPlaceholder;
    std::int32_t viewport[4];
    std::int32_t nextView;
    std::int32_t onlyClientDraw;
};

struct LegacyDemoCommand {
    std::int16_t lerp_msec;
    std::uint8_t msec;
    std::uint8_t paddingAfterMsec;
    float viewangles[3];
    float forwardmove;
    float sidemove;
    float upmove;
    std::uint8_t lightlevel;
    std::uint8_t paddingAfterLight;
    std::uint16_t buttons;
    std::uint8_t impulse;
    std::uint8_t weaponselect;
    std::uint8_t paddingBeforeImpact[2];
    std::int32_t impact_index;
    float impact_position[3];
};

struct LegacyDemoMoveVars {
    float gravity;
    float stopspeed;
    float maxspeed;
    float spectatormaxspeed;
    float accelerate;
    float airaccelerate;
    float wateraccelerate;
    float friction;
    float edgefriction;
    float waterfriction;
    float entgravity;
    float bounce;
    float stepsize;
    float maxvelocity;
    float zmax;
    float waveHeight;
    std::int32_t footsteps;
    char skyName[32];
    float rollangle;
    float rollspeed;
    float skycolor_r;
    float skycolor_g;
    float skycolor_b;
    float skyvec_x;
    float skyvec_y;
    float skyvec_z;
};

struct LegacyDemoFrame {
    float timestamp;
    LegacyRefParams rp;
    LegacyDemoCommand cmd;
    LegacyDemoMoveVars movevars;
    float view[3];
    std::int32_t viewmodel;
};
#pragma pack(pop)

static_assert(sizeof(float) == 4, "Win32 demos use four-byte IEEE float storage");
static_assert(sizeof(LegacyDemoHeader) == 544, "Win32 demo header stride changed");
static_assert(sizeof(LegacyDemoEntry) == 92, "Win32 demo directory stride changed");
static_assert(sizeof(LegacyRefParams) == 232, "Win32 ref_params stride changed");
static_assert(sizeof(LegacyDemoCommand) == 52, "Win32 usercmd stride changed");
static_assert(sizeof(LegacyDemoMoveVars) == 132, "Win32 movevars stride changed");
static_assert(sizeof(LegacyDemoFrame) == 436, "Win32 demo frame stride changed");
static_assert(offsetof(LegacyDemoFrame, cmd) == 236, "Win32 command position changed");
static_assert(offsetof(LegacyDemoFrame, movevars) == 288, "Win32 movevars position changed");
static_assert(offsetof(LegacyRefParams, cmdPointerPlaceholder) == 200, "Win32 pointer slot changed");

namespace detail {
template<typename Destination, typename Source>
inline void CopyDemoBits(Destination &destination, const Source &source) {
    static_assert(sizeof(destination) == sizeof(source), "Native demo value width differs from Win32");
    std::memcpy(&destination, &source, sizeof(destination));
}

template<typename Destination, typename Source>
inline void CopyRefValues(Destination &destination, const Source &source) {
#define CSE_COPY_DEMO_VALUE(name) CopyDemoBits(destination.name, source.name)
    CSE_COPY_DEMO_VALUE(vieworg);
    CSE_COPY_DEMO_VALUE(viewangles);
    CSE_COPY_DEMO_VALUE(forward);
    CSE_COPY_DEMO_VALUE(right);
    CSE_COPY_DEMO_VALUE(up);
    CSE_COPY_DEMO_VALUE(frametime);
    CSE_COPY_DEMO_VALUE(time);
    CSE_COPY_DEMO_VALUE(intermission);
    CSE_COPY_DEMO_VALUE(paused);
    CSE_COPY_DEMO_VALUE(spectator);
    CSE_COPY_DEMO_VALUE(onground);
    CSE_COPY_DEMO_VALUE(waterlevel);
    CSE_COPY_DEMO_VALUE(simvel);
    CSE_COPY_DEMO_VALUE(simorg);
    CSE_COPY_DEMO_VALUE(viewheight);
    CSE_COPY_DEMO_VALUE(idealpitch);
    CSE_COPY_DEMO_VALUE(cl_viewangles);
    CSE_COPY_DEMO_VALUE(health);
    CSE_COPY_DEMO_VALUE(crosshairangle);
    CSE_COPY_DEMO_VALUE(viewsize);
    CSE_COPY_DEMO_VALUE(punchangle);
    CSE_COPY_DEMO_VALUE(maxclients);
    CSE_COPY_DEMO_VALUE(viewentity);
    CSE_COPY_DEMO_VALUE(playernum);
    CSE_COPY_DEMO_VALUE(max_entities);
    CSE_COPY_DEMO_VALUE(demoplayback);
    CSE_COPY_DEMO_VALUE(hardware);
    CSE_COPY_DEMO_VALUE(smoothing);
    CSE_COPY_DEMO_VALUE(viewport);
    CSE_COPY_DEMO_VALUE(nextView);
    CSE_COPY_DEMO_VALUE(onlyClientDraw);
#undef CSE_COPY_DEMO_VALUE
}

template<typename Destination, typename Source>
inline void CopyCommandValues(Destination &destination, const Source &source) {
#define CSE_COPY_DEMO_VALUE(name) CopyDemoBits(destination.name, source.name)
    CSE_COPY_DEMO_VALUE(lerp_msec);
    CSE_COPY_DEMO_VALUE(msec);
    CSE_COPY_DEMO_VALUE(viewangles);
    CSE_COPY_DEMO_VALUE(forwardmove);
    CSE_COPY_DEMO_VALUE(sidemove);
    CSE_COPY_DEMO_VALUE(upmove);
    CSE_COPY_DEMO_VALUE(lightlevel);
    CSE_COPY_DEMO_VALUE(buttons);
    CSE_COPY_DEMO_VALUE(impulse);
    CSE_COPY_DEMO_VALUE(weaponselect);
    CSE_COPY_DEMO_VALUE(impact_index);
    CSE_COPY_DEMO_VALUE(impact_position);
#undef CSE_COPY_DEMO_VALUE
}

template<typename Destination, typename Source>
inline void CopyMoveValues(Destination &destination, const Source &source) {
#define CSE_COPY_DEMO_VALUE(name) CopyDemoBits(destination.name, source.name)
    CSE_COPY_DEMO_VALUE(gravity);
    CSE_COPY_DEMO_VALUE(stopspeed);
    CSE_COPY_DEMO_VALUE(maxspeed);
    CSE_COPY_DEMO_VALUE(spectatormaxspeed);
    CSE_COPY_DEMO_VALUE(accelerate);
    CSE_COPY_DEMO_VALUE(airaccelerate);
    CSE_COPY_DEMO_VALUE(wateraccelerate);
    CSE_COPY_DEMO_VALUE(friction);
    CSE_COPY_DEMO_VALUE(edgefriction);
    CSE_COPY_DEMO_VALUE(waterfriction);
    CSE_COPY_DEMO_VALUE(entgravity);
    CSE_COPY_DEMO_VALUE(bounce);
    CSE_COPY_DEMO_VALUE(stepsize);
    CSE_COPY_DEMO_VALUE(maxvelocity);
    CSE_COPY_DEMO_VALUE(zmax);
    CSE_COPY_DEMO_VALUE(waveHeight);
    CSE_COPY_DEMO_VALUE(footsteps);
    CSE_COPY_DEMO_VALUE(skyName);
    CSE_COPY_DEMO_VALUE(rollangle);
    CSE_COPY_DEMO_VALUE(rollspeed);
    CSE_COPY_DEMO_VALUE(skycolor_r);
    CSE_COPY_DEMO_VALUE(skycolor_g);
    CSE_COPY_DEMO_VALUE(skycolor_b);
    CSE_COPY_DEMO_VALUE(skyvec_x);
    CSE_COPY_DEMO_VALUE(skyvec_y);
    CSE_COPY_DEMO_VALUE(skyvec_z);
#undef CSE_COPY_DEMO_VALUE
}

template<typename Destination, typename Source>
inline void CopyFrameValues(Destination &destination, const Source &source) {
    CopyDemoBits(destination.timestamp, source.timestamp);
    CopyRefValues(destination.rp, source.rp);
    CopyCommandValues(destination.cmd, source.cmd);
    CopyMoveValues(destination.movevars, source.movevars);
    CopyDemoBits(destination.view, source.view);
    CopyDemoBits(destination.viewmodel, source.viewmodel);
}
} // namespace detail

template<typename NativeFrame>
inline LegacyDemoFrame EncodeLegacyDemoFrame(const NativeFrame &native) {
    LegacyDemoFrame disk{};
    detail::CopyFrameValues(disk, native);
    return disk;
}

template<typename NativeFrame>
inline bool DecodeLegacyDemoFrame(const void *bytes, std::size_t count, NativeFrame &native) {
    if (!bytes || count != sizeof(LegacyDemoFrame)) {
        return false;
    }
    LegacyDemoFrame disk;
    std::memcpy(&disk, bytes, sizeof(disk));
    native = NativeFrame{};
    detail::CopyFrameValues(native, disk);
    native.rp.cmd = &native.cmd;
    native.rp.movevars = &native.movevars;
    return true;
}

template<typename NativeFrame>
inline void CopyNativeDemoFrame(const void *bytes, NativeFrame &native) {
    std::memcpy(&native, bytes, sizeof(native));
    native.rp.cmd = &native.cmd;
    native.rp.movevars = &native.movevars;
}

} // namespace demo
} // namespace cse

#endif
