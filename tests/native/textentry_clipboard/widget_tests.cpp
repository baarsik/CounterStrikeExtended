// Complete production declarations and actual target bodies. Base/UI behavior
// is explicitly controlled; actual VPanel + Panel message-map delivery is optional.
#include "declarations.cpp"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

constexpr int BUFFER_SIZE = 999999; // Retained TextEntry line-break sentinel.
struct Unexpected : std::runtime_error {
    using std::runtime_error::runtime_error;
};
void RejectUnexpected(const char* message);
// Imported source assertions have a controlled sink. A bad fixture/mutant fails
// here; it must never invoke the Windows CRT assertion dialog on the desktop.
#ifdef ASSERT_AT
#undef ASSERT_AT
#endif
#define ASSERT_AT(expression, line) \
    do { \
        if (!(expression)) { \
            throw Unexpected("actual assertion: " #expression); \
        } \
    } while (false)
using vgui2::Panel;
using vgui2::VPANEL;
using Cells = std::vector<wchar_t>;
static std::vector<std::string> fixtureEvents;
static std::vector<Cells> fixturePublications;
static Cells fixtureClipboard;
static int fixtureHint = -1;
static Panel* fixtureActive = nullptr;
static VPANEL fixtureFocused = 0;
static bool fixtureRealMap = false;
static int keyAllocations = 0;
static int keyFrees = 0;
static std::map<void*, int> liveKeys;
static int deliveries = 0;
static int readCalls = 0;
static int reportOverride = INT_MIN;
static int writeLimit = INT_MAX;
static int throwReadAt = -1;
static int textAllocations = 0;
static int textReleases = 0;
static int failTextAllocation = -1;
static std::map<void*, std::size_t> textBlocks;
static std::vector<Cells> snapshots;
static std::deque<VPANEL> focusAnswers;
static std::vector<vgui2::KeyCode> pressedKeys;
static std::map<Panel*, VPANEL> fixtureParents;
static std::map<Panel*, VPANEL> fixtureNextPanels;
static std::map<VPANEL, Panel*> fixtureClients;
static std::deque<std::string> fixtureSymbols;
static int steamInitializerCalls = 0;

void* ControlledTextAllocate(std::size_t bytes) {
    ++textAllocations;
    if (textAllocations == failTextAllocation) {
        return nullptr;
    }
    constexpr std::size_t FixtureAllocationLimit = 1024 * 1024;
    if (bytes > FixtureAllocationLimit) {
        throw Unexpected("unexpected large owned allocation");
    }
    void* memory = std::malloc(bytes);
    if (memory) {
        std::memset(memory, 0xa5, bytes); // Authored bytes, never indeterminate storage.
        textBlocks[memory] = bytes;
    }
    return memory;
}

void ControlledTextRelease(void* memory) {
    if (textBlocks.erase(memory) != 1) {
        throw Unexpected("duplicate/unowned text release");
    }
    ++textReleases;
    std::free(memory);
}

VPANEL CreateActualVPanel(vgui2::IClientPanel* client);
void DestroyActualVPanel(VPANEL handle);
void SendViaActualVPanel(VPANEL target, KeyValues* message, VPANEL sender);

static void Event(Panel* panel, const char* name) {
    fixtureEvents.push_back(std::string(name) + ":" + panel->GetName());
}

static void ApiEvent(const char* name) {
    fixtureEvents.push_back(std::string(name) + ":" + (fixtureActive ? fixtureActive->GetName() : "constructor"));
}

static void LayoutCall(Panel*, const char*) {
    // Explicitly controlled layout/repaint; no renderer/host provider execution.
}

static VPANEL ParentOf(Panel* panel) {
    return fixtureParents.at(panel);
}

static void AdvanceFocus(Panel* panel) {
    fixtureFocused = fixtureNextPanels[panel];
}

static VPANEL QueryFocus() {
    if (!focusAnswers.empty()) {
        const VPANEL answer = focusAnswers.front();
        focusAnswers.pop_front();
        return answer;
    }
    return fixtureFocused;
}

static bool KeyDown(vgui2::KeyCode code) {
    return std::find(pressedKeys.begin(), pressedKeys.end(), code) != pressedKeys.end();
}

static int CountHint() {
    return fixtureHint == -1 ? static_cast<int>(fixtureClipboard.size()) : fixtureHint;
}

static int ReadClipboard(int offset, wchar_t* destination, int bytes) {
    if (offset != 0 || bytes < 0 || bytes % sizeof(wchar_t) != 0) {
        throw Unexpected("read boundary");
    }
    const int capacity = bytes / static_cast<int>(sizeof(wchar_t));
    fixtureEvents.push_back("Get" + std::to_string(capacity) + ":" + fixtureActive->GetName());
    const Cells& snapshot = snapshots.empty() ? fixtureClipboard
        : snapshots[std::min(static_cast<std::size_t>(readCalls), snapshots.size() - 1)];
    ++readCalls;
    if (readCalls == throwReadAt) {
        throw Unexpected("controlled reader exception");
    }
    const int copied = std::min(capacity, static_cast<int>(snapshot.size()));
    std::copy_n(snapshot.data(), std::min(copied, writeLimit), destination);
    return reportOverride == INT_MIN ? copied : reportOverride;
}

static void PublishClipboard(const wchar_t* text, int count) {
    if (count < 0) {
        throw Unexpected("negative publication");
    }
    ApiEvent("Set");
    fixturePublications.emplace_back(text, text + count);
    fixtureClipboard = fixturePublications.back();
    fixtureClipboard.push_back(0);
}

static int SymbolFor(const char* name) {
    for (std::size_t i = 0; i < fixtureSymbols.size(); ++i) {
        if (fixtureSymbols[i] == name) {
            return static_cast<int>(i);
        }
    }
    fixtureSymbols.emplace_back(name);
    return static_cast<int>(fixtureSymbols.size() - 1);
}

static const char* StringFor(int symbol) {
    return fixtureSymbols.at(symbol).c_str();
}

static void* AllocateKeyValues(int size) {
    ++keyAllocations;
    void* result = std::malloc(size);
    if (!result) {
        throw std::bad_alloc();
    }
    liveKeys[result] = size;
    return result;
}

static void FreeKeyValues(void* memory) {
    if (liveKeys.erase(memory) != 1) {
        throw Unexpected("duplicate/unowned KeyValues release");
    }
    ++keyFrees;
    std::free(memory);
}

static void DeliverMessage(VPANEL target, KeyValues* message, VPANEL sender);

namespace vgui2 {
#include "controlled_services.inc"
static ControlledISystem controlledSystem;
static ControlledIPanel controlledPanels;
static ControlledISurface controlledSurface;
static ControlledIInput controlledInput;
static ControlledIKeyValues controlledKeyValues;
ISystem* system() {
    return &controlledSystem;
}
IPanel* ipanel() {
    return &controlledPanels;
}
ISurface* surface() {
    return &controlledSurface;
}
IInput* input() {
    return &controlledInput;
}
IVGui* ivgui() {
    RejectUnexpected("unexpected IVGui diagnostic");
    return nullptr;
}

PanelMessageMap* FindOrAddPanelMessageMap(const char* name) {
    // Dictionary storage is controlled. Entry registration/dispatch are actual.
    static std::map<std::string, std::unique_ptr<PanelMessageMap>> maps;
    auto& map = maps[name];
    if (!map) {
        map.reset(new PanelMessageMap());
    }
    return map.get();
}

Panel::Panel(Panel* parent, const char* name)
    : _vpanel(0), _cursor(0), _markedForDeletion(false), _border(nullptr), _needsRepaint(false),
      _buildGroup(nullptr), _paintBorderEnabled(true), _paintBackgroundEnabled(true), _paintEnabled(true),
      _postChildPaintEnabled(false), _panelName(const_cast<char*>(name)), _needsLayout(false),
      _needsSchemeUpdate(false), _autoDelete(false), _tabPosition(0), _pinCorner(PIN_TOPLEFT),
      _autoResizeDirection(AUTORESIZE_NO), m_iScheme(0), _buildModeFlags(0), m_bProportional(false),
      m_bInPerformLayout(false), m_pTooltips(nullptr), m_eIsHDProportional(HD_PROPORTIONAL_USE_PARENT) {
    _vpanel = CreateActualVPanel(this);
    fixtureClients[_vpanel] = this;
    fixtureParents[this] = parent ? parent->GetVPanel() : 0;
}

static void UnregisterPanel(Panel* panel) {
    const VPANEL handle = panel->GetVPanel();
    fixtureClients.erase(handle);
    fixtureParents.erase(panel);
    fixtureNextPanels.erase(panel);
    DestroyActualVPanel(handle);
}

#include "unrelated_methods.inc"
#include "actual_textentry.inc"
} // namespace vgui2

IKeyValues* keyvalues() {
    return &vgui2::controlledKeyValues;
}
#include "keyvalues_base.inc"
#include "actual_keyvalues.inc"
#include "actual_prepare_map.inc"
namespace vgui2 {
#include "actual_onmessage.inc"
}

struct ActiveScope {
    Panel* previous;
    explicit ActiveScope(Panel* panel) : previous(fixtureActive) {
        fixtureActive = panel;
    }
    ~ActiveScope() {
        fixtureActive = previous;
    }
};

struct ActualWidget : vgui2::TextEntry {
    using vgui2::TextEntry::TextEntry;
    void Paste() override {
        ActiveScope scope(this);
        vgui2::TextEntry::Paste();
    }
    void CopySelected() override {
        ActiveScope scope(this);
        vgui2::TextEntry::CopySelected();
    }
    void SaveUndoState() override {
        Event(this, "Undo");
        vgui2::TextEntry::SaveUndoState();
    }
    void InsertChar(wchar_t value) override {
        fixtureEvents.push_back("Insert" + std::to_string(static_cast<unsigned>(value)) + ":" + GetName());
        vgui2::TextEntry::InsertChar(value);
    }
};

template<class Tag, typename Tag::Member member>
struct AccessActual {
    friend typename Tag::Member Access(Tag) {
        return member;
    }
};
struct TextTag {
    using Member = CUtlVector<wchar_t> vgui2::TextEntry::*;
    friend Member Access(TextTag);
};
struct SelectionTag {
    using Member = int (vgui2::TextEntry::*)[2];
    friend Member Access(SelectionTag);
};
struct DirtyTag {
    using Member = bool vgui2::TextEntry::*;
    friend Member Access(DirtyTag);
};
struct CursorTag {
    using Member = int vgui2::TextEntry::*;
    friend Member Access(CursorTag);
};
template struct AccessActual<TextTag, &vgui2::TextEntry::m_TextStream>;
template struct AccessActual<SelectionTag, &vgui2::TextEntry::_select>;
template struct AccessActual<DirtyTag, &vgui2::TextEntry::_dataChanged>;
template struct AccessActual<CursorTag, &vgui2::TextEntry::_cursorPos>;

static void SetCells(ActualWidget& widget, const Cells& cells) {
    auto& stream = widget.*Access(TextTag{});
    stream.RemoveAll();
    for (wchar_t cell : cells) {
        stream.AddToTail(cell);
    }
    widget.*Access(CursorTag{}) = stream.Count();
    widget.*Access(DirtyTag{}) = false;
}

static Cells GetCells(ActualWidget& widget) {
    auto& stream = widget.*Access(TextTag{});
    return Cells(stream.Base(), stream.Base() + stream.Count());
}

static void DeliverMessage(VPANEL target, KeyValues* message, VPANEL sender) {
    if (++deliveries > 12) {
        throw Unexpected("controlled no-progress recursion stop");
    }
    if (!target || fixtureClients.find(target) == fixtureClients.end()) {
        throw Unexpected("invalid controlled route");
    }
    ApiEvent("Send");
    if (fixtureRealMap) {
        SendViaActualVPanel(target, message, sender);
    } else {
        // This branch proves actual Paste under controlled direct routing only.
        dynamic_cast<ActualWidget*>(fixtureClients.at(target))->Paste();
    }
    if (liveKeys.find(message) == liveKeys.end()) {
        throw Unexpected("receiver unexpectedly consumed caller-owned message");
    }
}

extern "C" void CseSteamModuleInitialize() {
    // Typed forced owning-context binding: no actual provider or host startup.
    ++steamInitializerCalls;
}

static void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static void Trace(std::initializer_list<const char*> expected) {
    std::vector<std::string> rows;
    for (const char* entry : expected) {
        rows.emplace_back(entry);
    }
    Check(fixtureEvents == rows, "exact actual-method API/focus/action order");
}

static void Select(ActualWidget& widget, int begin, int end) {
    auto& selection = widget.*Access(SelectionTag{});
    selection[0] = begin;
    selection[1] = end;
}

int main(int argc, char** argv) {
    try {
        const std::string test = argc > 1 ? argv[1] : "copy";
#if defined(CSE_OWNED_STEAM_PROVIDER)
        Check(steamInitializerCalls > 0, "forced initializer reaches controlled binding");
#else
        Check(steamInitializerCalls == 0, "unselected initializer remains absent");
#endif
        Panel parent(nullptr, "parent");
        ActualWidget widget(&parent, "A");
        // The fixed actual constructor now defines this bool in both owners.
        if (test == "fresh") {
            Check(!(widget.*Access(DirtyTag{})), "actual constructor initializes dirty flag");
        }
        SetCells(widget, {L'A', L'\n', L'B'});
        fixtureEvents.clear();
        fixturePublications.clear();
        if (test == "copy") {
            Select(widget, 0, 3);
            widget.CopySelected();
            Check(fixturePublications == std::vector<Cells>{{L'A', L'\r', L'\n', L'B'}},
                  "CopySelected must publish all CR-expanded cells");
            Trace({"Set:A", "Focus:A"});
        } else if (test == "ordinary") {
            SetCells(widget, {});
            fixtureClipboard = {L'A', L'B', 0};
            widget.Paste();
            Check(GetCells(widget) == Cells{L'A', L'B'}, "ordinary actual Paste text");
            Check(keyAllocations == keyFrees, "actual KeyValues allocation/free balance");
            Trace({"Count:A", "Count:A", "Get3:A", "Undo:A", "Insert65:A", "Insert66:A",
                   "Action:A", "Focus:A"});
        } else if (test == "fresh") {
            widget.CopySelected();
            Trace({"Focus:A"});
        } else if (test == "copy-multiple" || test == "copy-CRLF" || test == "copy-reversed"
                   || test == "copy-empty" || test == "copy-none" || test == "copy-dirty"
                   || test == "copy-invalid") {
            Cells expected;
            if (test == "copy-multiple") {
                SetCells(widget, {L'\n', L'\n', L'\n'});
                Select(widget, 0, 3);
                expected = {L'\r', L'\n', L'\r', L'\n', L'\r', L'\n'};
            } else if (test == "copy-CRLF") {
                SetCells(widget, {L'A', L'\r', L'\n', L'B'});
                Select(widget, 0, 4);
                expected = {L'A', L'\r', L'\r', L'\n', L'B'};
            } else if (test == "copy-reversed") {
                Select(widget, 3, 0);
                expected = {L'A', L'\r', L'\n', L'B'};
            } else if (test == "copy-empty") {
                Select(widget, 0, 0);
            } else if (test == "copy-dirty") {
                Select(widget, 0, 1);
                widget.*Access(DirtyTag{}) = true;
                expected = {L'A'};
            } else if (test == "copy-invalid") {
                Select(widget, -2, INT_MAX);
            }
            widget.CopySelected();
            if (test == "copy-none" || test == "copy-invalid") {
                Check(fixturePublications.empty(), "invalid/no-selection never publishes");
                Trace({"Focus:A"});
            } else {
                Check(fixturePublications == std::vector<Cells>{expected}, "complete selected cells");
                if (test == "copy-dirty") {
                    Trace({"Set:A", "Focus:A", "Action:A"});
                } else {
                    Trace({"Set:A", "Focus:A"});
                }
            }
        } else if (test == "hidden" || test == "uneditable") {
            widget.SetTextHidden(test == "hidden");
            widget.SetEditable(test != "uneditable");
            fixtureEvents.clear();
            widget.Paste();
            Check(fixtureEvents.empty(), "hidden/uneditable Paste has no clipboard callbacks");
            if (test == "hidden") {
                widget.CopySelected();
                Check(fixtureEvents.empty(), "hidden Copy has no callbacks");
            }
        } else if (test == "allocation" || test == "grow-allocation" || test == "copy-allocation") {
            SetCells(widget, {});
            fixtureClipboard = {L'A', L'B', L'C', 0};
            fixtureHint = test == "grow-allocation" ? 1 : 4;
            failTextAllocation = test == "grow-allocation" ? 2 : 1;
            if (test == "copy-allocation") {
                SetCells(widget, {L'A'});
                Select(widget, 0, 1);
                widget.CopySelected();
                Trace({"Focus:A"});
            } else {
                widget.Paste();
                Check(GetCells(widget).empty(), "allocation failure has no insertion");
                if (test == "grow-allocation") {
                    Trace({"Count:A", "Count:A", "Get1:A"});
                } else {
                    Trace({"Count:A", "Count:A"});
                }
            }
            Check(fixturePublications.empty(), "allocation failure never publishes");
        } else if (test == "identity") {
            if (sizeof(VPANEL) > sizeof(std::uint32_t)) {
                Check(widget.GetVPanel() > UINT32_MAX, "actual high native VPanel allocation");
            }
            std::cout << "actual-vpanel=" << widget.GetVPanel() << " wchar-bytes=" << sizeof(wchar_t)
                      << " controlled-init-calls=" << steamInitializerCalls << "\n";
        } else if (test == "max" || test == "max-long" || test == "INTMAX" || test == "hint-zero-max") {
            SetCells(widget, {});
            widget.SetMaximumCharCount(test == "INTMAX" ? INT_MAX : 2);
            fixtureClipboard = test == "max-long" ? Cells{L'A', L'B', L'C', L'D', 0} : Cells{L'A', L'B', 0};
            if (test == "hint-zero-max") {
                fixtureHint = 0;
            }
            widget.Paste();
            if (test == "INTMAX") {
                Check(GetCells(widget).empty(), "INTMAX has no text mutation");
                Trace({"Count:A"});
            } else {
                Check(GetCells(widget) == Cells{L'A', L'B'}, "bounded complete accepted prefix");
                Trace({"Count:A", "Get3:A", "Undo:A", "Insert65:A", "Insert66:A", "Action:A", "Focus:A"});
            }
        } else if (test == "read-zero" || test == "read-negative" || test == "hint-zero"
                   || test == "hint-negative" || test == "hint-overflow" || test == "auto-hint-overflow"
                   || test == "short" || test == "no-write" || test == "empty"
                   || test == "over-report" || test == "changed") {
            SetCells(widget, {});
            fixtureClipboard = {L'A', L'B', 0};
            fixtureHint = 8;
            if (test == "read-zero" || test == "read-negative") {
                reportOverride = test == "read-zero" ? 0 : -1;
            } else if (test == "hint-zero") {
                fixtureHint = 0;
            } else if (test == "hint-negative") {
                fixtureHint = -7;
            } else if (test == "hint-overflow" || test == "auto-hint-overflow") {
                fixtureHint = INT_MAX;
                widget.SetAutoProgressOnHittingCharLimit(test == "auto-hint-overflow");
            } else if (test == "short") {
                fixtureClipboard = {L'A', L'B'};
            } else if (test == "no-write") {
                reportOverride = 3;
                writeLimit = 0;
            } else if (test == "empty") {
                fixtureClipboard = {0};
            } else if (test == "over-report") {
                fixtureHint = 3;
                reportOverride = INT_MAX;
            } else if (test == "changed") {
                fixtureHint = 1;
                snapshots = {{L'A'}, {L'X', L'Y', L'Z', 0}, {L'X', L'Y', L'Z', 0}};
            }
            widget.Paste();
            if (test == "over-report") {
                Check(GetCells(widget) == Cells{L'A', L'B'}, "overreported count stays bounded");
            } else if (test == "changed") {
                Check(GetCells(widget) == Cells{L'X', L'Y', L'Z'}, "whole final snapshot, no joined prefixes");
                Check(readCalls == 3, "growth calls");
            } else {
                Check(GetCells(widget).empty(), "no invalid or uninitialized text inserted");
                if (test == "empty" || test == "no-write") {
                    Trace({"Count:A", "Count:A", "Get8:A", "Undo:A", "Action:A", "Focus:A"});
                } else if (test == "hint-zero" || test == "hint-negative" || test == "hint-overflow") {
                    Trace({"Count:A", "Count:A"});
                } else if (test == "auto-hint-overflow") {
                    Trace({"Count:A"});
                } else {
                    Trace({"Count:A", "Count:A", "Get8:A"});
                }
            }
            Check(fixturePublications.empty(), "non-auto failure/empty does not publish");
        } else if (test == "tail") {
            SetCells(widget, {});
            widget.SetAutoProgressOnHittingCharLimit(true);
            widget.SetMaximumCharCount(9);
            fixtureClipboard = {L'A', L'B', 0};
            fixtureClipboard.resize(17, 0x5a5a); // Every baseline cell written; no indeterminate read.
            widget.Paste();
            Check(GetCells(widget) == Cells{L'A', L'B'}, "tail text");
            Check(fixturePublications == std::vector<Cells>{{L'A', L'B'}}, "restore logical cells only");
        } else if (test == "grow") {
            SetCells(widget, {});
            fixtureClipboard = {L'A', L'B', L'C', 0};
            fixtureHint = 1;
            widget.Paste();
            Check(GetCells(widget) == Cells{L'A', L'B', L'C'}, "complete underestimated Unicode prefix");
        } else if (test == "ctrl-v" || test == "shift-insert" || test == "direct-selection") {
            SetCells(widget, {L'A', L'B', L'C'});
            Select(widget, 1, 2);
            fixtureClipboard = {L'X', 0};
            if (test == "direct-selection") {
                widget.Paste();
                Check(GetCells(widget) == Cells{L'A', L'B', L'C', L'X'},
                      "direct Paste does not secretly delete existing selection");
            } else {
                pressedKeys = {test == "ctrl-v" ? vgui2::KEY_LCONTROL : vgui2::KEY_LSHIFT};
                ActiveScope scope(&widget);
                widget.OnKeyCodeTyped(test == "ctrl-v" ? vgui2::KEY_V : vgui2::KEY_INSERT);
                Check(GetCells(widget) == Cells{L'A', L'X', L'C'}, "keyboard deletes selection exactly once");
                Trace({"Undo:A", "Count:A", "Count:A", "Get2:A", "Undo:A", "Insert88:A",
                       "Action:A", "Focus:A"});
            }
        } else if (test == "chain" || test == "map" || test == "cycle" || test == "cycle-zero" || test == "null"
                   || test == "progress-cycle" || test == "exception"
                   || test == "self" || test == "no-parent" || test == "focus-null") {
            ActualWidget peer(&parent, "B");
            SetCells(widget, {});
            SetCells(peer, {});
            widget.SetAutoProgressOnHittingCharLimit(true);
            peer.SetAutoProgressOnHittingCharLimit(true);
            widget.SetMaximumCharCount(2);
            peer.SetMaximumCharCount(8);
            fixtureNextPanels[&widget] = peer.GetVPanel();
            fixtureNextPanels[&peer] = widget.GetVPanel();
            fixtureRealMap = test == "map";
            if (test == "progress-cycle") {
                peer.SetMaximumCharCount(2);
            }
            if (test == "exception") {
                fixtureRealMap = true;
                throwReadAt = 2; // Peer throws during actual synchronous map delivery.
            }
            fixtureClipboard = {L'A', L'B', L'C', L'D', L'E', 0};
            if (test == "cycle" || test == "cycle-zero") {
                SetCells(widget, {L'A', L'B'});
                SetCells(peer, {L'X', L'Y'});
                peer.SetMaximumCharCount(2);
                if (test == "cycle-zero") {
                    SetCells(widget, {});
                    SetCells(peer, {});
                    widget.SetMaximumCharCount(0);
                    peer.SetMaximumCharCount(0);
                }
                fixtureFocused = peer.GetVPanel();
            }
            if (test == "null") {
                fixtureNextPanels[&widget] = 0;
            } else if (test == "self") {
                fixtureNextPanels[&widget] = widget.GetVPanel();
            } else if (test == "no-parent") {
                fixtureParents[&widget] = 0;
            } else if (test == "focus-null") {
                focusAnswers = {peer.GetVPanel(), 0};
            }
            if (test == "exception") {
                bool caught = false;
                try {
                    widget.Paste();
                } catch (const Unexpected&) {
                    caught = true;
                }
                Check(caught && liveKeys.empty() && textBlocks.empty(), "exception releases message/buffers");
                Check(fixtureClipboard == Cells{L'C', L'D', L'E', 0},
                      "external callback exception is not a clipboard rollback promise");
                throwReadAt = -1;
                widget.SetAutoProgressOnHittingCharLimit(false);
                widget.SetMaximumCharCount(-1);
                SetCells(widget, {});
                fixtureClipboard = {L'Z', 0};
                widget.Paste();
                Check(GetCells(widget) == Cells{L'Z'}, "exception pops every active frame");
            } else {
                widget.Paste();
            }
            if (test == "chain" || test == "map") {
                Check(GetCells(widget) == Cells{L'A', L'B'}, "chain first text");
                Check(GetCells(peer) == Cells{L'C', L'D', L'E'}, "chain remaining text");
                Check(fixturePublications == std::vector<Cells>{{L'C', L'D', L'E'}, {L'C', L'D', L'E'},
                                                         {L'A', L'B', L'C', L'D', L'E'}},
                      "exact chain/remainder/restoration cells");
                Check(keyAllocations == keyFrees, "synchronous message caller owns deletion");
                Trace({"Count:A", "Get6:A", "Undo:A", "Insert65:A", "Insert66:A", "Next:A", "Next:A",
                       "Set:A", "QueryFocus:A", "QueryFocus:A", "Send:A", "Count:B", "Get4:B", "Undo:B",
                       "Insert67:B", "Insert68:B", "Insert69:B", "Set:B", "Action:B", "Focus:B",
                       "Set:A", "Action:A"});
            } else if (test == "progress-cycle") {
                Check(GetCells(widget) == Cells{L'A', L'B'} && GetCells(peer) == Cells{L'C', L'D'},
                      "progressing reentry allowed before no-progress stop");
                Check(deliveries == 4 && fixtureClipboard == Cells{L'A', L'B', L'C', L'D', L'E', 0},
                      "strictly smaller reentry, then equal-length rejection and complete restore");
            } else if (test == "exception") {
                Check(deliveries == 1, "actual receiver exception observed exactly once");
            } else if (test == "cycle" || test == "cycle-zero") {
                Check(deliveries <= 2, "no-progress cycle bounded by active panels");
                widget.SetAutoProgressOnHittingCharLimit(false);
                widget.SetMaximumCharCount(-1);
                SetCells(widget, {});
                fixtureClipboard = {L'Z', 0};
                widget.Paste();
                Check(GetCells(widget) == Cells{L'Z'}, "active frames removed after full unwind");
            } else {
                Check(deliveries == 0, "no null/self/no-parent route");
            }
        } else if (test == "filter" || test == "multiline" || test == "numeric"
                   || test == "unicode" || test == "unicode-blocked" || test == "wrap") {
            SetCells(widget, {});
            fixtureClipboard = {L'A', L'\r', L'\n', L'\t', L'B', 0};
            Cells expected{L'A', L'B'};
            if (test == "multiline") {
                widget.SetMultiline(true);
                expected = {L'A', L'\n', L'B'};
            } else if (test == "numeric") {
                widget.SetAllowNumericInputOnly(true);
                fixtureClipboard = {L'1', L'x', L'.', L'2', 0};
                expected = {L'1', L'.', L'2'};
            } else if (test == "unicode" || test == "unicode-blocked") {
                widget.SetAllowNonAsciiCharacters(test == "unicode");
                fixtureClipboard = {0xd83d, 0xde00, 0, L'Z'};
                expected = test == "unicode" ? Cells{0xd83d, 0xde00} : Cells{};
            } else if (test == "wrap") {
                widget.SetWrap(true);
            }
            widget.Paste();
            Check(GetCells(widget) == expected, "actual unchanged InsertChar/filter result");
        } else {
            throw Unexpected("unknown test");
        }
        Check(keyAllocations == keyFrees && liveKeys.empty(), "all actual KeyValues ownership balanced");
        Check(textBlocks.empty(), "all caller text storage released");
        std::cout << "PASS actual-owner " << test << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << "\n";
        for (const auto& event : fixtureEvents) {
            std::cerr << event << "\n";
        }
        return 1;
    }
}
