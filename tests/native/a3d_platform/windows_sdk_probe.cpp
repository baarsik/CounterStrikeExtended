// Run against actual Windows SDK headers, independently of platform_*.h.
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <dsound.h>
#include <cstddef>
#include <cstdio>

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
    return 0;
}
