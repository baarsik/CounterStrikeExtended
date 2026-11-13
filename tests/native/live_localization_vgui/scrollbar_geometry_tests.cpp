#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdint>

namespace vgui2 {
class ScrollBarSlider {
public:
    int wide = 10;
    int tall = 101;
    bool _vertical = true;
    int _range[2] = {0, 100};
    int _rangeWindow = 100;
    int _value = 0;
    static constexpr int UncomputedNobPosition = -99;
    int _nobPos[2] = {UncomputedNobPosition, UncomputedNobPosition};
    int repaints = 0;
    void GetPaintSize(int& width, int& height) {
        width = wide;
        height = tall;
    }
    void Repaint() {
        ++repaints;
    }
    void RecomputeNobPosFromValue();
    void RecomputeValueFromNobPos();
};
#include "scrollbar_geometry.inc"
}

int TrackEnd(const vgui2::ScrollBarSlider& slider) {
    return slider._vertical ? (slider.tall > 0 ? slider.tall - 1 : 0) : std::max(0, slider.wide);
}
int LegalMaximum(const vgui2::ScrollBarSlider& slider) {
    return static_cast<int>(std::max<std::int64_t>(slider._range[0],
        static_cast<std::int64_t>(slider._range[1]) - std::max(0, slider._rangeWindow)));
}
void RequireBounds(vgui2::ScrollBarSlider& slider) {
    const int original = slider._value;
    slider.RecomputeNobPosFromValue();
    assert(slider.repaints == 1);
    assert(slider._nobPos[0] >= 0 && slider._nobPos[0] <= slider._nobPos[1]);
    assert(slider._nobPos[1] <= TrackEnd(slider));
    assert(slider._value >= slider._range[0] && slider._value <= LegalMaximum(slider));
    if (slider.wide <= 1 && slider.tall <= 1 && original >= slider._range[0]
        && original <= LegalMaximum(slider)) {
        assert(slider._value == original); // Collapsed layout must not reset a valid logical scroll position.
    }
    slider.RecomputeValueFromNobPos();
    assert(slider._value >= slider._range[0] && slider._value <= LegalMaximum(slider));
}
int main() {
    // This is the RichText teardown domain from the strict widget stack trace.
    vgui2::ScrollBarSlider full;
    full.RecomputeNobPosFromValue();
    assert(full._nobPos[0] == 0 && full._nobPos[1] == 100 && full._value == 0);
    full.RecomputeValueFromNobPos();
    assert(full._value == 0);

    for (const bool vertical : {false, true}) {
        vgui2::ScrollBarSlider normal;
        normal._vertical = vertical;
        normal.wide = vertical ? 10 : 100;
        normal.tall = vertical ? 101 : 11;
        normal._rangeWindow = 20;
        normal._value = 40;
        normal.RecomputeNobPosFromValue();
        assert(normal._nobPos[0] == 40 && normal._nobPos[1] == 60);
        normal.RecomputeValueFromNobPos();
        assert(normal._value == 40);
        normal._value = 80;
        normal.RecomputeNobPosFromValue();
        assert(normal._nobPos[0] == 80 && normal._nobPos[1] == 100);
        normal.RecomputeValueFromNobPos();
        assert(normal._value == 80);
        normal._range[0] = 10;
        normal._range[1] = 110;
        normal._value = 50;
        normal.RecomputeNobPosFromValue();
        assert(normal._nobPos[0] == 40 && normal._nobPos[1] == 60);
        normal.RecomputeValueFromNobPos();
        assert(normal._value == 50);
        normal._range[0] = 0;
        normal._range[1] = 100;
        normal._rangeWindow = 1;
        normal._value = 50;
        normal.RecomputeNobPosFromValue();
        assert(normal._nobPos[0] == 45 && normal._nobPos[1] == 55);

        vgui2::ScrollBarSlider tiny;
        tiny._vertical = vertical;
        tiny.wide = vertical ? 10 : 3;
        tiny.tall = vertical ? 4 : 10;
        tiny._rangeWindow = 20;
        tiny._value = 40;
        tiny.RecomputeNobPosFromValue();
        assert(tiny._nobPos[0] == 0 && tiny._nobPos[1] == 3);
        tiny.RecomputeValueFromNobPos();
        assert(tiny._value == 40);
    }
    const int ranges[][2] = {{0, 0}, {10, 10}, {10, 0}, {0, 100}, {-20, 80}, {INT_MIN, INT_MAX}};
    const int dimensions[][2] = {{10, 101}, {101, 10}, {0, 0}, {1, 1}, {10, 1}, {1, 10},
        {0, 100}, {100, 0}, {-1, 10}, {10, -1}, {INT_MAX, INT_MAX}, {INT_MIN, INT_MIN}};
    int cases = 0;
    for (const bool vertical : {false, true}) {
        for (const auto& range : ranges) {
            for (const int window : {-1, 0, 1, 20, 100, 200, INT_MAX}) {
                for (const int value : {INT_MIN, range[0], 0, 40, INT_MAX}) {
                    for (const auto& size : dimensions) {
                        vgui2::ScrollBarSlider slider;
                        slider._vertical = vertical;
                        slider._range[0] = range[0];
                        slider._range[1] = range[1];
                        slider._rangeWindow = window;
                        slider._value = value;
                        slider.wide = size[0];
                        slider.tall = size[1];
                        RequireBounds(slider);
                        for (const int pixel : {INT_MIN, -1, 0, 50, INT_MAX}) {
                            slider._nobPos[0] = pixel;
                            slider.RecomputeValueFromNobPos();
                            assert(slider._value >= slider._range[0] && slider._value <= LegalMaximum(slider));
                        }
                        ++cases;
                    }
                }
            }
        }
    }
    std::printf("PASS actual scrollbar geometry: %d domain cases, both axes, valid legacy bounds and collapsed-state retention\n", cases);
}
