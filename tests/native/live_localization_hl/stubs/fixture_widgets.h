#ifndef HL_LANGUAGE_FIXTURE_WIDGETS_H
#define HL_LANGUAGE_FIXTURE_WIDGETS_H
#include <cstring>
#include <string>
namespace vgui {
struct Font { int generation = 0; };
struct Panel {
    ~Panel() = default;
    virtual void invalidateLayout(bool) {}
    int getWide() { return 300; }
    int getTall() { return 200; }
};
struct TextImage {
    void setSize(int, int) {}
    void getTextSizeWrapped(int& wide, int& tall) { wide = 300; tall = 500; }
};
struct Label : Panel {
    explicit Label(const char* value = "") : text(value) {}
    void setFont(Font* value) { font = value; }
    void setText(int bytes, const char* value) {
        assert(bytes == static_cast<int>(strlen(value)) + 1);
        text = value;
    }
    std::string text;
    Font* font = nullptr;
};
struct TextPanel : Panel {
    TextPanel(const char* value, int, int, int, int) : text(value) {}
    void setFont(Font* value) { font = value; }
    void setText(const char* value) { text = value; }
    TextImage* getTextImage() { return &image; }
    void setSize(int, int) {}
    std::string text;
    Font* font = nullptr;
    TextImage image;
};
struct ScrollPanel : Panel {
    int horizontal = 13;
    int vertical = 57;
    void getScrollValue(int& x, int& y) { x = horizontal; y = vertical; }
    void setScrollValue(int x, int y) { horizontal = x; vertical = y; }
    Panel* getClientClip() { return this; }
    void validate() { horizontal = 0; vertical = 0; }
};
}
#endif
