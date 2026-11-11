#include "A3dSource.h"
#include "resman.h"
#include "A3d3.h"
#include "apimapper.h"
#include "dal_a2d.h"
#include "a3dclsfc.h"
#include "a2d.h"
#include "dalinfo.h"
#include <cstddef>
#include <cstdio>
#include <type_traits>

// Fixed legacy records are exchanged by public interfaces, including on LP64.
static_assert(sizeof(DWORD) == 4 && sizeof(LONG) == 4 && sizeof(HRESULT) == 4);
static_assert(std::is_signed_v<HRESULT>);
static_assert(sizeof(GUID) == 16);
static_assert(offsetof(GUID, Data2) == 4 && offsetof(GUID, Data3) == 6 && offsetof(GUID, Data4) == 8);
static_assert(sizeof(a3d_source_channel_t) == 16);
static_assert(sizeof(a3d_source_reflection_state_t) == 56);
static_assert(sizeof(A3DCTRL_SRC_SUPER) == 1036);
static_assert(offsetof(A3DCTRL_SRC_SUPER, Reflections) == 76);
static_assert(offsetof(A3DCTRL_SRC_SUPER, ReservedBackendData) == 972);
static_assert(sizeof(A3DCAPS_SOFTWARE) == 40 && sizeof(A3DCAPS_HARDWARE) == 36);
// Expectations from the independent SDK 10.0.26100.0 x64/Win32 executable receipts.
static_assert(sizeof(GUID) == 16 && alignof(GUID) == 4);
static_assert(offsetof(GUID, Data1) == 0);
static_assert(offsetof(GUID, Data2) == 4);
static_assert(offsetof(GUID, Data3) == 6);
static_assert(offsetof(GUID, Data4) == 8);
static_assert(sizeof(WAVEFORMAT) == 14 && alignof(WAVEFORMAT) == 1);
static_assert(sizeof(PCMWAVEFORMAT) == 16 && alignof(PCMWAVEFORMAT) == 1);
static_assert(sizeof(WAVEFORMATEX) == 18 && alignof(WAVEFORMATEX) == 1);
static_assert(offsetof(WAVEFORMATEX, wFormatTag) == 0);
static_assert(offsetof(WAVEFORMATEX, nChannels) == 2);
static_assert(offsetof(WAVEFORMATEX, nSamplesPerSec) == 4);
static_assert(offsetof(WAVEFORMATEX, nAvgBytesPerSec) == 8);
static_assert(offsetof(WAVEFORMATEX, nBlockAlign) == 12);
static_assert(offsetof(WAVEFORMATEX, wBitsPerSample) == 14);
static_assert(offsetof(WAVEFORMATEX, cbSize) == 16);
static_assert(sizeof(WAVEFORMATEXTENSIBLE) == 40 && alignof(WAVEFORMATEXTENSIBLE) == 1);
static_assert(offsetof(WAVEFORMATEXTENSIBLE, Format) == 0);
static_assert(offsetof(WAVEFORMATEXTENSIBLE, Samples) == 18);
static_assert(offsetof(WAVEFORMATEXTENSIBLE, dwChannelMask) == 20);
static_assert(offsetof(WAVEFORMATEXTENSIBLE, SubFormat) == 24);
static_assert(sizeof(DSCAPS) == 96 && alignof(DSCAPS) == 4);
static_assert(offsetof(DSCAPS, dwFreeHw3DAllBuffers) == 56);
static_assert(offsetof(DSCAPS, dwTotalHwMemBytes) == 68);
static_assert(offsetof(DSCAPS, dwReserved2) == 92);
static_assert(sizeof(DSBCAPS) == 20 && alignof(DSBCAPS) == 4);
static_assert(offsetof(DSBCAPS, dwUnlockTransferRate) == 12);
static_assert(offsetof(DSBCAPS, dwPlayCpuOverhead) == 16);
static_assert(sizeof(D3DVECTOR) == 12 && alignof(D3DVECTOR) == 4);
static_assert(sizeof(DS3DBUFFER) == 64 && alignof(DS3DBUFFER) == 4);
static_assert(sizeof(DS3DLISTENER) == 64 && alignof(DS3DLISTENER) == 4);
static_assert(offsetof(DS3DBUFFER, vPosition) == 4);
static_assert(offsetof(DS3DBUFFER, vConeOrientation) == 36);
static_assert(offsetof(DS3DBUFFER, lConeOutsideVolume) == 48);
static_assert(offsetof(DS3DBUFFER, dwMode) == 60);
static_assert(offsetof(DS3DLISTENER, vOrientFront) == 28);
static_assert(offsetof(DS3DLISTENER, flDistanceFactor) == 52);
static_assert(offsetof(DS3DLISTENER, flDopplerFactor) == 60);
static_assert(sizeof(DSBUFFERDESC) == (sizeof(void*) == 8 ? 40 : 36) && alignof(DSBUFFERDESC) == (sizeof(void*) == 8 ? 8 : 4));
static_assert(offsetof(DSBUFFERDESC, lpwfxFormat) == 16);
static_assert(offsetof(DSBUFFERDESC, guid3DAlgorithm) == (sizeof(void*) == 8 ? 24 : 20));
static_assert(sizeof(DSBUFFERDESC1) == (sizeof(void*) == 8 ? 24 : 20) && alignof(DSBUFFERDESC1) == (sizeof(void*) == 8 ? 8 : 4));
static_assert(offsetof(DSBUFFERDESC1, lpwfxFormat) == 16);
static_assert(sizeof(DSBPOSITIONNOTIFY) == (sizeof(void*) == 8 ? 16 : 8) && alignof(DSBPOSITIONNOTIFY) == (sizeof(void*) == 8 ? 8 : 4));
static_assert(offsetof(DSBPOSITIONNOTIFY, hEventNotify) == (sizeof(void*) == 8 ? 8 : 4));

