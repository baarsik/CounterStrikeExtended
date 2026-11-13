#pragma once

#include "../public/cse_command.h"

CseCommandHandle GetFirstCmdFunctionHandle() {
    return reinterpret_cast<CseCommandHandle>(Cmd_GetFirstCmd());
}

CseCommandHandle GetNextCmdFunctionHandle(CseCommandHandle handle) {
    return reinterpret_cast<CseCommandHandle>(reinterpret_cast<cmd_function_t *>(handle)->next);
}

const char *GetCmdFunctionName(CseCommandHandle handle) {
    return reinterpret_cast<cmd_function_t *>(handle)->name;
}

