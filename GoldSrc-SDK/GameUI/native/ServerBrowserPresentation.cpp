#include "ServerBrowserPresentation.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace cse::server_browser {
namespace {
struct Less {
    Sort sort;
    bool operator()(const Row &left, const Row &right) const {
        return RowPrecedes(left, right, sort);
    }
};
bool SameSort(Sort left, Sort right) {
    return left.column == right.column && left.ascending == right.ascending;
}
}

struct ServerBrowserPresentation::State {
    using Index = std::set<Row, Less>;
    using Rows = std::map<RowKey, Row>;
    Rows rows;
    Rows oldRows;
    std::unique_ptr<Index> active{new Index(Less{})};
    std::unique_ptr<Index> building;
    std::unique_ptr<Index> retired;
    std::map<RowKey, RowUpdate> deferred;
    std::optional<RowKey> buildCursor;
    std::optional<Row> anchor;
    std::optional<Row> buildingAnchor;
    std::optional<Sort> requestedSort;
    size_t rank = 0;
    size_t buildingRank = 0;
    bool resetPending = false;
    bool closing = false;

    void Erase(Index &index, const Row &row, size_t &before, const std::optional<Row> &boundary) {
        const auto found = index.find(row);
        if (found != index.end()) {
            if (boundary && index.key_comp()(row, *boundary)) {
                --before;
            }
            index.erase(found);
        }
    }
    void Insert(Index &index, const Row &row, size_t &before, const std::optional<Row> &boundary) {
        if (index.insert(row).second && boundary && index.key_comp()(row, *boundary)) {
            ++before;
        }
    }
    void Update(const RowUpdate &update) {
        const auto old = rows.find(update.row.key);
        if (old != rows.end()) {
            Erase(*active, old->second, rank, anchor);
            if (building) {
                Erase(*building, old->second, buildingRank, buildingAnchor);
            }
            rows.erase(old);
        }
        if (update.visible) {
            rows.emplace(update.row.key, update.row);
            Insert(*active, update.row, rank, anchor);
            if (building) {
                Insert(*building, update.row, buildingRank, buildingAnchor);
            }
        }
    }
};

ServerBrowserPresentation::ServerBrowserPresentation() : state_(new State) {}
ServerBrowserPresentation::~ServerBrowserPresentation() = default;

void ServerBrowserPresentation::Apply(const RowUpdate &update) {
    auto &state = *state_;
    if (state.closing) {
        return;
    }
    if (update.reset) {
        state.resetPending = true;
        // At most the previous bounded delivery batch is deferred, never a full discovered list.
        state.deferred.clear();
    } else if (state.resetPending) {
        state.deferred[update.row.key] = update;
    } else {
        state.Update(update);
    }
}

void ServerBrowserPresentation::RequestSort(Sort sort) {
    if (!state_->closing) {
        state_->requestedSort = sort;
    }
}

size_t ServerBrowserPresentation::Pump(size_t workBudget) {
    auto &state = *state_;
    size_t work = 0;
    while (work < workBudget) {
        if (state.retired) {
            if (!state.retired->empty()) {
                state.retired->erase(state.retired->begin());
                ++work;
            } else {
                state.retired.reset();
            }
        } else if (!state.oldRows.empty()) {
            state.oldRows.erase(state.oldRows.begin());
            ++work;
        } else if (state.resetPending) {
            if (state.building) {
                state.retired = std::move(state.building);
                state.buildCursor.reset();
            } else {
                const Sort sort = state.active->key_comp().sort;
                state.retired = std::move(state.active);
                state.active.reset(new State::Index(Less{sort}));
                state.oldRows.swap(state.rows);
                state.anchor.reset();
                state.rank = 0;
                state.buildingRank = 0;
                state.resetPending = false;
            }
        } else if (!state.deferred.empty()) {
            const auto entry = state.deferred.begin();
            if (!state.closing) {
                state.Update(entry->second);
            }
            state.deferred.erase(entry);
            ++work;
        } else if (state.building) {
            const auto row = state.buildCursor ? state.rows.upper_bound(*state.buildCursor) : state.rows.begin();
            if (row != state.rows.end()) {
                state.Insert(*state.building, row->second, state.buildingRank, state.buildingAnchor);
                state.buildCursor = row->first;
                ++work;
            } else {
                state.retired = std::move(state.active);
                state.active = std::move(state.building);
                state.rank = state.buildingRank;
                state.anchor = std::move(state.buildingAnchor);
                state.buildingAnchor.reset();
                state.buildCursor.reset();
            }
        } else if (state.requestedSort && !state.closing) {
            const Sort sort = *state.requestedSort;
            state.requestedSort.reset();
            if (!SameSort(sort, state.active->key_comp().sort)) {
                state.building.reset(new State::Index(Less{sort}));
                state.buildCursor.reset();
                state.buildingRank = 0;
                state.buildingAnchor = state.anchor;
            }
        } else {
            break;
        }
    }
    return work;
}

