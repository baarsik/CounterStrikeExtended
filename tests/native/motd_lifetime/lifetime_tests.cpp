#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// Controlled bindings provide VGUI registration/queues, filesystem I/O, action
// delivery, and browser transport. All close, owner navigation, detachment, and
// cleanup methods under test are extracted unchanged from production.
class KeyValues {
public:
    explicit KeyValues(const char* value) : name(value) {}
    std::string name;
};

struct CUtlString {
    ~CUtlString() { std::free(m_pchString); }
    char* m_pchString = nullptr;
};

class IHTMLResponses {};
class IHTMLResponses_HL1 {
public:
    virtual void BrowserClose() = 0;
protected:
    ~IHTMLResponses_HL1() = default;
};

class IHTMLSerializer {
public:
    int BrowserGetIndex() { return ready_ ? 1 : -1; }
    int GetBrowserIndex() { return BrowserGetIndex(); }
    IHTMLResponses* GetResponseTarget() { return reinterpret_cast<IHTMLResponses*>(this); }
    std::uintptr_t GetBrowserHandle() { return reinterpret_cast<std::uintptr_t>(GetResponseTarget()); }
    void PostURL(const char* url, const char*) { navigation = url; }
    #include "detach.inc"
    IHTMLResponses_HL1* responses_ = nullptr;
    bool ready_ = true;
    std::string navigation;
};

struct IHTMLChromeController {
    void RemoveBrowser(IHTMLResponses* target) {
        if (!serializer || target != serializer->GetResponseTarget()) {
            std::abort();
        }
        ++removals;
        serializer->Detach();
    }
    IHTMLSerializer* serializer = nullptr;
    int removals = 0;
};

namespace vgui2 {
using VPANEL = std::uintptr_t;
class Panel;
struct Record {
    Panel* panel = nullptr;
    VPANEL parent = 0;
    std::vector<VPANEL> children;
};
std::map<VPANEL, Record> records;
std::map<VPANEL, int> frees;
std::vector<VPANEL> pendingDeletes;
int closeEvents = 0;
bool running = true;
IHTMLChromeController fixtureController;

class Panel {
public:
    explicit Panel(Panel* parent = nullptr);
    virtual ~Panel();
    VPANEL GetVPanel() { return _vpanel; }
    void SetParent(VPANEL parent);
    void MarkForDeletion();
    void PostActionSignal(KeyValues* event) {
        if (event->name != "OnCloseWindow") {
            std::abort();
        }
        ++closeEvents;
        delete event;
    }
    VPANEL _vpanel = reinterpret_cast<VPANEL>(this);
    bool _autoDelete = true;
    bool _markedForDeletion = false;
    char* _panelName = nullptr;
};

void Panel::SetParent(VPANEL parent) {
    auto& record = records.at(_vpanel);
    if (record.parent) {
        auto& children = records.at(record.parent).children;
        children.erase(std::remove(children.begin(), children.end(), _vpanel), children.end());
    }
    record.parent = parent;
    if (parent) {
        records.at(parent).children.push_back(_vpanel);
    }
}

Panel::Panel(Panel* parent) {
    records.emplace(_vpanel, Record{});
    records.at(_vpanel).panel = this;
    SetParent(parent ? parent->GetVPanel() : 0);
}

struct IPanel {
    int GetChildCount(VPANEL handle) { return static_cast<int>(records.at(handle).children.size()); }
    VPANEL GetChild(VPANEL handle, int index) {
        return records.at(handle).children.at(static_cast<std::size_t>(index));
    }
    bool IsAutoDeleteSet(VPANEL handle) { return records.at(handle).panel->_autoDelete; }
    void DeletePanel(VPANEL handle) { delete records.at(handle).panel; }
    void SetParent(VPANEL child, VPANEL parent) { records.at(child).panel->SetParent(parent); }
};
IPanel* ipanel() {
    static IPanel service;
    return &service;
}

struct IVGui {
    bool IsRunning() { return running; }
    void MarkPanelForDeletion(VPANEL handle) { pendingDeletes.push_back(handle); }
    void FreePanel(VPANEL handle) {
        ++frees[handle];
        records.erase(handle);
    }
};
IVGui* ivgui() {
    static IVGui service;
    return &service;
}

struct Surface {
    IHTMLChromeController* AccessChromeHTMLController() { return &fixtureController; }
};
Surface* surface() {
    static Surface service;
    return &service;
}

class HTML : public Panel, public IHTMLResponses_HL1 {
public:
    HTML(Panel* parent, IHTMLSerializer* serializer) : Panel(parent), m_Serializer(serializer) {
        m_pContextMenu = new Panel(this);
        serializer->responses_ = this;
    }
    ~HTML() override;
    void BrowserClose() override;
    virtual void OpenURL(const char* url, const char* postData, bool force);
    IHTMLSerializer* m_Serializer = nullptr;
    Panel* m_pContextMenu = nullptr;
    CUtlString m_sPendingURLLoad;
    CUtlString m_sPendingPostData;
};

class Frame : public Panel {
public:
    void Activate() {}
    void SetTitle(const char*, bool) {}
    void SetControlString(const char*, const char*) {}
};

struct FileSystem {
    bool FileExists(const char*) { return !content.empty(); }
    void RemoveFile(const char*, const char*) { content.clear(); }
    void* Open(const char*, const char*, const char*) { return this; }
    void Write(const void* data, int bytes, void*) { content.assign(static_cast<const char*>(data), bytes); }
    void Close(void*) {}
    void GetLocalPath(const char*, char* output, int bytes) {
        std::snprintf(output, static_cast<std::size_t>(bytes), "C:/fixture/motd_temp.html");
    }
    std::string content;
};
FileSystem* filesystem() {
    static FileSystem service;
    return &service;
}
}

