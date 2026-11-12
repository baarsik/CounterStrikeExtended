// Authored against the approved private presentation contract. Build separately only after this
// actual production component exists; absence of its header is not behavioral RED or qualification.
#include "ServerBrowserPresentation.h"
#include "ServerBrowserModel.h"
#include "check.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace cse::server_browser;

namespace {
constexpr size_t ServerCount = 10000;
constexpr size_t RowBudget = 128;
constexpr size_t ViewportRows = 37;
constexpr uint32_t AddressBase = 0x0a000001U;
constexpr uint16_t ConnectionPort = 27015;
constexpr uint16_t QueryPort = 27016;
constexpr AppId_t CurrentApp = 10;
constexpr uint64_t Generation = 1;

Row MakeRow(size_t index) {
    Row row;
    row.key = {AddressBase + static_cast<uint32_t>(index), ConnectionPort, QueryPort};
    row.app = CurrentApp;
    char name[32]{};
    std::snprintf(name, sizeof(name), "server-%05zu", index);
    row.name = name;
    row.gameDirectory = "cstrike";
    row.gameDescription = "Counter-Strike";
    row.map = "de_dust2";
    row.ping = static_cast<int>(ServerCount - index);
    row.players = 3;
    row.maxPlayers = 16;
    row.responded = true;
    return row;
}

void Maintenance(ServerBrowserPresentation &view) {
    Require(view.Pump(RowBudget) <= RowBudget, "presentation exceeded maintenance work budget");
}

void Fill(ServerBrowserPresentation &view) {
    size_t inserted = 0;
    while (inserted < ServerCount) {
        for (size_t step = 0; step < RowBudget && inserted < ServerCount; ++step) {
            const size_t index = ServerCount - inserted - 1;
            view.Apply({MakeRow(index), true, false, Generation});
            ++inserted;
        }
        Maintenance(view);
        const auto rows = view.ReadViewport(ViewportRows);
        Require(rows.size() <= ViewportRows, "viewport copied full discovered list");
        for (size_t index = 1; index < rows.size(); ++index) {
            Require(rows[index - 1].name < rows[index].name, "evolving name presentation is unsorted");
        }
    }
    Require(view.VisibleCount() == ServerCount, "presentation silently capped valid servers");
}

void Settle(ServerBrowserPresentation &view) {
    // Finite test ceiling covers active/building/retiring sets; no production timing assumption.
    constexpr size_t MaximumTurns = ServerCount;
    for (size_t turn = 0; turn < MaximumTurns; ++turn) {
        const size_t work = view.Pump(RowBudget);
        Require(work <= RowBudget, "sort/reclamation exceeded maintenance budget");
        if (work == 0) {
            return;
        }
    }
    throw std::runtime_error("presentation maintenance did not converge");
}

void CompleteReachability() {
    ServerBrowserPresentation view;
    Fill(view);
    view.SeekForUserAction(ServerCount - 1);
    const auto tail = view.ReadViewport(ViewportRows);
    Require(!tail.empty() && tail.front().name == "server-09999", "logical scrollbar cannot reach last result");
    view.SeekForUserAction(0);
    const auto head = view.ReadViewport(ViewportRows);
    Require(head.size() == ViewportRows && head.front().name == "server-00000", "logical Home lost first result");
}

void AnchorRankAfterOrdinaryUpdates() {
    ServerBrowserPresentation view;
    Fill(view);
    constexpr size_t AnchorRank = 5000;
    view.SeekForUserAction(AnchorRank);
    view.Apply({MakeRow(4), false, false, Generation});
    Maintenance(view);
    Require(view.FirstVisibleRank() == AnchorRank - 1, "removal before anchor lost logical rank");
    Require(view.ReadViewport(ViewportRows).front().name == "server-05000", "removal moved viewport identity");
    view.Apply({MakeRow(4), true, false, Generation});
    Maintenance(view);
    Require(view.FirstVisibleRank() == AnchorRank, "reinsertion before anchor lost logical rank");
    view.Apply({MakeRow(AnchorRank), false, false, Generation});
    Maintenance(view);
    Require(view.ReadViewport(ViewportRows).front().name == "server-05001", "removed anchor left dangling cursor");
    Require(view.FirstVisibleRank() == AnchorRank, "anchor replacement performed wrong rank adjustment");
}

void SortWithChangingMembership() {
    ServerBrowserPresentation view;
    Fill(view);
    view.RequestSort({Column::Ping, true});
    Maintenance(view);
    view.Apply({MakeRow(ServerCount - 1), false, false, Generation});
    Row changed = MakeRow(2);
    changed.ping = 0;
    view.Apply({changed, true, false, Generation});
    Settle(view);
    view.SeekForUserAction(0);
    const auto rows = view.ReadViewport(ViewportRows);
    Require(rows.front().key == changed.key, "pending-sort update did not replace changed row");
    Require(rows[1].name == "server-09998", "pending-sort removal survived as duplicate or stale row");
    Require(view.VisibleCount() == ServerCount - 1, "sort changed canonical membership count");
}

void UnknownFieldsFollowKnown() {
    ServerBrowserPresentation view;
    Row unknown = MakeRow(0);
    unknown.responded = false;
    Row known = MakeRow(1);
    view.Apply({unknown, true, false, Generation});
    view.Apply({known, true, false, Generation});
    view.RequestSort({Column::Ping, false});
    Settle(view);
    view.SeekForUserAction(0);
    const auto rows = view.ReadViewport(ViewportRows);
    Require(rows.size() == 2 && rows.front().key == known.key,
            "unknown fields moved ahead of known fields in descending sort");
}

void RapidSortIntentsConverge() {
    ServerBrowserPresentation view;
    Fill(view);
    view.RequestSort({Column::Ping, true});
    Maintenance(view);
    view.RequestSort({Column::Name, true});
    Maintenance(view);
    view.RequestSort({Column::Name, false});
    Settle(view);
    view.SeekForUserAction(0);
    Require(view.ReadViewport(ViewportRows).front().name == "server-09999",
            "coalesced sort did not converge to latest user intent");
    Require(view.VisibleCount() == ServerCount, "sort coalescing lost valid rows");
}

void RemovedEndAnchorDuringSort() {
    ServerBrowserPresentation view;
    Fill(view);
    view.SeekForUserAction(ServerCount - 1);
    view.RequestSort({Column::Ping, true});
    Require(view.Pump(1) <= 1, "single sort step exceeded budget");
    view.Apply({MakeRow(ServerCount - 1), false, false, Generation});
    const auto rows = view.ReadViewport(ViewportRows);
    Require(rows.size() == ViewportRows, "end removal failed to produce a final viewport");
    Require(view.FirstVisibleRank() == ServerCount - 1 - ViewportRows,
            "end clamp during sort left logical scrollbar rank beyond displayed window");
    Settle(view);
    view.SeekForUserAction(0);
    Require(view.ReadViewport(ViewportRows).front().name == "server-09998", "sort lost end-clamp updates");
}
void ExplicitRankUsesActiveOrdering() {
    ServerBrowserPresentation view;
    Fill(view);
    constexpr size_t SelectedRank = 123;
    const auto key = MakeRow(SelectedRank).key;
    Require(view.RankForUserAction(key) == SelectedRank, "explicit rank did not resolve offscreen key");
    view.RequestSort({Column::Name, false});
    Maintenance(view);
    Require(view.RankForUserAction(key) == SelectedRank, "navigation used incomplete requested ordering");
    Settle(view);
    Require(view.RankForUserAction(key) == ServerCount - SelectedRank - 1,
            "navigation did not adopt active ordering");
    view.Apply({MakeRow(SelectedRank), false, false, Generation});
    Require(!view.RankForUserAction(key), "removed key retained a navigation rank");
}

void OriginalColumnsUseCanonicalOrdering() {
    ServerBrowserPresentation view;
    auto first = MakeRow(1);
    auto second = MakeRow(2);
    first.bots = 9;
    first.lastPlayed = 100;
    second.bots = 1;
    second.lastPlayed = 200;
    view.Apply({first, true, false, Generation});
    view.Apply({second, true, false, Generation});
    view.RequestSort({Column::Bots, false});
    Settle(view);
    view.SeekForUserAction(0);
    Require(view.ReadViewport(2).front().key == first.key, "Bots order differs from canonical model ordering");
    view.RequestSort({Column::LastPlayed, false});
    Settle(view);
    view.SeekForUserAction(0);
    Require(view.ReadViewport(2).front().key == second.key, "History is not newest-first");
    first.lastPlayed = 300;
    view.Apply({first, true, false, Generation});
    Settle(view);
    view.SeekForUserAction(0);
    Require(view.ReadViewport(2).front().key == first.key, "History mutation did not update bounded ordering");
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    const std::string name = argv[1];
    return RunCase([&] {
        if (name == "original-columns") {
            OriginalColumnsUseCanonicalOrdering();
        } else if (name == "reachability") {
            CompleteReachability();
        } else if (name == "anchor-rank") {
            AnchorRankAfterOrdinaryUpdates();
        } else if (name == "sort-mutation") {
            SortWithChangingMembership();
        } else if (name == "unknown-order") {
            UnknownFieldsFollowKnown();
        } else if (name == "coalesced-sort") {
            RapidSortIntentsConverge();
        } else if (name == "end-clamp-sort") {
            RemovedEndAnchorDuringSort();
        } else if (name == "selected-rank") {
            ExplicitRankUsesActiveOrdering();
        } else {
            throw std::runtime_error("unknown presentation case");
        }
    });
}
