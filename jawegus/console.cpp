#include "jlm.h"
#include "console.h"

// SLOW!
void putchar_printf(int ch, void *ctx) {
    putchar(ch);
}

void putchar(int ch) {
    Client_Reg_Struc *pcl = jlm_BeginNestedExec();
    pcl->Client_EAX = 0x0200;
    pcl->Client_EDX = ch;
    Exec_Int(pcl, 0x21);
    End_Nest_Exec(pcl);
}

// prints asciiz string
const char* puts(const char *str) {
    Client_Reg_Struc *pcl = jlm_BeginNestedExec();
    for (; *str; str++) {
        pcl->Client_EAX = 0x0200;
        pcl->Client_EDX = *str;
        Exec_Int(pcl, 0x21);
    }
    End_Nest_Exec(pcl);
    return str;
}

char toupper(char c) {
    return ((c >= 'a') && (c <= 'z')) ? c - ('a' - 'A') : c;
}

#define NANOPRINTF_IMPLEMENTATION
#include "nprintf.h"
