#pragma once

typedef void (*xcommand_t)();

typedef struct cmd_function_s {
    struct cmd_function_s *next;
    char *name;
    xcommand_t function;
    int flags;
} cmd_function_t;
