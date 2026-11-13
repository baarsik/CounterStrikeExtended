#pragma once

#include "ISystemModule.h"
#include <cstdlib>
#include <cstring>
#include <limits>

// Provider-local, non-throwing storage: legacy publishers have a void subscription
// slot, while the native subscription table reports allocation failure to consumers.
class BorrowedListeners {
public:
    BorrowedListeners() = default;
    BorrowedListeners(const BorrowedListeners &other) {
        if (other.m_Size) {
            m_Entries = static_cast<ISystemModule **>(std::malloc(other.m_Size * sizeof(*m_Entries)));
            if (m_Entries) {
                std::memcpy(m_Entries, other.m_Entries, other.m_Size * sizeof(*m_Entries));
                m_Size = other.m_Size;
                m_Capacity = other.m_Size;
            }
        }
    }
    BorrowedListeners &operator=(const BorrowedListeners &) = delete;
    ~BorrowedListeners() {
        std::free(m_Entries);
    }
    void Init() {
        clear();
    }
    void Clear(bool) {
        clear();
    }
    void clear() {
        m_Size = 0;
    }
    ISystemModule **begin() const {
        return m_Entries;
    }
    ISystemModule **end() const {
        return m_Entries ? m_Entries + m_Size : nullptr;
    }
    bool Contains(ISystemModule *module) const {
        for (std::size_t index = 0; index < m_Size; ++index) {
            if (m_Entries[index] == module) {
                return true;
            }
        }
        return false;
    }
    bool Add(ISystemModule *module) {
        if (Contains(module)) {
            return true;
        }
        if (m_Size == m_Capacity) {
            constexpr std::size_t InitialCapacity = 8;
            constexpr std::size_t GrowthFactor = 2;
            constexpr std::size_t MaxCapacity = std::numeric_limits<std::size_t>::max() / sizeof(*m_Entries);
            if (m_Capacity > MaxCapacity / GrowthFactor) {
                return false;
            }
            const std::size_t capacity = m_Capacity ? m_Capacity * GrowthFactor : InitialCapacity;
            auto *entries = static_cast<ISystemModule **>(std::realloc(m_Entries, capacity * sizeof(*m_Entries)));
            if (!entries) {
                return false;
            }
            m_Entries = entries;
            m_Capacity = capacity;
        }
        m_Entries[m_Size++] = module;
        return true;
    }
    void Remove(ISystemModule *module) {
        for (std::size_t index = 0; index < m_Size; ++index) {
            if (m_Entries[index] == module) {
                --m_Size;
                std::memmove(m_Entries + index, m_Entries + index + 1, (m_Size - index) * sizeof(*m_Entries));
                return;
            }
        }
    }

private:
    ISystemModule **m_Entries = nullptr;
    std::size_t m_Size = 0;
    std::size_t m_Capacity = 0;
};
