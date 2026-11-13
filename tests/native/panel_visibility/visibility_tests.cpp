#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

static void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

using VPANEL = std::uintptr_t;
class KeyValues {
public:
    explicit KeyValues(const char* value) : name(value) {}
    void deleteThis() { delete this; }
    std::string name;
};
class Panel;
class VPanel;
using Dar = std::vector<VPanel*>;
static void DarRemoveElement(Dar& children, VPanel* panel) {
    children.erase(std::remove(children.begin(), children.end(), panel), children.end());
}
static void DarPutElement(Dar& children, VPanel* panel) {
    children.push_back(panel);
}
struct Surface {
    void SetPanelVisible(VPANEL, bool) {}
    void CalculateMouseVisible() {}
};
namespace vgui2 {
Surface surface;
Surface* g_pSurface = &surface;
}
class VPanel {
public:
    explicit VPanel(Panel* client) : client_(client) {}
    void SetVisible(bool state);
    void SetParent(VPanel* parent);
    Panel* Client() { return client_; }
    bool IsPopup() { return false; }
    bool IsVisible() { return _visible; }
    int GetChildCount() { return static_cast<int>(_childDar.size()); }
    VPanel* GetChild(int index) { return _childDar.at(static_cast<std::size_t>(index)); }
    void SendMessage(KeyValues* message, VPANEL from);
    void SetZPos(int) {}
    bool _visible = true;
    VPanel* _parent = nullptr;
    Dar _childDar;
    int _zpos = 0;
private:
    Panel* client_;
};
struct PanelApi {
    void SetVisible(VPANEL panel, bool state) { reinterpret_cast<VPanel*>(panel)->SetVisible(state); }
    bool IsVisible(VPANEL panel) { return reinterpret_cast<VPanel*>(panel)->IsVisible(); }
    VPANEL GetParent(VPANEL panel) { return reinterpret_cast<VPANEL>(reinterpret_cast<VPanel*>(panel)->_parent); }
    int GetChildCount(VPANEL panel) { return reinterpret_cast<VPanel*>(panel)->GetChildCount(); }
};
static PanelApi panelApi;
static PanelApi* ipanel() { return &panelApi; }
class Panel {
public:
    Panel() : node(this) {}
    virtual ~Panel() = default;
    virtual void SetVisible(bool state);
    virtual void OnMessage(KeyValues*, VPANEL) {}
    void OnChildAdded(VPANEL) {}
    VPANEL GetVPanel() { return reinterpret_cast<VPANEL>(&node); }
    Panel* GetChild(int index) { return node.GetChild(index)->Client(); }
    VPanel node;
};
void VPanel::SendMessage(KeyValues* message, VPANEL from) { Client()->OnMessage(message, from); }
struct ScrollBar {
    int GetValue() { return 7; }
};
struct Serializer {
    void BrowserSize(int wide, int tall) { sizes.emplace_back(wide, tall); }
    void SetFocus(bool focus) { focuses.push_back(focus); }
    void SetHorizontalScroll(int value) { horizontal = value; }
    void SetVerticalScroll(int value) { vertical = value; }
    std::vector<std::pair<int, int>> sizes;
    std::vector<bool> focuses;
    int horizontal = 0;
    int vertical = 0;
};
class HTML : public Panel {
public:
    void BrowserResize();
#define MESSAGE_FUNC(name, message) void name()
#if defined(CSE_TEST_CS_HTML)
#include "actual_visibility_handler_cs.inc"
#else
#include "actual_visibility_handler.inc"
#endif
#undef MESSAGE_FUNC
    void GetSize(int& width, int& height) { width = fixtureWidth; height = fixtureHeight; }
    void SetTall(int value) { fixtureHeight = value; }
    void OnMessage(KeyValues* message, VPANEL) override {
        Check(message->name == "CseEffectiveVisibilityChanged", "only a private notification, never a command");
        OnCseEffectiveVisibilityChanged();
    }
    Serializer serializer;
    Serializer* m_Serializer = &serializer;
    int m_iScrollBorderX = 4;
    int m_iScrollBorderY = 6;
    int m_iWideLastHTMLSize = 0;
    int m_iTalLastHTMLSize = 0;
    ScrollBar horizontalWidget;
    ScrollBar verticalWidget;
    ScrollBar* _vbar = &verticalWidget;
    ScrollBar* _hbar = &horizontalWidget;
    int fixtureWidth = 100;
    int fixtureHeight = 80;
};

#if defined(CSE_TEST_CS_HTML)
#include "actual_visibility_cs.inc"
#else
#include "actual_visibility.inc"
#endif

int main() {
    Panel root;
    Panel nested;
    HTML browser;
    HTML hidden;
    nested.node.SetParent(&root.node);
    browser.node.SetParent(&nested.node);
    hidden.node.SetParent(&root.node);
    hidden.SetVisible(false);
    root.SetVisible(false);
    root.SetVisible(true);
    Check(!hidden.node.IsVisible(), "parent visibility preserves a browser's local hidden state");
    Check(browser.node.IsVisible(), "ancestor visibility does not change local child state");
    Check(!browser.serializer.sizes.empty(), "nested browser receives ancestor notifications");
    Check(browser.serializer.sizes.back() == std::make_pair(96, 74), "shown browser restores its retained dimensions");
    nested.node.SetVisible(false);
    Check(browser.serializer.sizes.back() == std::make_pair(0, 0), "direct IPanel visibility hides nested browser");
    Check(!browser.serializer.focuses.back(), "hidden browser relinquishes provider focus");
    browser.fixtureWidth = 200;
    browser.BrowserResize();
    Check(browser.serializer.sizes.back() == std::make_pair(0, 0), "hidden resize cannot wake provider");
    browser.node.SetParent(&root.node);
    Check(browser.serializer.sizes.back() == std::make_pair(196, 74), "reparenting restores effective visibility");
    Check(browser.serializer.horizontal == 7 && browser.serializer.vertical == 7, "scroll state is retained");
    browser.node.SetParent(&nested.node);
    Check(browser.serializer.sizes.back() == std::make_pair(0, 0), "reparenting into hidden ancestry hides browser");
    browser.node.SetParent(nullptr);
    Check(browser.serializer.sizes.back() == std::make_pair(196, 74), "detaching uses the panel's own visibility");
    const auto before = browser.serializer.sizes.size();
    browser.BrowserResize();
    Check(browser.serializer.sizes.size() == before, "unchanged visible dimensions remain cached");
    browser.m_Serializer = nullptr;
    browser.SetVisible(false);
    browser.BrowserResize();
    std::puts("PASS: actual panel hierarchy and browser visibility methods");
}
