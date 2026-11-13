#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include "../../../src/common/command_types.h"
#include "../strings/high_offset_buffer.h"

cmd_function_t *firstCommand = nullptr;
cmd_function_t *Cmd_GetFirstCmd() {
    return firstCommand;
}
#include "../../../src/engine/command_handles_impl.h"

static_assert(sizeof(decltype(GetFirstCmdFunctionHandle())) == sizeof(std::uintptr_t),
              "Command handles must retain native pointers");

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Command handle check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (false)

int main() {
    HighOffsetBuffer mapping(nullptr);
    CHECK(mapping.get());
    auto *commands = reinterpret_cast<cmd_function_t *>(mapping.get());
    char firstName[] = "owned-first";
    char secondName[] = "owned-second";
    new (commands) cmd_function_t{commands + 1, firstName, nullptr, 0};
    new (commands + 1) cmd_function_t{nullptr, secondName, nullptr, 0};
    firstCommand = commands;
    if (sizeof(void *) > sizeof(std::uint32_t)) {
        CHECK(reinterpret_cast<std::uintptr_t>(commands) > UINT32_MAX);
    }
    const auto first = GetFirstCmdFunctionHandle();
    CHECK(first == reinterpret_cast<std::uintptr_t>(commands));
    CHECK(GetCmdFunctionName(first) == firstName);
    firstName[0] = 'O';
    CHECK(std::strcmp(GetCmdFunctionName(first), "Owned-first") == 0);
    const auto second = GetNextCmdFunctionHandle(first);
    CHECK(second == reinterpret_cast<std::uintptr_t>(commands + 1));
    CHECK(GetCmdFunctionName(second) == secondName);
    CHECK(GetNextCmdFunctionHandle(second) == 0);
    firstCommand = commands + 1;
    CHECK(GetFirstCmdFunctionHandle() == second);
    firstCommand = nullptr;
    CHECK(GetFirstCmdFunctionHandle() == 0);
    commands[1].~cmd_function_t();
    commands[0].~cmd_function_t();
    std::puts("Command iteration, name identity and native high handles passed");
    return 0;
}