struct GameUIFuncs {
    int GetVGUI2KeyCodeForBind(const char*) { return 1; }
};
GameUIFuncs gameUIService;
GameUIFuncs* gameuifuncs = &gameUIService;
using FileHandle_t = void*;
using std::strlen;
using std::strstr;

class CClientMOTD : public vgui2::Frame {
public:
    explicit CClientMOTD(IHTMLSerializer* serializer) {
        m_pMessage = new vgui2::HTML(this, serializer);
    }
    ~CClientMOTD() override;
    bool IsURL(const char* text);
    void Activate(const char* title, const char* message);
    vgui2::HTML* m_pMessage = nullptr;
    char m_szTempFileName[4096] = "motd_temp.html";
    int m_iScoreBoardKey = 0;
};

#ifdef CSE_MOTD_CS
#include "cs_lifetime.inc"
#else
#include "shared_lifetime.inc"
#endif
#include "owner.inc"

void Drain() {
    while (!vgui2::pendingDeletes.empty()) {
        const auto handle = vgui2::pendingDeletes.back();
        vgui2::pendingDeletes.pop_back();
        auto found = vgui2::records.find(handle);
        if (found != vgui2::records.end()) {
            delete found->second.panel;
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    IHTMLSerializer serializer;
    vgui2::fixtureController.serializer = &serializer;
    CClientMOTD* owner = new CClientMOTD(&serializer);
    vgui2::HTML* child = owner->m_pMessage;
    const auto ownerHandle = owner->GetVPanel();
    const auto childHandle = child->GetVPanel();
    const auto menuHandle = child->m_pContextMenu->GetVPanel();
    IHTMLResponses* stableTarget = serializer.GetResponseTarget();
    if (std::strcmp(argv[1], "gui-stopped") == 0) {
        vgui2::running = false;
    }
    auto* responses = serializer.CloseResponses();
    responses->BrowserClose();
    Drain();
    const bool childAlive = vgui2::records.count(childHandle) == 1;
    bool passed = childAlive && vgui2::closeEvents == 1 && vgui2::fixtureController.removals == 0 &&
        serializer.responses_ == nullptr && serializer.GetResponseTarget() == stableTarget;
    if (!childAlive) {
        std::fprintf(stderr, "MOTD retained child was deleted by browser-close notification\n");
    } else if (std::strcmp(argv[1], "owner-destroy") != 0) {
        const bool html = std::strcmp(argv[1], "close-html") == 0;
        const char* payload = html ? "<!DOCTYPE html><p>Full MOTD</p>" : "http://fixture.invalid/motd";
        owner->Activate("fixture", payload);
        const char* expectedURL = html ? "file:///C:/fixture/motd_temp.html" : "http://fixture.invalid/motd";
        passed = child->m_sPendingURLLoad.m_pchString &&
            std::strcmp(child->m_sPendingURLLoad.m_pchString, expectedURL) == 0 && passed;
        if (html) {
            passed = vgui2::filesystem()->content == payload && passed;
        }
    }
    vgui2::running = true;
    delete owner;
    passed = vgui2::frees[ownerHandle] == 1 && vgui2::frees[childHandle] == 1 &&
        vgui2::records.count(menuHandle) == 1 && vgui2::frees[menuHandle] == 0 &&
        std::find(vgui2::pendingDeletes.begin(), vgui2::pendingDeletes.end(), menuHandle) !=
            vgui2::pendingDeletes.end() && passed;
    Drain();
    passed = vgui2::records.empty() && vgui2::fixtureController.removals == 1 &&
        vgui2::frees[ownerHandle] == 1 && vgui2::frees[childHandle] == 1 &&
        vgui2::frees[menuHandle] == 1 && passed;
    if (!passed) {
        return 1;
    }
    std::puts("motd_lifetime_passed=1");
    return 0;
}
