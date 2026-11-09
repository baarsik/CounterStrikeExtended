#include "VGUI_Color.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void Rgba(vgui::Color& color, const std::array<int, 4>& expected) {
    std::array<int, 4> actual{{}};
    color.getColor(actual[0], actual[1], actual[2], actual[3]);
    Require(actual == expected, "RGBA/alpha bytes differ");
    for (int index = 0; index < 4; ++index) {
        Require(color[index] == expected[static_cast<std::size_t>(index)], "virtual channel getter differs");
    }
}

class DerivedColor : public vgui::Color {
public:
    DerivedColor() : Color(1, 2, 3, 4) {}
    int operator[](int) override { return 9182; }
};
}

int main() {
    try {
        const std::array<int, 4> expected{{33, 67, 101, 135}};
        vgui::Color source(expected[0], expected[1], expected[2], expected[3]);
        vgui::Color copied(source);
        vgui::Color assigned;
        assigned = copied;
        Rgba(copied, expected);
        Rgba(assigned, expected);
        vgui::Color moved(std::move(copied));
        Rgba(moved, expected);
        vgui::Color wrapped(-1, 256, 257, -2);
        Rgba(wrapped, {{255, 0, 1, 254}});
        vgui::Color scheme(vgui::Scheme::sc_primary2);
        vgui::Color schemeCopy(scheme);
        vgui::Scheme::SchemeColor schemeValue = vgui::Scheme::sc_user;
        schemeCopy.getColor(schemeValue);
        Require(schemeValue == vgui::Scheme::sc_primary2, "scheme member copy differs");
        DerivedColor derived;
        vgui::Color sliced(derived);
        Rgba(sliced, {{1, 2, 3, 4}});
        vgui::Color& base = derived;
        base = source;
        Require(base[0] == 9182, "assignment overwrote destination virtual dispatch");
        int r = 0;
        int g = 0;
        int b = 0;
        int a = 0;
        base.getColor(r, g, b, a);
        Require(std::array<int, 4>{{r, g, b, a}} == expected, "base assignment member bytes differ");

        // The object is properly aligned. Guard bytes are bytes, not type-punned uint32 storage.
        constexpr std::size_t GuardBytes = alignof(vgui::Color);
        alignas(vgui::Color) std::array<unsigned char, sizeof(vgui::Color) + 2 * GuardBytes> storage;
        storage.fill(0xa5);
        vgui::Color* placed = new (storage.data() + GuardBytes) vgui::Color(source);
        Rgba(*placed, expected);
        for (std::size_t index = 0; index < GuardBytes; ++index) {
            Require(storage[index] == 0xa5 && storage[GuardBytes + sizeof(vgui::Color) + index] == 0xa5,
                "aligned placement copy touched guards");
        }
        placed->vgui::Color::~Color();
        std::array<unsigned char, sizeof(vgui::Color)> representation{{}};
        std::memcpy(representation.data(), static_cast<const void*>(&source), representation.size());
        std::size_t rgbaOffset = representation.size();
        for (std::size_t index = 0; index + expected.size() <= representation.size(); ++index) {
            bool match = true;
            for (std::size_t channel = 0; channel < expected.size(); ++channel) {
                match = match && representation[index + channel] == expected[channel];
            }
            if (match) {
                Require(rgbaOffset == representation.size(), "ambiguous controlled byte pattern");
                rgbaOffset = index;
            }
        }
        Require(rgbaOffset < representation.size(), "RGBA representation missing");
        std::printf("{\"size\":%zu,\"align\":%zu,\"rgba_offset\":%zu,", sizeof(vgui::Color), alignof(vgui::Color), rgbaOffset);
        std::printf("\"copy_constructible\":%d,\"copy_assignable\":%d,\"nothrow_copy\":%d,\"nothrow_assign\":%d,",
            std::is_copy_constructible<vgui::Color>::value, std::is_copy_assignable<vgui::Color>::value,
            std::is_nothrow_copy_constructible<vgui::Color>::value, std::is_nothrow_copy_assignable<vgui::Color>::value);
        std::printf("\"trivial_copy\":%d,\"trivial_assign\":%d,\"move_constructible\":%d,\"move_assignable\":%d,",
            std::is_trivially_copy_constructible<vgui::Color>::value, std::is_trivially_copy_assignable<vgui::Color>::value,
            std::is_move_constructible<vgui::Color>::value, std::is_move_assignable<vgui::Color>::value);
        std::printf("\"nothrow_move\":%d,\"nothrow_move_assign\":%d,\"polymorphic\":%d,\"virtual_destructor\":%d}\n",
            std::is_nothrow_move_constructible<vgui::Color>::value, std::is_nothrow_move_assignable<vgui::Color>::value,
            std::is_polymorphic<vgui::Color>::value, std::has_virtual_destructor<vgui::Color>::value);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s\n", error.what());
        return 1;
    }
}
