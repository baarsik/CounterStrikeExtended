#include "model_fixture.inc"
#include "check.h"
#include <type_traits>
#include <utility>

namespace {
constexpr size_t Rows = 10000;
constexpr size_t Budget = 128;
constexpr int MaximumSteps = 2000;
template <class Model, class = void> struct CloseObservation {
    static bool Step(Model &model, size_t) {
        return model.BeginClose();
    }
};
template <class Model> struct CloseObservation<Model, std::void_t<decltype(std::declval<Model>().CloseStep(Budget))>> {
    static bool Step(Model &model, size_t budget) {
        return model.CloseStep(budget);
    }
};
template <class State, class = void> struct StorageObservation {
    static size_t Nodes(const State &state) {
        return state.rows;
    }
    static size_t Generations(const State &) {
        return 0;
    }
    static size_t Reclaimed(const State &) {
        return 0;
    }
};
template <class State> struct StorageObservation<State, std::void_t<decltype(std::declval<State>().retainedNodes)>> {
    static size_t Nodes(const State &state) {
        return state.retainedNodes;
    }
    static size_t Generations(const State &state) {
        return state.retainedGenerations;
    }
    static size_t Reclaimed(const State &state) {
        return state.lastReclamationWork;
    }
};
void Populate(Provider &provider, ServerBrowserModel &model) {
    provider.nextRows.reserve(Rows);
    for (size_t index = 0; index < Rows; ++index) {
        provider.nextRows.push_back(Server(0x7f000001U + static_cast<uint32_t>(index)));
    }
    Require(model.Refresh(ListKind::Internet, DefaultFilters()), "start real large model");
    provider.Complete();
    Drain(model);
    Require(model.GetStatus().rows == Rows, "large model setup dropped rows");
}
void Finish(ServerBrowserModel &model) {
    for (int step = 0; step < MaximumSteps; ++step) {
        const size_t before = StorageObservation<Status>::Nodes(model.GetStatus());
        const bool finished = CloseObservation<ServerBrowserModel>::Step(model, Budget);
        const auto status = model.GetStatus();
        const size_t after = StorageObservation<Status>::Nodes(status);
        Require(after <= before && before - after <= Budget, "close reclaimed more than aggregate node budget");
        Require(StorageObservation<Status>::Reclaimed(status) == before - after,
                "reported reclamation does not match actual container population change");
        if (finished) {
            Require(after == 0, "close allowed nonempty production model destruction");
            return;
        }
    }
    throw std::runtime_error("bounded model close failed to progress");
}
}
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    const std::string name = argv[1];
    return RunCase([&] {
        Provider provider;
        ServerBrowserModel model({&provider, nullptr});
        Populate(provider, model);
        if (name == "stopped-request") {
            Require(!model.GetStatus().requestRetired, "active request reported retirement");
            model.Stop();
            Require(!model.GetStatus().requestRetired, "stopped request storage skipped bounded retirement");
            size_t turns = 0;
            while (!model.GetStatus().requestRetired && turns < MaximumSteps) {
                Require(model.Pump(Budget) <= Budget, "hidden model retirement exceeded budget");
                ++turns;
            }
            Require(model.GetStatus().requestRetired && turns > 1, "stopped request did not retire incrementally");
            Require(model.GetStatus().rows == Rows && model.GetStatus().phase == Phase::Stopped,
                    "ordinary retirement destroyed copied rows or changed stopped identity");
            Require(provider.canceled == 1 && provider.released == 1, "ordinary retirement repeated cancellation");
            Finish(model);
        } else if (name == "bounded-close") {
            Require(!CloseObservation<ServerBrowserModel>::Step(model, Budget),
                    "large model authorized destruction in one close attempt");
            Require(provider.canceled == 1 && provider.released == 1, "provider handle not retired once");
            Finish(model);
            Require(provider.canceled == 1 && provider.released == 1, "retry repeated provider cancellation");
        } else if (name == "coalesced-replacement") {
            for (int index = 0; index < 200; ++index) {
                model.SetSort({index % 2 ? Column::Name : Column::Ping, index % 2 == 0});
                Require(model.Refresh(index % 2 ? ListKind::Favorites : ListKind::Internet, DefaultFilters()),
                        "valid replacement intent was lost");
                model.Stop();
                Require(StorageObservation<Status>::Generations(model.GetStatus()) <= 2,
                        "repeated intents retained unbounded request/root generations");
                Require(provider.requests.size() <= 1, "replacement created concurrent provider requests");
            }
            Require(!CloseObservation<ServerBrowserModel>::Step(model, Budget),
                    "replacement storage was freed eagerly or hidden from close");
            Finish(model);
        } else if (name == "latest-replacement") {
            auto filters = DefaultFilters();
            filters.app = 70;
            filters.gameDirectory = "valve";
            auto next = Server(0x0b000001U, "Latest retained game");
            next.m_nAppID = filters.app;
            std::strcpy(next.m_szGameDir, filters.gameDirectory.c_str());
            provider.nextRows = {next};
            constexpr int ReplacementIntents = 200;
            for (int index = 0; index < ReplacementIntents; ++index) {
                Require(model.Refresh(index % 2 ? ListKind::Favorites : ListKind::History, filters),
                        "replacement intent rejected during bounded drain");
            }
            Require(model.Refresh(ListKind::History, filters), "latest history intent rejected");
            Require(provider.requests.empty(), "replacement started before old storage retired");
            for (int step = 0; step < MaximumSteps && provider.requests.empty(); ++step) {
                Require(model.Pump(Budget) <= Budget, "pending replacement exceeded frame budget");
            }
            Require(provider.requests.size() == 1 && provider.lastKind == ListKind::History && provider.lastApp == 70,
                    "coalesced replacement lost the latest kind/app");
            provider.Complete();
            Drain(model);
            Require(model.OrderedKeys().size() == 1 && model.Find({0x0b000001U, 27015, 27016}) != nullptr,
                    "latest replacement dropped legitimate row data");
            Finish(model);
        } else if (name == "cancel-reentry") {
            provider.cancelCallbacks = true;
            Require(!CloseObservation<ServerBrowserModel>::Step(model, Budget), "large cancellation skipped drain");
            Require(model.IsQuiescent(), "canceled callback owner remained active after provider return");
            Finish(model);
        } else {
            throw std::runtime_error("unknown model retirement case");
        }
    });
}
