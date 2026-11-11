// Reuse the exact typed fixture prefix extracted from Task 1; its tests/main are not copied or run.
#include "model_fixture.inc"

#include "check.h"
#include <type_traits>
#include <utility>

namespace {
constexpr AppId_t CurrentApp = 10;
constexpr Address FavoriteAddress{0x7f000001U, 27015, 27016};
constexpr uint32_t PreservedTimestamp = 123456;

template <class StatusType, class = void> struct ActionErrorObservation {
    static std::string Read(const StatusType &status) {
        // Old owner conflates errors. Observe its real legacy result for initial behavioral RED.
        return status.error;
    }
};

template <class StatusType>
struct ActionErrorObservation<StatusType, std::void_t<decltype(std::declval<StatusType>().actionError)>> {
    static std::string Read(const StatusType &status) {
        return status.actionError;
    }
};

std::string ActionError(const ServerBrowserModel &model) {
    return ActionErrorObservation<Status>::Read(model.GetStatus());
}

void FailedActionThenExistingAdd() {
    Favorites favorites;
    favorites.records.push_back({CurrentApp, FavoriteAddress, k_unFavoriteFlagFavorite, PreservedTimestamp});
    ServerBrowserModel model({nullptr, &favorites});
    favorites.failing = true;
    Require(!model.AddFavorite(CurrentApp, FavoriteAddress), "controlled favorite read must fail");
    Require(!ActionError(model).empty(), "failed favorite must remain visible");
    favorites.failing = false;
    Require(model.AddFavorite(CurrentApp, FavoriteAddress), "existing favorite add must succeed");
    Require(ActionError(model).empty(), "successful existing-add must clear only obsolete action error");
    Require(favorites.records.front().lastPlayed == PreservedTimestamp, "no-op add changed prior timestamp");
}

void FailedActionThenAbsentRemove() {
    Favorites favorites;
    ServerBrowserModel model({nullptr, &favorites});
    favorites.failing = true;
    Require(!model.RemoveFavorite(CurrentApp, FavoriteAddress), "controlled favorite read must fail");
    favorites.failing = false;
    Require(model.RemoveFavorite(CurrentApp, FavoriteAddress), "absent favorite remove must succeed");
    Require(ActionError(model).empty(), "successful absent-remove must clear obsolete action error");
}

void QueryFailureSurvivesFavoriteWrite() {
    Provider provider;
    Favorites favorites;
    provider.failRequest = true;
    ServerBrowserModel model({&provider, &favorites});
    Require(!model.Refresh(ListKind::Internet, DefaultFilters()), "provider must refuse request");
    const std::string queryError = model.GetStatus().error;
    Require(!queryError.empty(), "query failure missing");
    Require(model.AddFavorite(CurrentApp, FavoriteAddress), "favorite write should succeed");
    Require(model.GetStatus().error == queryError, "favorite write erased unrelated query error");
    Require(ActionError(model).empty(), "successful write retained action error");
}

void ActionFailureSurvivesQueryCompletion() {
    Provider provider;
    Favorites favorites;
    provider.nextRows = {Server()};
    ServerBrowserModel model({&provider, &favorites});
    Require(model.Refresh(ListKind::Internet, DefaultFilters()), "query must start");
    favorites.failing = true;
    Require(!model.AddFavorite(CurrentApp, FavoriteAddress), "favorite read must fail");
    const std::string actionError = ActionError(model);
    provider.Complete();
    Drain(model);
    Require(ActionError(model) == actionError && !actionError.empty(), "query completion erased action failure");
    Require(model.GetStatus().error.empty(), "favorite failure polluted query status");
}

void ActionFailureSurvivesRefresh() {
    Provider provider;
    Favorites favorites;
    ServerBrowserModel model({&provider, &favorites});
    favorites.failing = true;
    Require(!model.RecordAcceptedConnection(CurrentApp, FavoriteAddress, PreservedTimestamp),
            "history write precondition must report read failure");
    const std::string actionError = ActionError(model);
    Require(model.Refresh(ListKind::Internet, DefaultFilters()), "query must start");
    Require(ActionError(model) == actionError && !actionError.empty(), "new query erased history action failure");
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    const std::string name = argv[1];
    return RunCase([&] {
        if (name == "existing-add") {
            FailedActionThenExistingAdd();
        } else if (name == "absent-remove") {
            FailedActionThenAbsentRemove();
        } else if (name == "query-preserved") {
            QueryFailureSurvivesFavoriteWrite();
        } else if (name == "completion-preserved") {
            ActionFailureSurvivesQueryCompletion();
        } else if (name == "refresh-preserved") {
            ActionFailureSurvivesRefresh();
        } else {
            throw std::runtime_error("unknown action-error case");
        }
    });
}
