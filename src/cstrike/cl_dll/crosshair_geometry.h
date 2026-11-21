#pragma once

#include <cctype>
#include <charconv>
#include <cmath>
#include <string_view>

namespace cse::crosshair {
// Console values follow NextClient. The four existing size values keep their CSE scale.
enum class Shape { Cross, T, Circle, Dot, Count };
enum class Size { Auto, Small, Medium, Large, ExtraSmall, Count };

inline constexpr const char* ShapeCvar = "cl_crosshair_type";
inline constexpr const char* SizeCvar = "cl_crosshair_size";

inline int ParseIndex(const char* value, int count) {
    if (!value || !*value) {
        return 0;
    }
    const std::string_view input(value);
    int index = 0;
    const auto result = std::from_chars(input.data(), input.data() + input.size(), index);
    return result.ec == std::errc{} && result.ptr == input.data() + input.size() && index >= 0 && index < count
        ? index : 0;
}

inline Shape ParseShape(const char* value) {
    return static_cast<Shape>(ParseIndex(value, static_cast<int>(Shape::Count)));
}

inline Size ParseSize(const char* value) {
    constexpr std::string_view names[] = {"auto", "small", "medium", "large", "extra_small"};
    static_assert(sizeof(names) / sizeof(names[0]) == static_cast<int>(Size::Count));
    const std::string_view input = value ? value : "";
    for (int index = 0; index < static_cast<int>(Size::Count); ++index) {
        const auto name = names[index];
        if (input.size() != name.size()) {
            continue;
        }
        std::size_t character = 0;
        while (character < name.size() && std::tolower(static_cast<unsigned char>(input[character])) == name[character]) {
            ++character;
        }
        if (character == name.size()) {
            return static_cast<Size>(index);
        }
    }
    return static_cast<Size>(ParseIndex(value, static_cast<int>(Size::Count)));
}

inline int ScaleBase(Size size, int screenWidth) {
    constexpr int small = 1024;
    constexpr int medium = 800;
    constexpr int large = 640;
    constexpr int extraSmall = 1400;
    switch (size) {
    case Size::Small: return small;
    case Size::Medium: return medium;
    case Size::Large: return large;
    case Size::ExtraSmall: return extraSmall;
    default: return screenWidth <= large ? small : (screenWidth < small ? medium : large);
    }
}

struct Geometry {
    float gap;
    int barLength;
};

inline Geometry Scale(Geometry geometry, Size size, int screenWidth) {
    const int base = ScaleBase(size, screenWidth);
    return {static_cast<float>(static_cast<double>(geometry.gap) * screenWidth / base),
        screenWidth * geometry.barLength / base};
}

template<class Fill>
void Draw(Shape shape, int centerX, int centerY, Geometry geometry, Fill fill) {
    if (shape == Shape::Dot) {
        constexpr int radius = 1;
        constexpr int diameter = radius * 2 + 1;
        fill(centerX - radius, centerY - radius, diameter, diameter);
        return;
    }
    if (shape == Shape::Circle) {
        int x = static_cast<int>(std::round(geometry.gap + geometry.barLength * 0.5f));
        if (x < 1) {
            x = 1;
        }
        int y = 0;
        int error = 1 - x;
        // Use octant symmetry. Draw axis and diagonal pixels only once for additive blending.
        auto plot = [&](int dx, int dy) {
            fill(centerX + dx, centerY + dy, 1, 1);
            fill(centerX - dx, centerY + dy, 1, 1);
            if (dy != 0) {
                fill(centerX + dx, centerY - dy, 1, 1);
                fill(centerX - dx, centerY - dy, 1, 1);
            }
        };
        while (x >= y) {
            plot(x, y);
            if (y == 0) {
                fill(centerX, centerY - x, 1, 1);
                fill(centerX, centerY + x, 1, 1);
            } else if (x != y) {
                plot(y, x);
            }
            ++y;
            if (error < 0) {
                error += 2 * y + 1;
            } else {
                --x;
                error += 2 * (y - x) + 1;
            }
        }
        return;
    }

    const int gap = static_cast<int>(geometry.gap);
    const int length = geometry.barLength;
    fill(centerX - gap - length + 1, centerY, length, 1);
    fill(centerX + gap, centerY, length, 1);
    if (shape != Shape::T) {
        fill(centerX, centerY - gap - length + 1, 1, length);
    }
    fill(centerX, centerY + gap, 1, length);
}
} // namespace cse::crosshair
