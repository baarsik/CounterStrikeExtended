#include "model_fixture.inc"
#include "ServerBrowserDetails.h"
#include "ServerBrowserPresentation.h"
#include "check.h"
#include "detail_provider_fixture.h"
#include <algorithm>

namespace vgui2 {
enum KeyCode { KEY_LSHIFT, KEY_RSHIFT, KEY_LCONTROL, KEY_RCONTROL };
struct Input {
    bool shift = false;
    bool IsKeyDown(KeyCode code) const {
        return shift && (code == KEY_LSHIFT || code == KEY_RSHIFT);
    }
};
Input *input() {
    static Input instance;
    return &instance;
}
template <class T> struct DHANDLE {
    T *value = nullptr;
    T *Get() {
        return value;
    }
    T *operator->() {
        return value;
    }
    T *operator=(T *next) {
        value = next;
        return value;
    }
};
}
namespace cse::server_browser {
struct BrowserSession {
    struct Connection {
        bool CanConnect(const Row &row) const {
            return row.app == 10 && row.gameDirectory == "cstrike";
        }
    } connection;
    ServerBrowserModel model;
    ServerBrowserPresentation presentation;
    ISteamMatchmakingServers *servers;
    bool closing = false;
    bool active = true;
    explicit BrowserSession(ISteamMatchmakingServers *provider) : model({provider, nullptr}), servers(provider) {}
};
struct BrowserUse {
    explicit BrowserUse(BrowserSession &) {}
};
class BrowserDetailsDialog;
class ServerBrowserDialog {
public:
    explicit ServerBrowserDialog(BrowserSession &session) : session_(session) {}
    ~ServerBrowserDialog();
    void SelectVisible(size_t slot);
    void SelectVisibleItems(const std::vector<size_t> &slots);
    void MoveSelection(int delta, bool endpoint = false);
    void ScrollViewport(int delta);
    void ReconcileSelection();
    size_t WindowSize() const {
        return 2;
    }
    void ConnectSelection();
    BrowserDetailsDialog *OpenDetails();
    void UpdateRows() {
        visible_ = session_.presentation.ReadViewport(2);
    }
    void ShowActionStatus(const char *token) {
        actionStatus_ = token;
    }
    BrowserSession &session_;
    std::vector<Row> visible_;
    std::vector<RowKey> selectedKeys_;
    std::optional<RowKey> keyboardCursor_;
    std::optional<RowKey> rangeAnchor_;
    std::vector<vgui2::DHANDLE<BrowserDetailsDialog>> detailsDialogs_;
    bool restoreSelection_ = false;
    bool transitioning_ = false;
    std::string actionStatus_;
};
// Only the VGUI shell is substituted. The child metadata request and source model are real owners.
class BrowserDetailsDialog {
public:
    BrowserDetailsDialog(ServerBrowserDialog &, BrowserSession &session, const Row &row)
        : row_(row), query_(session.servers) {}
    bool Closing() const {
        return false;
    }
    RowKey Key() const {
        return row_.key;
    }
    void Activate() {
        if (!activated_) {
            activated_ = true;
            query_.Start(row_.key);
        }
    }
    void Connect() {
        connecting_ = true;
    }
    Row row_;
    ServerBrowserDetails query_;
    bool activated_ = false;
    bool connecting_ = false;
};
ServerBrowserDialog::~ServerBrowserDialog() {
    for (auto &dialog : detailsDialogs_) {
        delete dialog.Get();
    }
}
#include "secondary_actions.inc"
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    const std::string name = argv[1];
    return RunCase([&] {
#if defined(_WIN32)
        const FixtureWinsockContext winsock;
#endif
        DetailProvider provider;
        for (uint32_t index = 0; index < 6; ++index) {
            provider.lists.nextRows.push_back(Server(0x7f000001U + index, ("Server " + std::to_string(index)).c_str()));
        }
        BrowserSession session(&provider);
        Require(session.model.Refresh(ListKind::Internet, DefaultFilters()), "list refresh did not start");
        const auto request = provider.lists.current;
        for (int index = 0; index < 6; ++index) {
            provider.lists.requests.at(request).callback->ServerResponded(request, index);
        }
        for (int turn = 0; turn < 20; ++turn) {
            session.model.Pump(128);
            for (const auto &update : session.model.TakeUpdates(128)) {
                session.presentation.Apply(update);
            }
            session.presentation.Pump(128);
        }
        ServerBrowserDialog dialog(session);
        dialog.UpdateRows();
        dialog.SelectVisible(0);
        const auto keys = session.model.OrderedKeys();
        if (name == "filter-selection" || name == "filter-partial") {
            dialog.SelectVisibleItems({0, 1});
            auto filters = DefaultFilters();
            filters.name = name == "filter-partial" ? "Server 1" : "absent";
            session.model.SetFilters(filters);
            // The view rebuild has not run: actions must already respect the new membership.
            Require(!session.model.Selection(), "hidden primary selection survived filter change");
            dialog.ConnectSelection();
            Require(dialog.detailsDialogs_.empty(), "Enter opened a filtered-out endpoint");
            dialog.ReconcileSelection();
            const std::vector<RowKey> expected = name == "filter-partial" ? std::vector<RowKey>{keys[1]} :
                std::vector<RowKey>{};
            Require(dialog.selectedKeys_ == expected, "filter reconciliation retained hidden batch members");
            Require(!dialog.rangeAnchor_, "filter retained a hidden Shift anchor");
            if (name == "filter-selection") {
                Require(!dialog.keyboardCursor_, "filter retained hidden keyboard cursor");
                session.model.SetFilters(DefaultFilters());
                dialog.ReconcileSelection();
                Require(dialog.selectedKeys_.empty(), "relaxing filters resurrected hidden selection");
            } else {
                Require(session.model.Selection() == std::optional<RowKey>{keys[1]},
                    "remaining visible selection did not become actionable");
            }
        } else if (name == "wheel-selection" || name == "wheel-shift" || name == "wheel-offscreen") {
            if (name == "wheel-offscreen") {
                dialog.MoveSelection(1, true);
                session.presentation.SeekForUserAction(0);
                dialog.UpdateRows();
            }
            const auto selected = dialog.selectedKeys_;
            const auto cursor = dialog.keyboardCursor_;
            const auto anchor = dialog.rangeAnchor_;
            vgui2::input()->shift = name == "wheel-shift";
            dialog.ScrollViewport(-1);
            Require(session.presentation.FirstVisibleRank() == 3, "wheel followed selection instead of viewport");
            Require(dialog.selectedKeys_ == selected && dialog.keyboardCursor_ == cursor && dialog.rangeAnchor_ == anchor,
                "wheel changed selection, keyboard cursor, or Shift anchor");
            dialog.ScrollViewport(-1);
            Require(session.presentation.FirstVisibleRank() == 4, "wheel scrolled beyond last full page");
            dialog.ScrollViewport(100);
            Require(session.presentation.FirstVisibleRank() == 0 && dialog.selectedKeys_ == selected,
                "wheel endpoint clamp changed selection");
        } else if (name == "stopped-quick-all" || name == "stopped-quick-selected" ||
                   name == "info-quick-selected" || name == "fallback-cancel" ||
                   name == "fallback-wrong-endpoint" || name == "fallback-failed-start" ||
                   name == "fallback-restart" || name == "fallback-bounded") {
            if (name == "info-quick-selected") {
                dialog.OpenDetails();
                // Isolate the independently owned GameInfo ping from list refresh pings.
                for (auto &child : dialog.detailsDialogs_) {
                    delete child.Get();
                }
                dialog.detailsDialogs_.clear();
            } else {
                session.model.Stop();
            }
            for (int turn = 0; turn < 20; ++turn) {
                session.model.Pump(128);
            }
            const int before = provider.pings;
            const bool all = name == "stopped-quick-all" || name == "fallback-bounded";
            Require(all ? session.model.QuickRefresh() : session.model.QuickRefresh(keys[0]),
                "retired list request disabled quick refresh");
            if (!all) {
                Require(session.model.QuickRefresh(keys[0]), "pending selected refresh did not coalesce");
            }
            if (name == "fallback-cancel") {
                provider.duringPing = [&] { session.model.Stop(); };
            }
            if (name == "fallback-failed-start") {
                provider.failPing = true;
            }
            session.model.Pump(name == "fallback-bounded" ? 1 : 128);
            if (name == "fallback-bounded") {
                Require(provider.pings == before + 1 && session.model.GetStatus().pending >= keys.size(),
                    "fallback exceeded one unit of query-start work or lost pending rows");
                session.model.Stop();
                while (!session.model.CloseStep(1)) {
                    Require(session.model.GetStatus().lastReclamationWork <= 1,
                        "fallback retirement exceeded node budget");
                }
            } else if (name == "fallback-cancel") {
                Require(session.model.GetStatus().phase == Phase::Stopped && provider.pingCallbacks.empty(),
                    "Stop reentry left a fallback callback owner live");
            } else if (name == "fallback-failed-start") {
                session.model.Pump(128);
                Require(session.model.GetStatus().phase == Phase::Complete && session.model.Find(keys[0])->failed,
                    "invalid ping handle left fallback refresh running forever");
            } else {
                Require(provider.pings == before + (all ? static_cast<int>(keys.size()) : 1),
                    "fallback queried the wrong number of endpoints");
                Require(provider.pingPorts.back() == keys[0].queryPort, "fallback confused query and connection port");
                auto callbacks = provider.pingCallbacks;
                for (const auto &entry : callbacks) {
                    auto item = Server(name == "fallback-wrong-endpoint" ? 0x01020304U :
                        (all ? keys[static_cast<size_t>(entry.first - callbacks.begin()->first)].ip : keys[0].ip),
                        "Updated server");
                    entry.second->ServerResponded(item);
                }
                session.model.Pump(128);
                Require(session.model.GetStatus().phase == Phase::Complete && provider.pingCallbacks.empty(),
                    "completed fallback did not retire its callback owners");
                Require(session.model.Find(keys[0])->key == keys[0], "fallback retargeted selected endpoint");
                if (name == "fallback-wrong-endpoint") {
                    Require(session.model.Find(keys[0])->name == "Server 0" && session.model.Find(keys[0])->failed,
                        "wrong-endpoint ping replaced the selected row");
                } else {
                    Require(session.model.Find(keys[0])->name == "Updated server", "fallback result was not applied");
                }
                if (name == "fallback-restart") {
                    Require(session.model.QuickRefresh(), "second fallback refresh rejected");
                    session.model.Pump(1);
                    session.model.Stop();
                    Require(session.model.QuickRefresh(keys[0]), "Stop left queued fallback work blocking restart");
                    session.model.Pump(128);
                    Require(provider.pingCallbacks.size() == 1, "selected restart revived unrelated queued rows");
                }
            }
        } else if (name == "async-multiselect" || name == "refresh-cancel-reentry") {
            provider.lists.Complete();
            provider.deferListRefresh = true;
            if (name == "refresh-cancel-reentry") {
                provider.duringListRefresh = [&] { session.model.Stop(); };
            }
            auto *callback = provider.lists.requests.at(request).callback;
            const bool first = session.model.QuickRefresh(keys[0]);
            const bool second = session.model.QuickRefresh(keys[1]);
            if (name == "async-multiselect") {
                Require(first && second && provider.refreshedIndexes == std::vector<int>{0, 1},
                    "selected asynchronous refresh did not submit both identities before responses");
                Require(session.model.QuickRefresh(keys[0]) && provider.refreshedIndexes.size() == 2,
                    "repeated pending identity submitted duplicate query");
                callback->ServerResponded(request, 2);
                Require(session.model.GetStatus().phase == Phase::Refreshing, "unrelated callback completed batch");
                callback->ServerFailedToRespond(request, 1);
                Require(session.model.GetStatus().phase == Phase::Refreshing, "first batch callback completed all work");
                callback->ServerResponded(request, 0);
                Require(session.model.GetStatus().phase == Phase::Complete, "last batch callback did not complete");
            } else {
                Require(second && provider.refreshedIndexes == std::vector<int>{0},
                    "Stop did not switch the next selected refresh to the request-free fallback");
                callback->ServerResponded(request, 0);
                callback->ServerFailedToRespond(request, 1);
                session.model.Pump(1);
                Require(session.model.GetStatus().phase == Phase::Refreshing,
                    "late list callback changed new fallback refresh state");
            }
        } else if (name == "shift-range") {
            vgui2::input()->shift = true;
            dialog.MoveSelection(1);
            dialog.MoveSelection(1);
            Require(dialog.selectedKeys_ == std::vector<RowKey>{keys[0], keys[1], keys[2]},
                "second Shift+Down did not advance cursor across two-row viewport");
            Require(session.model.Selection() == std::optional<RowKey>{keys[0]}, "range extension changed first selection");
            dialog.MoveSelection(-1);
            Require(dialog.selectedKeys_ == std::vector<RowKey>{keys[0], keys[1]}, "Shift+Up failed to shrink range");
            dialog.ConnectSelection();
            Require(dialog.detailsDialogs_.front()->Key() == keys[0], "Connect followed keyboard cursor instead of first row");
        } else if (name == "shift-reverse") {
            vgui2::input()->shift = true;
            dialog.MoveSelection(1);
            dialog.MoveSelection(-1);
            Require(dialog.selectedKeys_ == std::vector<RowKey>{keys[0]}, "range reversal retained old tail");
        } else if (name == "info-stop" || name == "connect-stop") {
            Require(session.model.GetStatus().phase == Phase::Refreshing, "fixture source was not refreshing");
            if (name == "info-stop") {
                dialog.OpenDetails();
            } else {
                dialog.ConnectSelection();
            }
            Require(session.model.GetStatus().phase == Phase::Stopped && provider.lists.canceled == 1,
                "opening GameInfo did not stop the source refresh");
            Require(dialog.detailsDialogs_.front()->Key() == keys[0], "source Stop lost captured endpoint");
            Require(provider.pings == 1 && provider.pingPorts.front() == keys[0].queryPort,
                "source Stop canceled or retargeted independent GameInfo ping");
        } else {
            throw std::runtime_error("unknown secondary action case");
        }
        vgui2::input()->shift = false;
        while (!session.model.CloseStep(128)) {}
        while (!session.presentation.BeginClose()) {
            session.presentation.Pump(128);
        }
    });
}
