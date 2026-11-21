#pragma once

#include "browser_commands.h"
#include "browser_paint.h"
#include "browser_wire.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace cse::browser {

// The retained images must outlive the synchronous write of all five segments.
struct PaintWireParts {
    static constexpr std::size_t PrefixFields = 10;
    std::array<std::uint8_t, PrefixFields * sizeof(std::uint32_t)> prefix{};
    const std::uint8_t* viewPixels = nullptr;
    std::size_t viewBytes = 0;
    std::array<std::uint8_t, sizeof(std::uint32_t)> popupLength{};
    const std::uint8_t* popupPixels = nullptr;
    std::size_t popupBytes = 0;
    std::uint32_t payloadBytes = 0;
};

inline bool EncodePaintWireParts(int texture, int width, int height, const PaintBounds& update,
    int popupWidth, int popupHeight, const std::vector<std::uint8_t>& view,
    const std::vector<std::uint8_t>& popup, PaintWireParts& parts) {
    parts = {};
    if (width <= 0 || width > MaximumBrowserDimension || height <= 0 || height > MaximumBrowserDimension ||
        popupWidth < 0 || popupWidth > MaximumBrowserDimension ||
        popupHeight < 0 || popupHeight > MaximumBrowserDimension || !ValidPaintBounds(update, width, height)) {
        return false;
    }
    const std::size_t expectedView = static_cast<std::size_t>(width) * height * PaintBytesPerPixel;
    const std::size_t expectedPopup = static_cast<std::size_t>(popupWidth) * popupHeight * PaintBytesPerPixel;
    if (view.size() != expectedView || popup.size() != expectedPopup) {
        return false;
    }
    const std::size_t payloadBytes = parts.prefix.size() + view.size() + parts.popupLength.size() + popup.size();
    if (payloadBytes > MaximumPaintPayloadBytes || payloadBytes > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    const std::array<int, PaintWireParts::PrefixFields> fields{
        texture, width, height, update.x, update.y, update.width, update.height,
        popupWidth, popupHeight, static_cast<int>(view.size())};
    for (std::size_t index = 0; index < fields.size(); ++index) {
        wire_detail::Store32(parts.prefix.data() + index * sizeof(std::uint32_t),
            static_cast<std::uint32_t>(fields[index]));
    }
    wire_detail::Store32(parts.popupLength.data(), static_cast<std::uint32_t>(popup.size()));
    parts.viewPixels = view.data();
    parts.viewBytes = view.size();
    parts.popupPixels = popup.empty() ? nullptr : popup.data();
    parts.popupBytes = popup.size();
    parts.payloadBytes = static_cast<std::uint32_t>(payloadBytes);
    return true;
}

}
