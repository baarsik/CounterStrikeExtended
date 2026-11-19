#pragma once

// Commands are generated at the client frame rate. Server snapshot storage
// and the maximum number of commands serialized per packet are independent.
constexpr int CL_COMMAND_HISTORY_SIZE = 512;
constexpr int CL_COMMAND_HISTORY_MASK = CL_COMMAND_HISTORY_SIZE - 1;
static_assert((CL_COMMAND_HISTORY_SIZE & CL_COMMAND_HISTORY_MASK) == 0);

constexpr int CL_CommandHistoryIndex(int sequence) {
    return sequence & CL_COMMAND_HISTORY_MASK;
}

constexpr bool CL_CanReplayCommands(int outgoing, int acknowledged) {
    // Reserve space for the acknowledged state and the current command.
    constexpr unsigned int maximumBacklog = CL_COMMAND_HISTORY_SIZE - 2;
    return static_cast<unsigned int>(outgoing) - static_cast<unsigned int>(acknowledged) < maximumBacklog;
}
