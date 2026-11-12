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
        if (name == "async-multiselect" || name == "refresh-cancel-reentry") {
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
                Require(!second && provider.refreshedIndexes == std::vector<int>{0},
                    "Stop during selected refresh allowed another provider call");
                callback->ServerResponded(request, 0);
                callback->ServerFailedToRespond(request, 1);
                session.model.Pump(1);
                Require(session.model.GetStatus().phase == Phase::Stopped, "late callback changed reentrant Stop");
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
