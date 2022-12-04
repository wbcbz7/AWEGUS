#include "console.h"
#include "cmdline.h"
#include "utils.h"

uint32_t parse_cmdline(char *cmdline, const cmdline_params_t *params, uint32_t paramCount) {
    char parm[64];
    // tokenize
    char *arg = cmdline, *argnext = 0;
    int argc = 1;

    // trim \r, \n and spaces
    tiny_rtrim(cmdline);
    tiny_ltrim(cmdline);

#if 0
    char *s = cmdline;
    while (*s != '\0') printf("%02X ", *s++);
    printf("\r\ncommand line: %s (%d chars)\r\n", cmdline, tiny_strlen(cmdline));
#endif

    while (((arg = tiny_strtok(arg, " ", &argnext)) != 0)) {
        // copy to temp buffer and uppercase it
        tiny_strncpy(parm, arg, sizeof(parm)); tiny_strupr(parm);
        char *p = parm;

        // strip separators
        while ((*p == '-') || (*p == '/')) p++;

        // get best matching option
        const cmdline_params_t* cmd = params; char* nextp = p;
        int curParm = 0;
        for (curParm = 0; curParm < paramCount; curParm++, cmd++) {
            if ((cmd->longname != NULL) && (tiny_strstr(p, cmd->longname) != NULL)) {
                p += tiny_strlen(cmd->longname);
                break;
            }
            if (*p == cmd->shortname) {
                p++; break;
            }
        }
        if (curParm >= paramCount) {
            printf("error: unknown parameter: %s\r\n", parm);
            return 1;
        }

        if (cmd->flags == CMD_FLAG_NONE) {
            *(uint32_t*)cmd->parmPtr = 1;
            continue;       // get next param
        }

        // skip more separators
        while ((*p == '=') || (*p == ':')) p++;

        // finally extract value
        switch(cmd->flags & CMD_FLAG_MASK) {
            case CMD_FLAG_BOOL:
                if (tiny_strstr(p, "OFF") || (*p == '0')) *(uint32_t*)cmd->parmPtr = 0;
                else /* if (tiny_strstr(p, "ON") || (*p == '1')) */ *(uint32_t*)cmd->parmPtr = 1;
                break;
            case CMD_FLAG_INT:
                if (tiny_strtol_store(p, 0, (long*)cmd->parmPtr, 10) == false) {
                    printf("error: incorrect parameter: %s\r\n", p);
                    return 1;
                }
                break;
            case CMD_FLAG_HEX:
                if (tiny_strtol_store(p, 0, (long*)cmd->parmPtr, 16) == false) {
                    printf("error: incorrect parameter: %s\r\n", p);
                    return 1;
                }
                break;
            case CMD_FLAG_STRING:
                tiny_strncpy((char*)cmd->parmPtr, p, cmd->parmLength);
                break;
            default:
                printf("error: undefined parameter type %d for %c(\"%s\")!\r\n", cmd->flags, cmd->shortname, cmd->longname);
                return 1;
        }

        // switch to next command
        arg = argnext;
        argc++;
    }

    return 0;
}
