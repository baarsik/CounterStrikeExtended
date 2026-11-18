// snd_mem.c: sound caching
#include "sound.h"
#include "client.h"
#include "common.h"
#include "conprint.h"
#include "cvar.h"
#include "sys.h"
#include "zone.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <memory>

#define CHAR_STREAM		'*'		// as one of 1st 2 chars in name, indicates streaming wav data
#define CHAR_USERVOX	'?'		// as one of 1st 2 chars in name, indicates user realtime voice data

extern cvar_t hisound;
extern cvar_t loadas8bit;
extern cvar_t fs_precache_timings;

sfxcache_t* VoiceSE_GetSFXCache( sfx_t* sound );
qboolean GetWavinfo( char* name, byte* wav, int length, wavinfo_t* info );
void ResampleSfx( sfx_t* sound, int inputRate, int inputWidth, byte* data, int dataBytes );
extern "C" void Wavstream_Close( int index );

byte* iff_data;
byte* iff_end;
byte* last_chunk;
/*
===============================================================================

WAV loading

===============================================================================
*/


byte* data_p;
int iff_chunk_len;

static constexpr int WAV_CHUNK_HEADER_BYTES = 8;
static constexpr int WAV_RIFF_HEADER_BYTES = 12;
static constexpr int WAV_PCM_FORMAT_BYTES = 16;
static constexpr int WAV_CUE_BYTES = 28;
static constexpr int WAV_LOOP_LIST_BYTES = 24;
static constexpr int WAV_MAX_CHUNK_BYTES = 100 * 1024 * 1024;

static int ResampledSampleCount(int samples, int loopStart, float stepScale) {
    if (stepScale == 1.0f) {
        return samples;
    }

    const float count = static_cast<float>(samples) / stepScale;
    if (!(count >= 0.0f) || static_cast<double>(count) > INT_MAX) {
        return -1;
    }
    if (loopStart < 0) {
        return static_cast<int>(count);
    }

    // Downsampling can truncate the entire loop. Retain a sample at its resampled cue.
    const float loop = static_cast<float>(loopStart) / stepScale;
    if (!(loop >= 0.0f) || static_cast<double>(loop) >= INT_MAX) {
        return -1;
    }
    return std::max(static_cast<int>(count), static_cast<int>(loop) + 1);
}

static bool WavChunkHasBytes(int bytes) {
    return data_p && bytes >= 0 && iff_chunk_len >= bytes && iff_end - data_p >= WAV_CHUNK_HEADER_BYTES &&
           bytes <= iff_end - data_p - WAV_CHUNK_HEADER_BYTES;
}

int GetLittleLong() {
    int val;

    val = (int)((std::uint32_t)data_p[0] | ((std::uint32_t)data_p[1] << 8) | ((std::uint32_t)data_p[2] << 16) |
                ((std::uint32_t)data_p[3] << 24));
    data_p += 4;
    return val;
}

short GetLittleShort()
{
	short val;

	val = (short)( data_p[0] + ( data_p[1] << 8 ) );
	data_p += 2;
	return val;
}

void FindNextChunk(char *name) {
    while (last_chunk < iff_end) {
        if (iff_end - last_chunk < WAV_CHUNK_HEADER_BYTES) {
            break;
        }
        data_p = last_chunk + 4;
        iff_chunk_len = GetLittleLong();
        if (iff_chunk_len < 0) {
            break;
        }

        if (iff_chunk_len > WAV_MAX_CHUNK_BYTES) {
            break;
        }

        data_p = last_chunk;
        auto remaining = iff_end - data_p - WAV_CHUNK_HEADER_BYTES;
        last_chunk = iff_end;
        if (iff_chunk_len <= remaining) {
            last_chunk = data_p + WAV_CHUNK_HEADER_BYTES + iff_chunk_len;
            if ((iff_chunk_len & 1) && last_chunk < iff_end) {
                // Shipped Cool Edit WAVs such as death6.wav can omit the data pad.
                bool unpaddedList = false;
                if (!Q_strncmp((char *)data_p, "data", 4) && iff_end - last_chunk >= WAV_CHUNK_HEADER_BYTES &&
                    !Q_strncmp((char *)last_chunk, "LIST", 4)) {
                    byte *chunk = data_p;
                    data_p = last_chunk + 4;
                    int listBytes = GetLittleLong();
                    data_p = chunk;
                    unpaddedList = listBytes >= 4 && listBytes <= iff_end - last_chunk - WAV_CHUNK_HEADER_BYTES;
                }
                if (!unpaddedList) {
                    ++last_chunk;
                }
            }
        }
        if (!Q_strncmp((char *)data_p, name, 4)) {
            return;
        }
    }

    // didn't find the chunk
    data_p = NULL;
}

