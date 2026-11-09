#include "public/cse_module_abi.h"

using cse::abi::Compatibility;
using cse::abi::Describe;
using cse::abi::LayoutHash;

constexpr auto Required = Describe({{0x7f01, 0, LayoutHash({sizeof(int), alignof(int), 1})}});
constexpr auto Additional = Describe({
    {0x7f01, 0, LayoutHash({sizeof(int), alignof(int), 1})},
    {0x7f02, 0, LayoutHash({sizeof(void*), alignof(void*), 2})}
});
constexpr auto Missing = Describe({{0x7f02, 0, LayoutHash({sizeof(void*), alignof(void*), 2})}});
constexpr auto Duplicate = Describe({
    {0x7f01, 0, LayoutHash({sizeof(int), alignof(int), 1})},
    {0x7f01, 0, LayoutHash({sizeof(int), alignof(int), 1})}
});
constexpr auto TooMany = Describe({
    {1, 0, 1}, {2, 0, 1}, {3, 0, 1}, {4, 0, 1}, {5, 0, 1}, {6, 0, 1},
    {7, 0, 1}, {8, 0, 1}, {9, 0, 1}, {10, 0, 1}, {11, 0, 1}, {12, 0, 1},
    {13, 0, 1}, {14, 0, 1}, {15, 0, 1}, {16, 0, 1}, {17, 0, 1}
});

static_assert(cse::abi::RuntimeVersion == 9, "Inventory is pinned to runtime9");
static_assert(cse::abi::MaxTables == 16, "Group proposal must fit the existing bound");
static_assert(sizeof(cse::abi::Descriptor) == 288, "No descriptor envelope change is proposed");
static_assert(cse::abi::CheckRequired(Required, &Additional) == Compatibility::Compatible,
    "Compatible family addition is already supported");
static_assert(cse::abi::Check(Required, &Additional) == Compatibility::TableCount,
    "Exact old-style checks require a deliberate composition update before appending families");
static_assert(cse::abi::CheckRequired(Required, &Missing) == Compatibility::RequiredTableMissing,
    "Missing owned family must reject before factory use");
static_assert(cse::abi::CheckRequired(Required, &Duplicate) == Compatibility::InvalidDescriptor,
    "Duplicate families are rejected");
static_assert(cse::abi::CheckRequired(Required, &TooMany) == Compatibility::TableCount,
    "The seventeenth entry is rejected before table lookup");
