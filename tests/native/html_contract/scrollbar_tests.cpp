#include "html/native/browser_wire.h"
#include "html/htmlmessages.h"

#include <cstdio>
#include <stdexcept>
#include <string>

using cse::browser::Reader;
using cse::browser::Writer;
using VPANEL = unsigned int;

struct PanelTree {
    VPANEL GetParent(VPANEL) const {
        return 0;
    }
    bool IsVisible(VPANEL) const {
        return true;
    }
};

PanelTree* ipanel() {
    static PanelTree tree;
    return &tree;
}

struct KeyValues {
    explicit KeyValues(const char*) {}
};

struct ScrollBar {
    bool visible = false;
    int value = 0;
    bool IsVisible() const {
        return visible;
    }
    void SetVisible(bool next) {
        visible = next;
    }
    int GetValue() const {
        return value;
    }
    void SetValue(int next) {
        value = next;
    }
    void SetEnabled(bool) {}
    void SetRangeWindow(int) {}
    void SetRange(int, int) {}
    void SetButtonPressedScrollValue(int) {}
    void SetPos(int, int) {}
    void SetTall(int) {}
    void SetWide(int) {}
};

struct Serializer {
    int width = 0;
    int height = 0;
    void BrowserSize(int nextWidth, int nextHeight) {
        width = nextWidth;
        height = nextHeight;
    }
    void SetHorizontalScroll(int) {}
    void SetVerticalScroll(int) {}
    void SetFocus(bool) {}
};

struct ScrollGeometry {
    bool m_bVisible = false;
    int m_nX = 0;
    int m_nY = 0;
    int m_nWide = 0;
    int m_nTall = 0;
    int m_nMax = 0;
    int m_nScroll = 0;
    float m_flZoom = 0;
};

class HTML {
public:
    ScrollBar horizontal;
    ScrollBar vertical;
    ScrollBar* _hbar = &horizontal;
    ScrollBar* _vbar = &vertical;
    Serializer serializer;
    Serializer* m_Serializer = &serializer;
    ScrollGeometry m_scrollHorizontal;
    ScrollGeometry m_scrollVertical;
    bool m_bScrollBarEnabled = true;
    int m_iScrollBorderX = 0;
    int m_iScrollBorderY = 0;
    int m_iWideLastHTMLSize = 0;
    int m_iTalLastHTMLSize = 0;
    int width = 640;
    int height = 480;
    VPANEL GetVPanel() const {
        return 1;
    }
    void GetSize(int& wide, int& tall) const {
        wide = width;
        tall = height;
    }
    void SetTall(int tall) {
        height = tall;
    }
    void InvalidateLayout(bool, bool) {}
    void PostMessage(HTML*, KeyValues* message, float) {
        delete message;
    }
    void UpdateSizeAndScrollBars();
    void CalcScrollBars(int wide, int tall);
    void BrowserResize();
    void BrowserHorizontalScrollBarSizeResponse(int, int, int, int, int, int, float);
    void BrowserVerticalScrollBarSizeResponse(int, int, int, int, int, int, float);
};

#include "scrollbars.inc"

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class HostGeometry {
public:
    explicit HostGeometry(HTML& widget) : widget_(widget) {}
    void Emit(EHTMLCommands operation, const Writer& payload) {
        Reader reader(payload.Data());
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t wide = 0;
        std::int32_t tall = 0;
        std::int32_t scroll = 0;
        std::int32_t maximum = 0;
        std::uint32_t zoomBits = 0;
        Require(reader.I32(x) && reader.I32(y) && reader.I32(wide) && reader.I32(tall)
            && reader.I32(scroll) && reader.I32(maximum) && reader.U32(zoomBits) && reader.Finish(),
            "incomplete native geometry response");
        float zoom = 0;
        std::memcpy(&zoom, &zoomBits, sizeof(zoom));
        if (operation == eHTMLCommands_VerticalScrollBarSizeResponse) {
            widget_.BrowserVerticalScrollBarSizeResponse(x, y, wide, tall, scroll, maximum, zoom);
        } else {
            Require(operation == eHTMLCommands_HorizontalScrollBarSizeResponse, "wrong geometry operation");
            widget_.BrowserHorizontalScrollBarSizeResponse(x, y, wide, tall, scroll, maximum, zoom);
        }
    }
    // Constants consumed by the extracted producer; no browser process is needed.
    static constexpr int ScrollBarThickness = 16;
    static constexpr float ZoomPercent = 100.0f;
#include "host_geometry.inc"
private:
    HTML& widget_;
};

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected a scrollbar scenario");
        const std::string scenario = argv[1];
        HTML widget;
        HostGeometry host(widget);
        widget.BrowserResize();
        if (scenario == "fit") {
            for (int reply = 0; reply < 3; ++reply) {
                host.Geometry(false, 640, 480, 640, 0);
                host.Geometry(true, 640, 480, 480, 0);
            }
            Require(!widget.vertical.visible && !widget.horizontal.visible, "fitted page acquired scrollbars");
            Require(widget.serializer.width == 640 && widget.serializer.height == 480,
                "fitted page lost browser viewport space");
        } else if (scenario == "vertical") {
            host.Geometry(true, 640, 480, 960, 0);
            Require(widget.vertical.visible && widget.serializer.width == 619,
                "first overflowing geometry did not show the retained vertical border");
            host.Geometry(true, 640, 480, 480, 0);
            Require(!widget.vertical.visible && widget.serializer.width == 640,
                "overflow-to-fit geometry did not restore the full viewport immediately");
        } else if (scenario == "horizontal") {
            host.Geometry(false, 640, 480, 1280, 0);
            Require(widget.horizontal.visible && widget.serializer.height == 459,
                "first overflowing geometry did not show the retained horizontal border");
            host.Geometry(false, 640, 480, 640, 0);
            Require(!widget.horizontal.visible && widget.serializer.height == 480,
                "horizontal overflow-to-fit geometry did not restore the full viewport immediately");
        } else if (scenario == "both") {
            host.Geometry(false, 640, 480, 1280, 0);
            host.Geometry(true, 640, 480, 960, 0);
            Require(widget.vertical.visible && widget.horizontal.visible, "two-axis overflow lost a scrollbar");
            host.Geometry(true, 640, 480, 480, 0);
            Require(!widget.vertical.visible && widget.horizontal.visible && widget.serializer.width == 640,
                "removing vertical overflow disturbed the horizontal scrollbar");
        } else if (scenario == "disabled") {
            widget.m_bScrollBarEnabled = false;
            host.Geometry(false, 640, 480, 1280, 0);
            host.Geometry(true, 640, 480, 960, 0);
            Require(!widget.vertical.visible && !widget.horizontal.visible,
                "disabled widget displayed scrollbars");
            Require(widget.serializer.width == 640 && widget.serializer.height == 480,
                "disabled scrollbars reduced the viewport");
        } else {
            throw std::runtime_error("unknown scrollbar scenario");
        }
        std::puts("native scrollbar producer/consumer fixture passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
