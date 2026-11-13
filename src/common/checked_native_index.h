#pragma once
#include <climits>
#include <cstddef>
#include <cstdint>

namespace cse {
namespace native {
inline bool CheckedIndexFromAddress(std::uintptr_t element, std::uintptr_t base, std::size_t stride,
    std::size_t count, int& result) {
    if (base == 0 || element < base || stride == 0) {
        return false;
    }
    const std::uintptr_t distance = element - base;
    if (distance % stride != 0) {
        return false;
    }
    const std::uintptr_t index = distance / stride;
    if (index >= count || index > static_cast<std::uintptr_t>(INT_MAX)) {
        return false;
    }
    result = static_cast<int>(index);
    return true;
}

template<class T>
bool CheckedIndex(const T* element, const T* base, std::size_t count, int& result) {
    return CheckedIndexFromAddress(reinterpret_cast<std::uintptr_t>(element),
        reinterpret_cast<std::uintptr_t>(base), sizeof(T), count, result);
}
}
}
