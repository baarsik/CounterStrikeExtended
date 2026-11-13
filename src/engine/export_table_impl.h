#pragma once

#include "pe_export_file_impl.h"
#include "../common/legacy_symbol_names.h"

void BuildExportTable(extensiondll_t *dll, const char *filename) {
    if (!dll || !filename) {
        return;
    }
    dll->functionTable = nullptr;
    dll->functionCount = 0;
    char *names = nullptr;
    const auto bytes = static_cast<std::size_t>(GetExportNames(filename, &names));
    if (!names) {
        return;
    }
    std::size_t count = 0;
    std::size_t nameBytes = 0;
    for (std::size_t offset = 0; offset < bytes; offset += std::strlen(names + offset) + 1) {
        std::size_t length = 0;
        if (cse::symbols::ExportName(names + offset, length)) {
            ++count;
            nameBytes += length + 1;
        }
    }
    if (count == 0 || count > INT_MAX || count > (SIZE_MAX - nameBytes) / sizeof(functiontable_t)) {
        return;
    }
    auto *table = static_cast<functiontable_t *>(Mem_Malloc(count * sizeof(functiontable_t) + nameBytes));
    if (!table) {
        return;
    }
    auto *output = reinterpret_cast<char *>(table + count);
    std::size_t index = 0;
    for (std::size_t offset = 0; offset < bytes; offset += std::strlen(names + offset) + 1) {
        std::size_t length = 0;
        const auto *canonical = cse::symbols::ExportName(names + offset, length);
        if (canonical) {
            std::memcpy(output, canonical, length);
            output[length] = '\0';
            table[index].pFunctionName = output;
            table[index].pFunction = reinterpret_cast<CseSymbolAddress>(GetProcAddress(dll->lDLLHandle, names + offset));
            output += length + 1;
            ++index;
        }
    }
    dll->functionTable = table;
    dll->functionCount = static_cast<int>(count);
}
