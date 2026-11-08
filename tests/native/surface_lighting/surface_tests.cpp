#include "model.h"
#include "d_iface.h"
#include "../strings/high_offset_buffer.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#if CSE_SURFACE_REFERENCE_MMX
#include <mmintrin.h>
#endif

int gHasMMXTechnology;
int blocksize;
int blockdivshift;
void* prowdestbase;
int surfrowbytes;
int sourcetstep;
int r_numvblocks;
int r_deltav;
int r_lightwidth;
int r_offset;
int sourcevstep;
int r_stepback;
int is15bit;
colorVec* r_lightptr;
std::array<texture_t*, 18> r_basetexture;
drawsurf_t r_drawsurf;

#include "surface_header.h"

namespace {
constexpr std::size_t TextureStride = 8192;
constexpr std::size_t MipPixels = 1024;
constexpr std::size_t PixelOffset = 128;
constexpr std::size_t PaletteOffset = PixelOffset + MIPLEVELS * MipPixels + 1;
constexpr std::size_t MappingBytes = 65536;
constexpr unsigned char GuardByte = 0xcd;
constexpr int LightColumns = 3;
constexpr int VerticalBlocks = 3;
constexpr int SourceWidth = 32;
constexpr int RowPaddingPixels = 5;
constexpr int CorpusCases = 256;
constexpr int BytesPerPixel = 2;
constexpr int PaletteEntries = 256;
constexpr int PaletteChannels = 4;
constexpr std::size_t PaletteChannelBytes = sizeof(std::uint16_t);
constexpr std::size_t PaletteEntryBytes = PaletteChannels * PaletteChannelBytes;
constexpr std::uint16_t NormalPaletteMask = 0x7ff;
static_assert(PixelOffset >= sizeof(texture_t), "Fixture pixels must follow the native texture header");

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Surface lighting fixture failed: %s\n", message);
        std::exit(1);
    }
}

std::uint32_t Next(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void Write32(std::FILE* output, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8) {
        Require(std::fputc(static_cast<unsigned char>(value >> shift), output) != EOF, "write state corpus");
    }
}
} // namespace

int main(int argc, char** argv) {
    Require(argc == 2, "one pixel corpus path required");
    HighOffsetBuffer textures(nullptr);
    HighOffsetBuffer pixels(nullptr);
    Require(textures.get() && pixels.get(), "native high mappings available");
#if UINTPTR_MAX > UINT32_MAX
    Require(reinterpret_cast<std::uintptr_t>(textures.get()) > UINT32_MAX
        && reinterpret_cast<std::uintptr_t>(pixels.get()) > UINT32_MAX, "high texture and output addresses");
#endif
    std::FILE* output = std::fopen(argv[1], "wb");
    Require(output != nullptr, "open pixel corpus");
    std::array<colorVec, (VerticalBlocks + 1) * LightColumns> lights;
    std::uint32_t state = 0x6d2b79f5;
    for (int test = 0; test < CorpusCases; ++test) {
        for (int textureIndex = 0; textureIndex <= VerticalBlocks; ++textureIndex) {
            char* storage = textures.get() + textureIndex * TextureStride;
            std::memset(storage, 0, TextureStride);
            auto* texture = new (storage) texture_t{};
            texture->paloffset = static_cast<unsigned int>(PaletteOffset);
            for (int mip = 0; mip < MIPLEVELS; ++mip) {
                texture->offsets[mip] = static_cast<unsigned int>(PixelOffset + mip * MipPixels);
                for (std::size_t index = 0; index < MipPixels; ++index) {
                    storage[texture->offsets[mip] + index] = static_cast<char>(Next(state));
                }
            }
            for (int paletteIndex = 0; paletteIndex < PaletteEntries; ++paletteIndex) {
                for (int channel = 0; channel < PaletteChannels; ++channel) {
                    const auto bits = static_cast<std::uint16_t>(Next(state));
                    const std::uint16_t value = test % 2 == 0 ? bits : static_cast<std::uint16_t>(bits & NormalPaletteMask);
                    std::memcpy(storage + PaletteOffset + paletteIndex * PaletteEntryBytes + channel * PaletteChannelBytes, &value, sizeof(value));
                }
            }
            r_basetexture[textureIndex] = texture;
        }
        for (auto& light : lights) {
            light = {Next(state), Next(state), Next(state), Next(state)};
        }
        if (test == 0) {
            for (auto& light : lights) {
                light = {};
            }
        }
        for (int mip = 0; mip < MIPLEVELS; ++mip) {
            for (int colorBits = 0; colorBits < 2; ++colorBits) {
                for (int enabled = 0; enabled < 2; ++enabled) {
                    std::memset(pixels.get(), GuardByte, MappingBytes);
                    gHasMMXTechnology = enabled;
                    is15bit = colorBits;
                    blockdivshift = 4 - mip;
                    blocksize = 1 << blockdivshift;
                    sourcetstep = SourceWidth;
                    sourcevstep = SourceWidth * blocksize;
                    r_stepback = static_cast<int>(MipPixels);
                    const int initialOffset = test % 2 == 0 ? 0 : sourcevstep;
                    r_offset = initialOffset;
                    r_deltav = 1;
                    r_numvblocks = VerticalBlocks;
                    r_lightwidth = LightColumns;
                    r_lightptr = lights.data();
                    surfrowbytes = (blocksize + RowPaddingPixels) * BytesPerPixel;
                    prowdestbase = pixels.get() + 1;
                    r_drawsurf.surfmip = mip;
                    R_DrawSurfaceBlock16MMX();
                    const int rows = blocksize * VerticalBlocks;
                    const int activeBytes = rows * surfrowbytes;
                    Require(static_cast<unsigned char>(pixels[0]) == GuardByte, "output prefix guard");
                    for (int row = 0; row < rows; ++row) {
                        const int writtenBytes = enabled ? blocksize * BytesPerPixel : 0;
                        for (int byte = writtenBytes; byte < surfrowbytes; ++byte) {
                            Require(static_cast<unsigned char>(pixels[1 + row * surfrowbytes + byte]) == GuardByte,
                                "output row padding and disabled path unchanged");
                        }
                    }
                    for (int index = activeBytes + 1; index < activeBytes + 16; ++index) {
                        Require(static_cast<unsigned char>(pixels[index]) == GuardByte, "output suffix guard");
                    }
                    Require(r_lightptr == lights.data() + (enabled ? VerticalBlocks * LightColumns : 0),
                        "light pointer advancement");
                    const int expectedOffset = enabled
                        ? (initialOffset + VerticalBlocks * sourcevstep) % r_stepback : initialOffset;
                    Require(r_offset == expectedOffset, "source texture wrapping");
                    Write32(output, static_cast<std::uint32_t>(r_offset));
                    Write32(output, static_cast<std::uint32_t>(r_lightptr - lights.data()));
                    Write32(output, static_cast<std::uint32_t>(activeBytes));
                    Require(std::fwrite(pixels.get(), 1, activeBytes + 16, output)
                        == static_cast<std::size_t>(activeBytes + 16), "write pixels and guards");
                }
            }
        }
    }
    Require(std::fclose(output) == 0, "close pixel corpus");
    std::puts("4,096 surface cases checked: RGB555/565, four mips, palette overflow, wrapping, guards and high pointers");
    return 0;
}
