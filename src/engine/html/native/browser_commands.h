#pragma once

#include "../htmlmessages.h"
#include <cstddef>
#include <cstdint>

namespace cse::browser {
// The legacy command numbers are internal to this versioned local protocol, not GoldSrc network encodings.
enum class Control : std::uint32_t {
    Hello = 1,
    SetCookie = 2,
    GetCookies = 3,
    Cookies = 4,
    SetBuild = 5,
    SetFrameRate = 6,
    Failure = 7
};
constexpr std::uint32_t ProviderApiVersion = 15600;
constexpr std::uint32_t ProviderContractVersion = 1;
constexpr int MaximumBrowserDimension = 8192;
constexpr std::size_t PaintBytesPerPixel = 4;
// A paint frame contains the view and popup BGRA images, nine int32 fields and two blob-length fields.
constexpr std::size_t MaximumPaintPayloadBytes = 2 * static_cast<std::size_t>(MaximumBrowserDimension) *
    MaximumBrowserDimension * PaintBytesPerPixel + 11 * sizeof(std::uint32_t);
}
