#include <cstring>
#include "../../src/engine/utlmemory.h"
#include <cassert>

int main() {
    constexpr int initialElements = 2;
    constexpr int firstValue = 7;
    constexpr int secondValue = 11;
    CUtlMemory<int> memory(0, initialElements);
    memory[0] = firstValue;
    memory[1] = secondValue;
    if (memory[0] != firstValue || memory[1] != secondValue || memory.IsIdxValid(initialElements)) {
        return 1;
    }
    memory.Grow(1);
    if (memory.NumAllocated() <= initialElements || memory[0] != firstValue || memory[1] != secondValue) {
        return 2;
    }
    memory.Purge();
    return memory.NumAllocated() == 0 ? 0 : 3;
}
