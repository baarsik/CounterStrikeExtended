//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "voice_wavefile.h"
#include "filesystem.h"

#include <cstdint>
#include <cstring>

static std::uint32_t ReadDWord(FileHandle_t file) {
    std::uint32_t value;
    FS_Read(&value, sizeof(value), 1, file);
    return value;
}

static int ReadDWordAsInt(FileHandle_t file) {
    // Win32 assigns the complete DWORD word to these signed int outputs.
    const std::uint32_t word = ReadDWord(file);
    int value;
    static_assert(sizeof(value) == sizeof(word), "WAVE signed word width");
    std::memcpy(&value, &word, sizeof(value));
    return value;
}

static std::uint16_t ReadWord(FileHandle_t file) {
    std::uint16_t value;
    FS_Read(&value, sizeof(value), 1, file);
    return value;
}

static void WriteDWord(FileHandle_t file, std::uint32_t value) {
    FS_Write(&value, sizeof(value), 1, file);
}

static void WriteWord(FileHandle_t file, std::uint16_t value) {
    FS_Write(&value, sizeof(value), 1, file);
}

bool ReadWaveFile(
    const char* filename, char*& data, int& dataBytes, int& bitsPerSample, int& channels, int& samplesPerSecond) {
    constexpr int kChannelsOffset = 22;
    constexpr int kBitsPerSampleOffset = 34;
    constexpr int kDataLengthOffset = 40;
    FileHandle_t file = FS_Open(filename, "rb");
    if (!file) {
        return false;
    }

    FS_Seek(file, kChannelsOffset, FILESYSTEM_SEEK_HEAD);
    channels = ReadWord(file);
    samplesPerSecond = ReadDWordAsInt(file);

    FS_Seek(file, kBitsPerSampleOffset, FILESYSTEM_SEEK_HEAD);
    bitsPerSample = ReadWord(file);

    FS_Seek(file, kDataLengthOffset, FILESYSTEM_SEEK_HEAD);
    dataBytes = ReadDWordAsInt(file);
    // Retain the existing reader's extra DWORD consumption before its PCM read.
    ReadDWord(file);

    data = new char[dataBytes];
    if (!data) {
        FS_Close(file);
        return false;
    }

    FS_Read(data, dataBytes, 1, file);
    FS_Close(file);
    return true;
}

bool WriteWaveFile(
    const char* filename, const char* data, int dataBytes, int bitsPerSample, int channels, int samplesPerSecond) {
    constexpr int kChunkIdBytes = 4;
    constexpr int kRiffPrefixBytes = kChunkIdBytes + sizeof(std::uint32_t);
    constexpr std::uint32_t kPcmFormatBytes = 16;
    constexpr std::uint16_t kPcmFormatTag = 1;
    constexpr int kBitsPerByte = 8;
    FileHandle_t file = FS_Open(filename, "wb");
    if (!file) {
        return false;
    }

    FS_Write("RIFF", kChunkIdBytes, 1, file);
    WriteDWord(file, 0);
    FS_Write("WAVE", kChunkIdBytes, 1, file);

    FS_Write("fmt ", kChunkIdBytes, 1, file);
    WriteDWord(file, kPcmFormatBytes);
    WriteWord(file, kPcmFormatTag);
    WriteWord(file, static_cast<std::uint16_t>(channels));
    WriteDWord(file, static_cast<std::uint32_t>(samplesPerSecond));
    WriteDWord(file, static_cast<std::uint32_t>((bitsPerSample / kBitsPerByte) * channels * samplesPerSecond));
    WriteWord(file, static_cast<std::uint16_t>((bitsPerSample / kBitsPerByte) * channels));
    WriteWord(file, static_cast<std::uint16_t>(bitsPerSample));

    FS_Write("data", kChunkIdBytes, 1, file);
    WriteDWord(file, static_cast<std::uint32_t>(dataBytes));
    FS_Write(data, dataBytes, 1, file);

    const std::uint32_t riffPayloadBytes = FS_Tell(file) - kRiffPrefixBytes;
    FS_Seek(file, kChunkIdBytes, FILESYSTEM_SEEK_HEAD);
    WriteDWord(file, riffPayloadBytes);

    FS_Close(file);
    return true;
}
