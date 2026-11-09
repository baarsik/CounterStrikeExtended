#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <clocale>
#include <climits>
#include <initializer_list>

using ucs2 = unsigned short;
class CLocalizedStringTable {
public:
    int ConvertANSIToUnicode(const char*, wchar_t*, int);
    int ConvertUnicodeToANSI(const wchar_t*, char*, int);
};
#include "actual_functions.inc"
#include "corpus.h"

namespace {
constexpr std::size_t BufferCells = 64;
constexpr std::size_t BeforeCells = 8;
constexpr int MaximumCapacity = 24;
struct Invocation {
    int operation;
    const char* utf8;
    const wchar_t* wide;
    void* destination;
    int capacity;
    int sourceBytes;
    int result;
    DWORD error;
    DWORD exception;
};

void Invoke(Invocation* invocation) {
    CLocalizedStringTable table;
    SetLastError(0);
    __try {
        if (invocation->operation == 0) {
            invocation->result = table.ConvertANSIToUnicode(invocation->utf8,
                static_cast<wchar_t*>(invocation->destination), invocation->capacity);
        } else if (invocation->operation == 1) {
            invocation->result = table.ConvertUnicodeToANSI(invocation->wide,
                static_cast<char*>(invocation->destination), invocation->capacity);
        } else {
            invocation->result = Q_UCS2ToUTF8(reinterpret_cast<const ucs2*>(invocation->wide), invocation->sourceBytes,
                static_cast<char*>(invocation->destination), invocation->capacity);
        }
        invocation->error = GetLastError();
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        invocation->exception = GetExceptionCode();
    }
}

void Observe(int operation, int index, int capacity, bool nullSource, bool nullDestination, int sourceBytes) {
    wchar_t wideDestination[BufferCells];
    char byteDestination[BufferCells];
    for (std::size_t i = 0; i < BufferCells; ++i) {
        wideDestination[i] = 0x5a5a;
        byteDestination[i] = 0x5a;
    }
    Invocation invocation{};
    invocation.operation = operation;
    invocation.utf8 = nullSource ? nullptr : Utf8Inputs[index];
    invocation.wide = nullSource ? nullptr : reinterpret_cast<const wchar_t*>(WideInputs[operation == 0 ? 0 : index]);
    invocation.destination = nullDestination ? nullptr : operation == 0 ?
        static_cast<void*>(wideDestination + BeforeCells) : static_cast<void*>(byteDestination + BeforeCells);
    invocation.capacity = capacity;
    invocation.sourceBytes = sourceBytes;
    invocation.result = -999;
    Invoke(&invocation);
    std::printf("%d,%d,%d,%d,%d,%d,%d,%lu,%lu,", operation, index, capacity, nullSource ? 1 : 0,
        nullDestination ? 1 : 0, sourceBytes, invocation.result, invocation.error, invocation.exception);
    for (std::size_t i = 0; i < BufferCells; ++i) {
        const unsigned int value = operation == 0 ? static_cast<unsigned int>(wideDestination[i]) :
            static_cast<unsigned char>(byteDestination[i]);
        std::printf("%04x", value);
    }
    std::puts("");
}
} // namespace

int main(int argc, char** argv) {
    if (!std::setlocale(LC_CTYPE, argc == 2 ? argv[1] : "C")) {
        return 3;
    }
    static_assert(sizeof(wchar_t) == 2, "The reference must use native WinAPI UTF16 wchar_t");
    for (int index = 0; index < static_cast<int>(sizeof(Utf8Inputs) / sizeof(Utf8Inputs[0])); ++index) {
        for (int capacity = 0; capacity <= MaximumCapacity * static_cast<int>(sizeof(wchar_t)) + 1; ++capacity) {
            if (index < 31 || capacity <= 6 || capacity == 8 || capacity == 12) {
                Observe(0, index, capacity, false, false, 0);
            }
        }
    }
    for (int operation = 1; operation <= 2; ++operation) {
        for (int index = 0; index < static_cast<int>(sizeof(WideInputs) / sizeof(WideInputs[0])); ++index) {
            for (int capacity = 0; capacity <= MaximumCapacity; ++capacity) {
                Observe(operation, index, capacity, false, false, 0);
            }
        }
    }
    for (int operation = 0; operation <= 2; ++operation) {
        for (int capacity : {0, 1, 2, 4, 8}) {
            Observe(operation, 1, capacity, true, false, -1);
            Observe(operation, 1, capacity, false, true, -1);
        }
    }
    for (int sourceBytes : {-1, 0, 1, 2, 7, 64, INT_MAX}) {
        Observe(2, 4, 24, false, false, sourceBytes);
    }
    return 0;
}
