#include <array>
#include <climits>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../../src/common/legacy_text_encoding.h"

#if !defined(_WIN32)
using ucs2 = unsigned short;
class CLocalizedStringTable {
public:
    int ConvertANSIToUnicode(const char*, wchar_t*, int);
    int ConvertUnicodeToANSI(const wchar_t*, char*, int);
};
#include "actual_functions.inc"
#endif
#include "corpus.h"

namespace {
constexpr std::size_t BufferCells = 64;
constexpr std::size_t BeforeCells = 8;

struct Row {
    std::array<int, 9> fields;
    std::array<unsigned int, BufferCells> values;
};

Row Parse(const std::string& line) {
    Row row{};
    std::istringstream stream(line);
    std::string field;
    for (std::size_t index = 0; index < row.fields.size(); ++index) {
        std::getline(stream, field, ',');
        row.fields[index] = static_cast<int>(std::strtoul(field.c_str(), nullptr, 10));
    }
    std::getline(stream, field);
    if (!field.empty() && field.back() == '\r') {
        field.pop_back();
    }
    if (field.size() != BufferCells * 4) {
        throw std::runtime_error("Malformed exact reference row");
    }
    for (std::size_t index = 0; index < row.values.size(); ++index) {
        row.values[index] = static_cast<unsigned int>(std::strtoul(field.substr(index * 4, 4).c_str(), nullptr, 16));
    }
    return row;
}

template<typename Cell>
int Candidate(const Row& row, std::array<Cell, BufferCells>& wide, std::array<char, BufferCells>& bytes) {
    const int operation = row.fields[0];
    const int index = row.fields[1];
    const int referenceCapacity = row.fields[2];
    const char* input = row.fields[3] ? nullptr : Utf8Inputs[index];
    std::array<Cell, 8> source{{}};
    if (operation != 0) {
        for (std::size_t unit = 0; unit < source.size(); ++unit) {
            source[unit] = static_cast<Cell>(WideInputs[index][unit]);
        }
    }
    const Cell* wideInput = row.fields[3] ? nullptr : source.data();
    Cell* wideOutput = row.fields[4] ? nullptr : wide.data() + BeforeCells;
    char* byteOutput = row.fields[4] ? nullptr : bytes.data() + BeforeCells;
    const int wideBytes = (referenceCapacity / 2) * static_cast<int>(sizeof(Cell)) + referenceCapacity % 2;
#if defined(_WIN32)
    if (operation == 0) {
        return cse::legacy_text::Utf8ToWideCells(input, wideOutput, wideBytes);
    }
    return cse::legacy_text::WideCellsToUtf8(wideInput, byteOutput, referenceCapacity, operation == 2);
#else
    CLocalizedStringTable table;
    if (operation == 0) {
        return table.ConvertANSIToUnicode(input, reinterpret_cast<wchar_t*>(wideOutput), wideBytes);
    }
    if (operation == 1) {
        return table.ConvertUnicodeToANSI(reinterpret_cast<const wchar_t*>(wideInput), byteOutput, referenceCapacity);
    }
    return Q_UCS2ToUTF8(row.fields[3] ? nullptr : WideInputs[index], row.fields[5], byteOutput, referenceCapacity);
#endif
}

template<typename Cell>
int Run(const char* oracle, bool baseline) {
    std::ifstream file(oracle);
    if (!file) {
        return 2;
    }
    std::string line;
    int checked = 0;
    int failures = 0;
    int safeFailures = 0;
    while (std::getline(file, line)) {
        const Row row = Parse(line);
        const int capacity = row.fields[2];
        const bool unsafeReference = row.fields[4] != 0 || capacity <= 0 ||
            (row.fields[0] == 0 && capacity < 2);
        if (baseline && (unsafeReference || (row.fields[0] == 2 && row.fields[5] != 0))) {
            continue;
        }
        std::array<Cell, BufferCells> wide;
        wide.fill(static_cast<Cell>(0x5a5a));
        std::array<char, BufferCells> bytes;
        bytes.fill(0x5a);
        const int result = Candidate(row, wide, bytes);
        bool match = true;
        for (std::size_t index = 0; index < row.values.size(); ++index) {
            const unsigned int expected = unsafeReference ? (row.fields[0] == 0 ? 0x5a5au : 0x5au) : row.values[index];
            const unsigned int actual = row.fields[0] == 0 ? static_cast<unsigned int>(wide[index]) :
                static_cast<unsigned char>(bytes[index]);
            match = match && actual == expected;
        }
        match = match && result == (unsafeReference ? 0 : row.fields[6]);
        if (!match) {
            if (failures < 12) {
                std::printf("mismatch op=%d input=%d capacity=%d nullsrc=%d nulldst=%d srcbytes=%d actual=%d expected=%d\n",
                    row.fields[0], row.fields[1], capacity, row.fields[3], row.fields[4], row.fields[5], result,
                    unsafeReference ? 0 : row.fields[6]);
            }
            ++failures;
        }
        ++checked;
        safeFailures += unsafeReference ? 1 : 0;
    }
    std::printf("cell_bytes=%zu checked=%d failures=%d safe_undefined_domain_cases=%d\n",
        sizeof(Cell), checked, failures, safeFailures);
    return failures == 0 ? 0 : 1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        return 2;
    }
    if (!std::setlocale(LC_CTYPE, argv[2])) {
        return 3;
    }
    const bool baseline = argc == 4;
#if defined(_WIN32)
    return Run<std::uint16_t>(argv[1], baseline) | Run<std::uint32_t>(argv[1], baseline);
#else
    return Run<wchar_t>(argv[1], baseline);
#endif
}