void FindChunk( char* name )
{
	last_chunk = iff_data;
	FindNextChunk( name );
}

sfxcache_t *S_LoadStreamSound(sfx_t *s, channel_t *ch) {
    if (!ch || cl.fPrecaching) {
        return nullptr;
    }

    const int channelIndex = static_cast<int>(ch - channels);
    wavstream_t *stream = &wavstreams[channelIndex];
    const bool firstLoad = !stream->hFile;
    if (!firstLoad) {
        if (auto *cache = static_cast<sfxcache_t *>(Cache_Check(&s->cache))) {
            return cache;
        }
    }

    const auto closeStream = [s, channelIndex](wavstream_t *) {
        Wavstream_Close(channelIndex);
        if (s->cache.data) {
            Cache_Free(&s->cache);
        }
    };
    std::unique_ptr<wavstream_t, decltype(closeStream)> streamCleanup(stream, closeStream);

    char wavname[SOUND_NAME_BYTES];
    Q_strncpy(wavname, s->name + 1, sizeof(wavname) - 1);
    wavname[sizeof(wavname) - 1] = 0;
    char namebuffer[256];
    Q_strcpy(namebuffer, "sound");
    if (wavname[0] != '/') {
        strncat(namebuffer, "/", sizeof(namebuffer) - strlen(namebuffer) - 1);
    }
    strncat(namebuffer, wavname, sizeof(namebuffer) - strlen(namebuffer) - 1);

    // An evicted live cache must be rebuilt from the same chunk, including its already played samples.
    const int readOffset = firstLoad ? 0 : stream->lastposloaded;
    const int readLimit = firstLoad ? WAV_STREAM_CHUNK_BYTES : stream->csamplesinmem;
    int bytesRead;
    byte *data = COM_LoadFileLimit(namebuffer, readOffset, readLimit, &bytesRead, &stream->hFile);
    if (!data || (!firstLoad && bytesRead != readLimit)) {
        Con_DPrintf("S_LoadStreamSound: Couldn't load %s\n", namebuffer);
        return nullptr;
    }

    wavinfo_t info = stream->info;
    if (firstLoad) {
        if (!GetWavinfo(s->name, data, bytesRead, &info)) {
            Con_DPrintf("Failed loading %s\n", wavname);
            return nullptr;
        }

        if (info.channels != 1) {
            Con_DPrintf("%s is a stereo sample\n", wavname);
            return nullptr;
        }

        if (info.width != 1) {
            Con_DPrintf("%s is a 16 bit sample\n", wavname);
            return nullptr;
        }

        if (shm && info.rate != shm->speed) {
            Con_DPrintf("%s ignored, not stored at playback sample rate!\n", wavname);
            return nullptr;
        }

        stream->lastposloaded = info.dataofs;
        stream->info = info;
    }

    // The temporary read buffer can move or evict the cache.
    auto *cache = static_cast<sfxcache_t *>(Cache_Check(&s->cache));
    if (!cache) {
        // Size for the largest chunk, including when the first read is mostly header bytes.
        const int cacheBytes = static_cast<int>(sizeof(sfxcache_t)) + std::min(info.samples, WAV_STREAM_CHUNK_BYTES);
        cache = static_cast<sfxcache_t *>(Cache_Alloc(&s->cache, cacheBytes, wavname));
        if (!cache) {
            return nullptr;
        }
    }

    const int pcmOffset = firstLoad ? info.dataofs : 0;
    const int pcmBytes = bytesRead - pcmOffset;
    stream->csamplesinmem = std::min(pcmBytes, info.samples);
    cache->length = stream->csamplesinmem;
    cache->loopstart = info.loopstart;
    cache->speed = info.rate;
    cache->width = info.width;
    cache->stereo = info.channels;
    ResampleSfx(s, info.rate, info.width, data + pcmOffset, pcmBytes);

    (void)streamCleanup.release();
    return cache;
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4701 )
#endif
/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound(sfx_t *s, channel_t *ch) {
    char namebuffer[256];
    byte stackbuf[1 * 1024]; // avoid dirtying the cache heap
    byte *data;
    wavinfo_t info;
    int len;
    float stepscale;
    sfxcache_t *sc;
    FileHandle_t hFile;
    double start;

    if (s->name[0] == CHAR_STREAM) {
        return S_LoadStreamSound(s, ch);
    }

    if (s->name[0] == CHAR_USERVOX) {
        return VoiceSE_GetSFXCache(s);
    }

    // see if still in memory
    sc = (sfxcache_t *)Cache_Check(&s->cache);
    if (sc) {
        if (hisound.value || !shm || sc->speed <= shm->speed) {
            return sc;
        }
        Cache_Free(&s->cache);
    }

    if (fs_precache_timings.value) {
        start = Sys_FloatTime();
    }

    // load it in
    Q_strcpy(namebuffer, "sound");
    if (s->name[0] != '/') {
        strncat(namebuffer, "/", sizeof(namebuffer) - 1 - strlen(namebuffer));
    }
    strncat(namebuffer, s->name, sizeof(namebuffer) - 1 - strlen(namebuffer));

    data = NULL;
    hFile = FS_Open(namebuffer, "rb");
    if (hFile) {
        data = (byte *)FS_GetReadBuffer(hFile, &len);
        if (!data) {
            const unsigned int fileBytes = FS_Size(hFile);
            if (fileBytes < cse::memory::MaximumHunkPayloadBytes) {
                len = static_cast<int>(fileBytes);
                data = static_cast<byte *>(Hunk_TempAlloc(len + 1));
            }
            const bool loaded = data && FS_Read(data, len, 1, hFile) == len;
            FS_Close(hFile);
            hFile = nullptr;
            if (!loaded) {
                Con_DPrintf("S_LoadSound: Couldn't read %s\n", namebuffer);
                return nullptr;
            }
        }
    }

    if (!data) {
        namebuffer[0] = 0;
        if (s->name[0] != '/') {
            strncat(namebuffer, "/", sizeof(namebuffer) - 1 - strlen(namebuffer));
        }
        strncat(namebuffer, s->name, sizeof(namebuffer) - 1 - strlen(namebuffer));
        namebuffer[sizeof(namebuffer) - 1] = 0;

        //	Con_Printf ("loading %s\n",namebuffer);

        data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf), &len);
        if (!data) {
            Con_DPrintf("S_LoadSound: Couldn't load %s\n", namebuffer);
            return NULL;
        }
    }

    const auto releaseReadBuffer = [data](void *file) {
        FS_ReleaseReadBuffer(file, data);
        FS_Close(file);
    };
    std::unique_ptr<void, decltype(releaseReadBuffer)> fileCleanup(hFile, releaseReadBuffer);

    if (!GetWavinfo(s->name, data, len, &info)) {
        Con_DPrintf("Failed loading %s\n", s->name);
        return NULL;
    }

    if (info.channels != 1) {
        Con_DPrintf("%s is a stereo sample\n", s->name);
        return NULL;
    }

    if (!info.rate) {
        Con_DPrintf("Invalid rate: %s\n", s->name);
        return NULL;
    }

    if (info.dataofs >= len) {
        Con_DPrintf("%s has invalid data offset\n", s->name);
        return NULL;
    }

    stepscale = 1.0f;
    if (shm && info.rate != shm->speed && !(info.rate == shm->speed * 2 && hisound.value != 0.0f)) {
        stepscale = (float)info.rate / (float)shm->speed;
        if (stepscale == 0.0f) {
            Con_DPrintf("Invalid stepscale: %s\n", s->name);
            return NULL;
        }
    }

    const int samples = ResampledSampleCount(info.samples, info.loopstart, stepscale);
    constexpr auto maximumPcmBytes = cse::memory::MaximumCachePayloadBytes - sizeof(sfxcache_t);
    if (samples < 0 || static_cast<std::size_t>(samples) > maximumPcmBytes / info.width) {
        Con_DPrintf("Invalid resampled length: %s\n", s->name);
        return nullptr;
    }

    const int cacheBytes = samples * info.width + static_cast<int>(sizeof(sfxcache_t));
    sc = static_cast<sfxcache_t *>(Cache_Alloc(&s->cache, cacheBytes, s->name));
    if (!sc) {
        return NULL;
    }

    sc->length = info.samples;
    sc->loopstart = info.loopstart;
    sc->speed = info.rate;
    sc->width = info.width;
    sc->stereo = info.channels;

    ResampleSfx(s, sc->speed, sc->width, data + info.dataofs, len - info.dataofs);

    fileCleanup.reset();

    if (fs_precache_timings.value) {
        float elapsed = (float)(Sys_FloatTime() - start);
        Con_DPrintf("fs_precache_timings: loaded sound %s in time %.3f sec\n", namebuffer, elapsed);
    }

    return sc;
}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif

