#pragma once

#include "ServerBrowserModel.h"

namespace cse::server_browser {
// A copied view only: provider membership, filtering, selection and requests stay with the model.
// Ordinary frame work uses bounded updates/maintenance and a geometry-sized viewport.
class ServerBrowserPresentation final {
public:
    ServerBrowserPresentation();
    ~ServerBrowserPresentation();
    ServerBrowserPresentation(const ServerBrowserPresentation &) = delete;
    ServerBrowserPresentation &operator=(const ServerBrowserPresentation &) = delete;
    void Apply(const RowUpdate &update);
    void RequestSort(Sort sort);
    size_t Pump(size_t workBudget);
    size_t VisibleCount() const;
    // Explicit user action only; may traverse O(n). Never call from ordinary frame/range updates.
    void SeekForUserAction(size_t firstVisibleRank);
    std::optional<size_t> RankForUserAction(const RowKey &key) const;
    std::vector<Row> ReadViewport(size_t maxRows);
    size_t FirstVisibleRank() const;
    Sort ActiveSort() const;
    bool IsUpdating() const;
    bool CanAcceptUpdates() const;
    bool BeginClose();

private:
    struct State;
    std::unique_ptr<State> state_;
};
}
