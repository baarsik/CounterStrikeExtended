#pragma once
#include "../../../src/public/cse_module_abi.h"

struct FixtureTable {
    std::uintptr_t (CSE_ABI_CALL *command)(std::uintptr_t);
    const char *(CSE_ABI_CALL *stringAt)(std::uintptr_t, std::ptrdiff_t);
};

constexpr cse::abi::TableLayout FixtureLayout{
    0xf17e0001, // Fixture-only table identifier; no production table is described.
    0,
    cse::abi::LayoutHash({sizeof(FixtureTable), alignof(FixtureTable),
                          offsetof(FixtureTable, command), offsetof(FixtureTable, stringAt)})
};
constexpr auto FixtureAbi = cse::abi::Describe({FixtureLayout});