// Retained pointer-bearing A3D records use natural pointer alignment; the
// public scalar/tag lanes remain 32 bits. These are not Win32-size aliases.
static_assert(sizeof(A3dSourceReflection) == (sizeof(void*) == 8 ? 88 : 84));
static_assert(offsetof(A3dSourceReflection, dwTag) == sizeof(void*));
static_assert(sizeof(a3d_platform_buffer_desc_t) == (sizeof(void*) == 8 ? 24 : 20));
static_assert(offsetof(a3d_platform_buffer_desc_t, lpwfxFormat) == 16);
static_assert(sizeof(a3d_primitive_t) == 96);
static_assert(sizeof(a3d_geometry_payload_t) == 96);
static_assert(sizeof(a3d_geometry_command_t) == (sizeof(void*) == 8 ? 120 : 108));
static_assert(offsetof(a3d_geometry_command_t, m_Payload) == (sizeof(void*) == 8 ? 8 : 4));
static_assert(offsetof(a3d_geometry_command_t, m_pOpeningFactor) == (sizeof(void*) == 8 ? 104 : 100));
static_assert(sizeof(a3d_mix_input_t) == (sizeof(void*) == 8 ? 40 : 20));
static_assert(offsetof(a3d_mix_input_t, pCurrent) == (sizeof(void*) == 8 ? 24 : 12));
static_assert(sizeof(a3d_sound_buffer_node_t) == 3 * sizeof(void*));
static_assert(sizeof(a3d_render_stream_node_t) == 3 * sizeof(void*));
static_assert(sizeof(a3d_sound_buffer_list_t) == (sizeof(void*) == 8 ? 48 : 24));
static_assert(sizeof(a3d_render_stream_list_t) == (sizeof(void*) == 8 ? 48 : 24));

#define RECORD(type) std::printf(#type " size=%zu align=%zu\n", sizeof(type), alignof(type))
#define FIELD(type, field) std::printf(#type "." #field "=%zu\n", offsetof(type, field))
int main() {
    RECORD(GUID);
    FIELD(GUID, Data1);
    FIELD(GUID, Data2);
    FIELD(GUID, Data3);
    FIELD(GUID, Data4);
    RECORD(WAVEFORMAT);
    RECORD(PCMWAVEFORMAT);
    RECORD(WAVEFORMATEX);
    FIELD(WAVEFORMATEX, wFormatTag);
    FIELD(WAVEFORMATEX, nChannels);
    FIELD(WAVEFORMATEX, nSamplesPerSec);
    FIELD(WAVEFORMATEX, nAvgBytesPerSec);
    FIELD(WAVEFORMATEX, nBlockAlign);
    FIELD(WAVEFORMATEX, wBitsPerSample);
    FIELD(WAVEFORMATEX, cbSize);
    RECORD(WAVEFORMATEXTENSIBLE);
    FIELD(WAVEFORMATEXTENSIBLE, Format);
    FIELD(WAVEFORMATEXTENSIBLE, Samples);
    FIELD(WAVEFORMATEXTENSIBLE, dwChannelMask);
    FIELD(WAVEFORMATEXTENSIBLE, SubFormat);
    RECORD(DSCAPS);
    FIELD(DSCAPS, dwFreeHw3DAllBuffers);
    FIELD(DSCAPS, dwTotalHwMemBytes);
    FIELD(DSCAPS, dwReserved2);
    RECORD(DSBCAPS);
    FIELD(DSBCAPS, dwUnlockTransferRate);
    FIELD(DSBCAPS, dwPlayCpuOverhead);
    RECORD(D3DVECTOR);
    RECORD(DS3DBUFFER);
    RECORD(DS3DLISTENER);
    FIELD(DS3DBUFFER, vPosition);
    FIELD(DS3DBUFFER, vConeOrientation);
    FIELD(DS3DBUFFER, lConeOutsideVolume);
    FIELD(DS3DBUFFER, dwMode);
    FIELD(DS3DLISTENER, vOrientFront);
    FIELD(DS3DLISTENER, flDistanceFactor);
    FIELD(DS3DLISTENER, flDopplerFactor);
    RECORD(DSBUFFERDESC);
    FIELD(DSBUFFERDESC, lpwfxFormat);
    FIELD(DSBUFFERDESC, guid3DAlgorithm);
    RECORD(DSBUFFERDESC1);
    FIELD(DSBUFFERDESC1, lpwfxFormat);
    RECORD(DSBPOSITIONNOTIFY);
    FIELD(DSBPOSITIONNOTIFY, hEventNotify);
    std::puts("fixed_scalar_records passed");
    return 0;
}
