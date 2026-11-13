#include <cstring>
#include "../../../src/engine/utlvector.h"
#include "../../../src/engine/utlrbtree.h"
#include "../../../src/engine/utllinkedlist.h"
#include "../../../src/cstrike/public/game_controls/utlstack.h"

bool Less(const int &left, const int &right) {
    return left < right;
}

int main(int argc, char **argv) {
    constexpr int InitialElements = 2;
    constexpr int FirstValue = 7;
    constexpr int SecondValue = 11;
    CUtlMemory<int> memory(0, InitialElements);
    memory[0] = FirstValue;
    memory[1] = SecondValue;
    CUtlVector<int> values;
    values.AddToTail(FirstValue);
    values.AddToTail(SecondValue);
    if (argc == 2 && std::strcmp(argv[1], "invalid-memory") == 0) {
        return memory[InitialElements];
    }
    if (argc == 2 && std::strcmp(argv[1], "invalid-vector") == 0) {
        return values[InitialElements];
    }
    if (argc == 2 && std::strcmp(argv[1], "invalid-tree") == 0) {
        CUtlRBTree<int> missingComparator;
        return missingComparator.Find(FirstValue);
    }
    CUtlLinkedList<int, unsigned short> list;
    const unsigned short item = list.AddToTail(FirstValue);
    CUtlStack<int> stack;
    stack.Push(FirstValue);
    if (argc == 2 && std::strcmp(argv[1], "invalid-list") == 0) {
        return list.Next(list.InvalidIndex());
    }
    if (argc == 2 && std::strcmp(argv[1], "invalid-stack") == 0) {
        stack.Pop();
        return stack.Top();
    }
    if (list[item] != FirstValue || stack.Top() != FirstValue) {
        return 1;
    }
    CUtlRBTree<int> tree(0, 0, Less);
    tree.Insert(SecondValue);
    tree.Insert(FirstValue);
    if (tree.Count() != InitialElements || tree[tree.FirstInorder()] != FirstValue
        || tree[tree.Find(SecondValue)] != SecondValue) {
        return 1;
    }
    memory.Grow(1);
    if (memory.NumAllocated() <= InitialElements || memory[0] != FirstValue || memory[1] != SecondValue
        || values.Count() != InitialElements || values[0] != FirstValue || values[1] != SecondValue) {
        return 1;
    }
    memory.Purge();
    values.Purge();
    return memory.NumAllocated() == 0 && values.Count() == 0 ? 0 : 1;
}