size_t ServerBrowserPresentation::VisibleCount() const {
    return state_->active->size();
}

std::optional<size_t> ServerBrowserPresentation::RankForUserAction(const RowKey &key) const {
    size_t rank = 0;
    for (const auto &row : *state_->active) {
        if (row.key == key) {
            return rank;
        }
        ++rank;
    }
    return std::nullopt;
}

void ServerBrowserPresentation::SeekForUserAction(size_t firstVisibleRank) {
    auto &state = *state_;
    if (state.active->empty() || firstVisibleRank == 0) {
        state.anchor.reset();
        state.rank = 0;
        state.buildingRank = 0;
        state.buildingAnchor.reset();
        return;
    }
    state.rank = std::min(firstVisibleRank, state.active->size() - 1);
    auto row = state.active->begin();
    std::advance(row, static_cast<State::Index::difference_type>(state.rank));
    state.anchor = *row;
    if (state.building) {
        state.buildingAnchor = state.anchor;
        state.buildingRank =
            static_cast<size_t>(std::distance(state.building->begin(), state.building->lower_bound(*state.anchor)));
    }
}

std::vector<Row> ServerBrowserPresentation::ReadViewport(size_t maxRows) {
    auto &state = *state_;
    std::vector<Row> result;
    if (maxRows == 0 || state.active->empty()) {
        return result;
    }
    auto cursor = state.anchor ? state.active->lower_bound(*state.anchor) : state.active->begin();
    if (cursor == state.active->end()) {
        // Keep a valid last page without walking the full index or recomputing a distance.
        size_t backwards = 0;
        while (cursor != state.active->begin() && backwards < maxRows) {
            --cursor;
            ++backwards;
        }
        // A building index owns a separate immutable boundary/count. Normalizing this active
        // viewport cannot require traversing that other ordering.
        state.anchor = *cursor;
        state.rank = state.active->size() - backwards;
    }
    result.reserve(std::min(maxRows, state.active->size()));
    while (cursor != state.active->end() && result.size() < maxRows) {
        result.push_back(*cursor++);
    }
    return result;
}

size_t ServerBrowserPresentation::FirstVisibleRank() const {
    return state_->rank;
}

Sort ServerBrowserPresentation::ActiveSort() const {
    return state_->active->key_comp().sort;
}

bool ServerBrowserPresentation::IsUpdating() const {
    return state_->building || state_->requestedSort || state_->resetPending || !state_->deferred.empty();
}

bool ServerBrowserPresentation::CanAcceptUpdates() const {
    return !state_->closing && !state_->resetPending && state_->deferred.empty();
}

bool ServerBrowserPresentation::BeginClose() {
    auto &state = *state_;
    if (!state.closing) {
        state.closing = true;
        state.resetPending = true;
        state.requestedSort.reset();
    }
    return !state.resetPending && !state.retired && state.oldRows.empty() && state.deferred.empty() &&
           state.active->empty() && !state.building;
}
}
