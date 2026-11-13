#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <memory>
#include <stdexcept>
#include <vector>

// Strings remain contiguous individually, while adding a later string never moves
// previously returned cells. Offsets retain the old localization table contract.
template<class Cell>
class LocalizationStorage {
public:
    explicit LocalizationStorage(int = 0) {}
    int Count() const {
        return count_;
    }
    int AddMultipleToTail(int length) {
        if (length <= 0 || length > INT_MAX - count_) {
            throw std::length_error("localization storage capacity");
        }
        const int offset = count_;
        blocks_.push_back(Block{offset, length, std::unique_ptr<Cell[]>(new Cell[length]{})});
        count_ += length;
        return offset;
    }
    Cell& operator[](int index) {
        return const_cast<Cell&>(static_cast<const LocalizationStorage&>(*this)[index]);
    }
    const Cell& operator[](int index) const {
        const auto after = std::upper_bound(blocks_.begin(), blocks_.end(), index,
            [](int value, const Block& block) { return value < block.offset; });
        assert(after != blocks_.begin());
        const auto& block = *std::prev(after);
        assert(index >= block.offset && index < block.offset + block.length);
        return block.cells[index - block.offset];
    }
    void RemoveAll() {
        blocks_.clear();
        count_ = 0;
    }
    void Purge() {
        RemoveAll();
    }
private:
    struct Block {
        int offset;
        int length;
        std::unique_ptr<Cell[]> cells;
    };
    std::vector<Block> blocks_;
    int count_ = 0;
};
