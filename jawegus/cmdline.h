#pragma once
#include <stdint.h>
#include "utils.h"

// ------------------------------
// command line options stuff

enum {
    CMD_FLAG_NONE,
    CMD_FLAG_BOOL,
    CMD_FLAG_INT,
    CMD_FLAG_HEX,
    CMD_FLAG_STRING,

    CMD_FLAG_MASK  = (1 << 16) - 1,
    CMD_FLAG_MULTI = (1 << 20),
};

struct cmdline_params_t {
    char        shortname;
    char        flags;
    char*       longname;
    void*       parmPtr;
    uint32_t    parmLength;     // string or binary data only
};

uint32_t parse_cmdline(char *cmdline, const cmdline_params_t *params, uint32_t paramCount);
