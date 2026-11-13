#include "../../../src/public/cse_module_abi.h"
#include "../../../src/public/cse_client_abi.h"
#include <cstdio>
#include <utility>

#define REQUIRE(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "Catalog failure at %d: %s\n", __LINE__, #condition); return 1; \
} } while (false)

int main() {
    using namespace cse::abi;
    constexpr Descriptor required = Describe({{1, 0, 100}, {2, 0, 200}});
    Descriptor offered = Describe({{2, 0, 200}, {3, 0, 300}, {1, 0, 100}});
    REQUIRE(CheckRequired(required, &offered) == Compatibility::Compatible);
    REQUIRE(Check(required, &offered) == Compatibility::TableCount);
    REQUIRE(CheckRequired(required, nullptr) == Compatibility::MissingDescriptor);
    offered.tables[1].identifier = 2;
    REQUIRE(CheckRequired(required, &offered) == Compatibility::InvalidDescriptor);
    offered = Describe({{1, 0, 100}, {2, 0, 200}, {0, 0, 0}});
    REQUIRE(CheckRequired(required, &offered) == Compatibility::InvalidDescriptor);
    offered = Describe({{1, 0, 100}, {2, 0, 200}, {3, 1, 0}});
    REQUIRE(CheckRequired(required, &offered) == Compatibility::InvalidDescriptor);
    offered = Describe({{1, 0, 100}});
    REQUIRE(CheckRequired(required, &offered) == Compatibility::RequiredTableMissing);
    offered = required;
    offered.tableCount = static_cast<std::uint32_t>(MaxTables + 1);
    REQUIRE(CheckRequired(required, &offered) == Compatibility::TableCount);
    offered = required;
    offered.tables[1].signature ^= 1;
    REQUIRE(CheckRequired(required, &offered) == Compatibility::TableLayout);
    using Original = int (*)(int);
    using Changed = int (*)(float);
    REQUIRE(sizeof(Original) == sizeof(Changed));
    REQUIRE(ClientFunctionTypeHash<Original>() != ClientFunctionTypeHash<Changed>());
    return 0;
}
