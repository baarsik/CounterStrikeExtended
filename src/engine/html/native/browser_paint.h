#pragma once

#include "browser_commands.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cse::browser {

struct PaintBounds {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Empty main updates are reserved for popup-only paints. The callback still receives full backing pixels.
inline bool ValidPaintBounds(const PaintBounds& bounds, int width, int height) {
    if (bounds.width == 0 && bounds.height == 0) {
        return bounds.x == 0 && bounds.y == 0;
    }
    return bounds.x >= 0 && bounds.y >= 0 && bounds.width > 0 && bounds.height > 0 &&
        bounds.x <= width && bounds.y <= height && bounds.width <= width - bounds.x &&
        bounds.height <= height - bounds.y;
}

// CEF supplies a complete BGRA image even when only one region changed. Keep the complete
// retained backing for the wire protocol, but copy only the changed rows for small paints.
// The caller has already validated dimensions and the CEF image pointer.
template<class Rectangles>
void UpdatePaintBacking(std::vector<std::uint8_t>& backing, const void* pixels, int width, int height,
    const Rectangles& rectangles) {
    const auto fullPixels = static_cast<std::size_t>(width) * height;
    const auto fullBytes = fullPixels * PaintBytesPerPixel;
    const auto* source = static_cast<const std::uint8_t*>(pixels);
    const auto copyFull = [&]() {
        backing.assign(source, source + fullBytes);
    };
    if (backing.size() != fullBytes || rectangles.size() != 1) {
        copyFull();
        return;
    }

    const auto& rectangle = rectangles.front();
    const auto left = std::clamp<std::int64_t>(rectangle.x, 0, width);
    const auto top = std::clamp<std::int64_t>(rectangle.y, 0, height);
    const auto right = std::clamp<std::int64_t>(static_cast<std::int64_t>(rectangle.x) + rectangle.width, 0, width);
    const auto bottom = std::clamp<std::int64_t>(static_cast<std::int64_t>(rectangle.y) + rectangle.height, 0, height);
    constexpr std::size_t kHalfAreaDivisor = 2;
    if (left >= right || top >= bottom ||
        static_cast<std::size_t>((right - left) * (bottom - top)) >= fullPixels / kHalfAreaDivisor) {
        copyFull();
        return;
    }

    const auto rowBytes = static_cast<std::size_t>(right - left) * PaintBytesPerPixel;
    for (auto row = top; row < bottom; ++row) {
        const auto offset = (static_cast<std::size_t>(row) * width + left) * PaintBytesPerPixel;
        std::memcpy(backing.data() + offset, source + offset, rowBytes);
    }
}

// CEF and its command callbacks use the same UI thread. Only bounds for the frame actually sent are cleared;
// paints received before its acknowledgement accumulate independently for the next full backing snapshot.
class PendingPaint {
public:
    template<class Rectangles>
    void View(int width, int height, const Rectangles& rectangles) {
        pending_ = true;
        if (width != width_ || height != height_ || fullView_ || rectangles.empty()) {
            bounds_ = {0, 0, width, height};
            width_ = width;
            height_ = height;
            fullView_ = false;
            return;
        }
        for (const auto& rectangle : rectangles) {
            const auto left = std::max<std::int64_t>(0, rectangle.x);
            const auto top = std::max<std::int64_t>(0, rectangle.y);
            const auto right = std::min<std::int64_t>(width, static_cast<std::int64_t>(rectangle.x) + rectangle.width);
            const auto bottom = std::min<std::int64_t>(height, static_cast<std::int64_t>(rectangle.y) + rectangle.height);
            if (left < right && top < bottom) {
                Include({static_cast<int>(left), static_cast<int>(top), static_cast<int>(right - left),
                    static_cast<int>(bottom - top)});
            }
        }
    }

    void Popup() {
        pending_ = true;
    }

    void RequireFullView() {
        fullView_ = true;
    }

    bool InFlight() const {
        return inFlight_;
    }

    bool BeginSend(PaintBounds& bounds) {
        if (!pending_ || inFlight_) {
            return false;
        }
        bounds = bounds_;
        bounds_ = {};
        pending_ = false;
        inFlight_ = true;
        return true;
    }

    bool Acknowledge() {
        if (!inFlight_) {
            return false;
        }
        inFlight_ = false;
        return true;
    }

private:
    void Include(const PaintBounds& rectangle) {
        if (bounds_.width == 0) {
            bounds_ = rectangle;
            return;
        }
        const int right = std::max(bounds_.x + bounds_.width, rectangle.x + rectangle.width);
        const int bottom = std::max(bounds_.y + bounds_.height, rectangle.y + rectangle.height);
        bounds_.x = std::min(bounds_.x, rectangle.x);
        bounds_.y = std::min(bounds_.y, rectangle.y);
        bounds_.width = right - bounds_.x;
        bounds_.height = bottom - bounds_.y;
    }

    PaintBounds bounds_;
    int width_ = 0;
    int height_ = 0;
    bool pending_ = false;
    bool inFlight_ = false;
    bool fullView_ = true;
};
}