/*
============
GetWavinfo
============
*/
qboolean GetWavinfo(char *name, byte *wav, int wavlength, wavinfo_t *info) {
    int samples;

    if (!name || !wav || !info || wavlength < WAV_RIFF_HEADER_BYTES) {
        return FALSE;
    }

    Q_memset(info, 0, sizeof(*info));

    iff_data = wav;
    iff_end = wav + wavlength;

    // find "RIFF" chunk
    FindChunk("RIFF");
    if (!WavChunkHasBytes(4) || Q_strncmp((char *)(data_p + 8), "WAVE", 4)) {
        Con_Printf("Missing RIFF/WAVE chunks\n");
        return FALSE;
    }

    byte *riffStart = data_p;
    int riffLength = iff_chunk_len;
    if (riffLength < iff_end - data_p - WAV_CHUNK_HEADER_BYTES) {
        iff_end = data_p + WAV_CHUNK_HEADER_BYTES + riffLength;
    }

    // get "fmt " chunk
    iff_data = data_p + 12;
    // DumpChunks ();

    FindChunk("fmt ");
    if (!WavChunkHasBytes(WAV_PCM_FORMAT_BYTES) || !WavChunkHasBytes(iff_chunk_len)) {
        Con_Printf("Missing fmt chunk\n");
        return FALSE;
    }

    data_p += 8;
    if (GetLittleShort() != 1) {
        Con_Printf("Microsoft PCM format only\n");
        return FALSE;
    }

    info->channels = GetLittleShort();
    info->rate = GetLittleLong();
    data_p += 4 + 2;
    constexpr int PcmByteSampleBits = 8;
    constexpr int PcmWordSampleBits = 16;
    int bits = GetLittleShort();
    info->width = bits / PcmByteSampleBits;
    if ((bits != PcmByteSampleBits && bits != PcmWordSampleBits) || info->rate <= 0 || info->channels <= 0) {
        Con_Printf("Invalid sample width\n");
        return FALSE;
    }

    // get cue chunk
    FindChunk("cue ");
    if (data_p) {
        if (!WavChunkHasBytes(WAV_CUE_BYTES) || !WavChunkHasBytes(iff_chunk_len)) {
            return FALSE;
        }
        data_p += 32;
        info->loopstart = GetLittleLong();
        if (info->loopstart < 0) {
            return FALSE;
        }
        //		Con_Printf("loopstart=%d\n", sfx->loopstart);

        // if the next chunk is a LIST chunk, look for a cue length marker
        FindNextChunk("LIST");
        if (data_p && !WavChunkHasBytes(iff_chunk_len)) {
            return FALSE;
        }
        if (WavChunkHasBytes(WAV_LOOP_LIST_BYTES) && !Q_strncmp((char *)(data_p + 28), "mark", 4)) {
            // this is not a proper parse, but it works with cooledit...
            data_p += 24;
            int loopSamples = GetLittleLong();
            if (loopSamples < 0 || loopSamples > INT_MAX - info->loopstart) {
                return FALSE;
            }
            info->samples = info->loopstart + loopSamples;
        }
    } else {
        info->loopstart = -1;
    }

    // find data chunk
    FindChunk("data");
    if (!data_p) {
        Con_Printf("Missing data chunk\n");
        return FALSE;
    }

    if (name[0] != CHAR_STREAM && !WavChunkHasBytes(iff_chunk_len)) {
        return FALSE;
    }
    if ((std::uint64_t)(data_p - riffStart) + WAV_CHUNK_HEADER_BYTES + (std::uint64_t)iff_chunk_len >
        (std::uint64_t)riffLength + WAV_CHUNK_HEADER_BYTES) {
        return FALSE;
    }
    data_p += 4;
    samples = GetLittleLong() / info->width;

    if (info->samples) {
        if (samples < info->samples) {
            Con_Printf("Sound %s has a bad loop length\n", name);
            return FALSE;
        }
    } else {
        info->samples = samples;
    }
    if (info->loopstart >= info->samples) {
        return FALSE;
    }

    info->dataofs = (int)(data_p - wav);

    if (info->channels < 0 || info->rate < 0 || info->width < 0 || info->samples < 0) {
        Con_Printf("Invalid wav info: %s\n", name);
        return FALSE;
    }

    return TRUE;
}

