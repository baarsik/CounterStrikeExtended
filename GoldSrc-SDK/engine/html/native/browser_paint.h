#pragma once

#include <algorithm>
#include <cstdint>

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
