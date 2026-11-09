#pragma once
#include "../common/checked_native_index.h"

namespace cse {
namespace bsp {
// The submodel's visleafs count excludes the leading solid leaf at index zero.
template<class Leaf>
bool CheckedWorldLeafIndex(const Leaf* leaf, const Leaf* begin, int visibleLeaves, int& result) {
    if (visibleLeaves < 0) {
        return false;
    }
    return native::CheckedIndex(leaf, begin, static_cast<std::size_t>(visibleLeaves) + 1, result);
}
}
}
