#include "browser_paint.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

using namespace cse::browser;

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "browser_paint_failure=%s\n", message);
        std::exit(1);
    }
}

static void ExpectSend(PendingPaint& pending, PaintBounds expected, const char* message) {
    PaintBounds actual;
    Require(pending.BeginSend(actual), message);
    Require(actual.x == expected.x && actual.y == expected.y && actual.width == expected.width &&
        actual.height == expected.height, message);
}

int main() {
    PendingPaint pending;
    PaintBounds unused;
    Require(!pending.BeginSend(unused) && !pending.Acknowledge(), "no unsolicited frames or acknowledgements");
    pending.View(8, 6, std::vector<PaintBounds>{{2, 3, 1, 1}});
    ExpectSend(pending, {0, 0, 8, 6}, "first paint initializes full backing surface");
    Require(pending.Acknowledge(), "first acknowledgement accepted");
    Require(!pending.BeginSend(unused), "acknowledgement does not create another frame");
    pending.View(8, 6, std::vector<PaintBounds>{{2, 3, 3, 2}});
    ExpectSend(pending, {2, 3, 3, 2}, "small dirty region preserved");
    pending.View(8, 6, std::vector<PaintBounds>{{1, 1, 2, 1}, {5, 4, 2, 1}});
    pending.View(8, 6, std::vector<PaintBounds>{{0, 2, 1, 1}});
    pending.Popup();
    Require(!pending.BeginSend(unused), "paints wait for outstanding acknowledgement");
    Require(pending.Acknowledge(), "outstanding paint acknowledgement accepted");
    ExpectSend(pending, {0, 1, 7, 4}, "all rectangles and in-flight paints coalesce");
    Require(pending.Acknowledge(), "union acknowledged");
    pending.Popup();
    ExpectSend(pending, {0, 0, 0, 0}, "popup-only paint does not upload unchanged main rows");
    pending.View(10, 7, std::vector<PaintBounds>{{4, 3, 1, 1}});
    pending.View(10, 7, std::vector<PaintBounds>{{8, 5, 1, 1}});
    Require(!pending.BeginSend(unused) && pending.Acknowledge(), "resize waits for prior frame");
    ExpectSend(pending, {0, 0, 10, 7}, "resize and subsequent paints retain full repaint");
    Require(pending.Acknowledge(), "resized frame acknowledged");
    pending.RequireFullView();
    pending.View(10, 7, std::vector<PaintBounds>{{4, 3, 1, 1}});
    ExpectSend(pending, {0, 0, 10, 7}, "explicit repaint initializes all rows");
    Require(pending.Acknowledge(), "explicit repaint acknowledged");
    pending.View(10, 7, std::vector<PaintBounds>{{-2, -1, 4, 3}, {9, 6, 100, 100}});
    ExpectSend(pending, {0, 0, 10, 7}, "CEF dirty bounds clipped to backing viewport");
    Require(pending.Acknowledge(), "clipped frame acknowledged");
    pending.View(10, 7, std::vector<PaintBounds>{{9, 6, std::numeric_limits<int>::max(), 1}});
    ExpectSend(pending, {9, 6, 1, 1}, "clipping avoids signed overflow");
    Require(pending.Acknowledge(), "overflow-safe frame acknowledged");
    pending.View(10, 7, std::vector<PaintBounds>{});
    ExpectSend(pending, {0, 0, 10, 7}, "missing CEF bounds conservatively repaint all rows");
    Require(ValidPaintBounds({0, 0, 0, 0}, 10, 7), "canonical empty popup-only main update accepted");
    for (PaintBounds invalid : std::vector<PaintBounds>{{-1, 0, 1, 1}, {0, -1, 1, 1}, {0, 0, -1, 1},
        {0, 0, 1, -1}, {10, 0, 1, 1}, {0, 7, 1, 1}, {9, 0, 2, 1}, {0, 6, 1, 2},
        {1, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 1, 0}, {1, 0, std::numeric_limits<int>::max(), 1}}) {
        Require(!ValidPaintBounds(invalid, 10, 7), "invalid wire bounds rejected");
    }
    std::puts("native_browser_paint_passed=1");
}