/*
================
ResampleSfx
================
*/
void ResampleSfx(sfx_t *sfx, int inrate, int inwidth, byte *data, int datasize) {
    constexpr int FractionBits = 8;
    constexpr int FractionScale = 1 << FractionBits;
    int outcount;
    int srcsample;
    float stepscale;
    int i;
    int sample;
    int fracstep;
    std::uint64_t samplefrac;
    std::uint64_t lastsample;
    sfxcache_t *sc;

    sc = (sfxcache_t *)Cache_Check(&sfx->cache);
    if (!sc) {
        return;
    }

    stepscale = shm ? (float)inrate / (float)shm->speed : 1.0f; // this is usually 0.5, 1, or 2

    if (shm && stepscale == 2.0f && hisound.value) {
        outcount = sc->length;
        stepscale = 1.0f;
    } else {
        outcount = ResampledSampleCount(sc->length, sc->loopstart, stepscale);
        if (outcount < 0) {
            sc->length = 0;
            sc->loopstart = -1;
            return;
        }
        sc->length = outcount;
        if (sc->loopstart >= 0 && stepscale != 1.0f) {
            sc->loopstart = static_cast<int>(static_cast<float>(sc->loopstart) / stepscale);
        }

        if (shm) {
            sc->speed = shm->speed;
        }
    }

    sc->width = loadas8bit.value ? 1 : inwidth;
    sc->stereo = 0;

    // resample / decimate to the current source rate
    if (stepscale == 1.0f && inwidth == 1 && sc->width == 1) {
        // fast special case
        if (outcount > datasize) {
            Con_DPrintf("ResampleSfx: %s has invalid sample count: %d datasize: %d\n", sfx->name, outcount, datasize);
            memset(sc->data, 0, outcount);
            return;
        }

        for (i = 0; i < outcount; i++) {
            sc->data[i] = (byte)(data[i] - 128);
        }
        return;
    }

    // general case
    if (stepscale != 1.0f && stepscale != 2.0f) {
        Con_DPrintf("WARNING! %s is causing runtime sample conversion!\n", sfx->name);
    }

    fracstep = static_cast<int>(stepscale * FractionScale);

    if (outcount > 0) {
        lastsample = static_cast<std::uint64_t>(outcount - 1) * static_cast<unsigned int>(fracstep);
        // Source positions index complete samples, not bytes. Keep the product
        // wide so a wrapped count cannot bypass this check.
        if (datasize < inwidth || lastsample > INT_MAX ||
            (lastsample >> FractionBits) >= static_cast<unsigned int>(datasize / inwidth)) {
            Con_DPrintf("ResampleSfx: %s has invalid resample count: %d step: %d datasize: %d\n", sfx->name, outcount,
                        fracstep, datasize);
            memset(sc->data, 0, static_cast<std::size_t>(outcount) * sc->width);
            return;
        }
    }

    samplefrac = 0;
    // check first 2 characters

    for (i = 0; i < outcount; i++) {
        srcsample = static_cast<int>(samplefrac >> FractionBits);
        samplefrac += fracstep;

        if (inwidth == 2) {
            sample = LittleShort(((short *)data)[srcsample]);
        } else {
            sample = ((int)data[srcsample] - 128) * 256;
        }

        if (sc->width == 2) {
            ((short *)sc->data)[i] = (short)sample;
        } else {
            sc->data[i] = (byte)(sample >> 8);
        }
    }
}

void DumpChunks()
{
	char str[5];

	str[4] = 0;
	data_p = iff_data;
	do
	{
		Q_memcpy( str, data_p, 4 );
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Con_Printf( "0x%x : %s (%d)\n", (uintptr_t)( data_p - 4 ), str, iff_chunk_len );
		data_p += (int)( ( (unsigned int)iff_chunk_len + 1 ) & ~1u );
	} while ( data_p < iff_end );
}
