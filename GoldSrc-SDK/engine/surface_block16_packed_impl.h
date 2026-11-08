// Scalar equivalent of the retained MMX RGB555/RGB565 block-lighting operation.
// This header is included after the renderer globals and SDK texture types.
#include <cstdint>
#include <cstring>

namespace cse_surface_detail {
inline std::int32_t SignedWord(std::uint32_t value) {
    constexpr std::uint32_t WordMask = 0xffff;
    constexpr std::uint32_t SignBit = 0x8000;
    constexpr std::int32_t WordModulus = 0x10000;
    const std::int32_t word = static_cast<std::int32_t>(value & WordMask);
    return (value & SignBit) != 0 ? word - WordModulus : word;
}

inline std::int32_t ShiftRight(std::int32_t value, unsigned int bits) {
    constexpr unsigned int WordBits = 16;
    if (bits >= WordBits) {
        return value < 0 ? -1 : 0;
    }
    const std::int32_t divisor = 1 << bits;
    return value < 0 ? -((-value + divisor - 1) / divisor) : value / divisor;
}

inline std::int32_t ScaleLight(unsigned int value) {
    constexpr std::uint32_t WordMask = 0xffff;
    constexpr std::uint32_t LightScale = 0x5555;
    constexpr unsigned int ProductHighShift = 16;
    return static_cast<std::int32_t>((((value & WordMask) >> 1) * LightScale) >> ProductHighShift);
}

inline unsigned int ColorChannel(std::uint16_t palette, std::int32_t light) {
    constexpr unsigned int PaletteShift = 3;
    constexpr std::int32_t ProductDivisor = 0x10000;
    constexpr std::int32_t ChannelMaximum = 255;
    const std::int32_t sample = SignedWord(static_cast<std::uint32_t>(palette) << PaletteShift);
    const std::int32_t product = sample * light;
    // Signed multiply-high rounds down; unsigned-byte packing then saturates.
    const std::int32_t high = product < 0
        ? -((-product + ProductDivisor - 1) / ProductDivisor) : product / ProductDivisor;
    if (high < 0) {
        return 0;
    }
    if (high > ChannelMaximum) {
        return ChannelMaximum;
    }
    return static_cast<unsigned int>(high);
}
} // namespace cse_surface_detail

void R_DrawSurfaceBlock16MMX() {
    if (!gHasMMXTechnology) {
        return;
    }
    constexpr std::size_t ColorChannels = 3;
    constexpr std::size_t PaletteEntryBytes = 8;
    constexpr unsigned int FiveBitMask = 0xf8;
    constexpr unsigned int SixBitMask = 0xfc;
    constexpr unsigned int ByteToFiveBitShift = 3;
    constexpr unsigned int RedPackShift = 8;
    constexpr unsigned int Green555Shift = 2;
    constexpr unsigned int Green565Shift = 3;
    using namespace cse_surface_detail;
    auto* destination = static_cast<unsigned char*>(prowdestbase);
    std::int32_t left[ColorChannels] = {
        ScaleLight(r_lightptr[0].b), ScaleLight(r_lightptr[0].g), ScaleLight(r_lightptr[0].r)
    };
    std::int32_t right[ColorChannels] = {
        ScaleLight(r_lightptr[1].b), ScaleLight(r_lightptr[1].g), ScaleLight(r_lightptr[1].r)
    };
    for (int vertical = 0; vertical < r_numvblocks; ++vertical) {
        texture_t* texture = r_basetexture[r_deltav + vertical];
        const auto* storage = reinterpret_cast<const unsigned char*>(texture);
        const unsigned char* source = storage + texture->offsets[r_drawsurf.surfmip] + r_offset;
        const unsigned char* palette = storage + texture->paloffset;
        r_lightptr += r_lightwidth;
        const std::int32_t nextLeft[ColorChannels] = {
            ScaleLight(r_lightptr[0].b), ScaleLight(r_lightptr[0].g), ScaleLight(r_lightptr[0].r)
        };
        const std::int32_t nextRight[ColorChannels] = {
            ScaleLight(r_lightptr[1].b), ScaleLight(r_lightptr[1].g), ScaleLight(r_lightptr[1].r)
        };
        std::int32_t leftStep[ColorChannels];
        std::int32_t rightStep[ColorChannels];
        for (std::size_t channel = 0; channel < ColorChannels; ++channel) {
            leftStep[channel] = ShiftRight(SignedWord(nextLeft[channel] - left[channel]), blockdivshift);
            rightStep[channel] = ShiftRight(SignedWord(nextRight[channel] - right[channel]), blockdivshift);
        }
        for (int row = 0; row < blocksize; ++row) {
            std::int32_t light[ColorChannels];
            std::int32_t step[ColorChannels];
            for (std::size_t channel = 0; channel < ColorChannels; ++channel) {
                light[channel] = left[channel];
                step[channel] = ShiftRight(SignedWord(right[channel] - left[channel]), blockdivshift);
            }
            for (int column = 0; column < blocksize; ++column) {
                std::uint16_t entry[ColorChannels];
                std::memcpy(entry, palette + source[column] * PaletteEntryBytes, sizeof(entry));
                unsigned int color[ColorChannels];
                for (std::size_t channel = 0; channel < ColorChannels; ++channel) {
                    color[channel] = ColorChannel(entry[channel], light[channel]);
                    light[channel] = SignedWord(light[channel] + step[channel]);
                }
                const unsigned int greenMask = is15bit ? FiveBitMask : SixBitMask;
                const unsigned int greenShift = is15bit ? Green555Shift : Green565Shift;
                const unsigned int redShift = is15bit ? RedPackShift - 1 : RedPackShift;
                const auto pixel = static_cast<std::uint16_t>((color[0] >> ByteToFiveBitShift)
                    | ((color[1] & greenMask) << greenShift) | ((color[2] & FiveBitMask) << redShift));
                std::memcpy(destination + column * sizeof(pixel), &pixel, sizeof(pixel));
            }
            for (std::size_t channel = 0; channel < ColorChannels; ++channel) {
                left[channel] = SignedWord(left[channel] + leftStep[channel]);
                right[channel] = SignedWord(right[channel] + rightStep[channel]);
            }
            source += sourcetstep;
            destination += surfrowbytes;
        }
        std::memcpy(left, nextLeft, sizeof(left));
        std::memcpy(right, nextRight, sizeof(right));
        r_offset += sourcevstep;
        if (r_offset >= r_stepback) {
            r_offset -= r_stepback;
        }
    }
}
