#include "jlm.h"
#include "console.h"
#include "utils.h"

// memcpy, with count == 0 guard
__declspec(naked) void tiny_memcpy(void *dst, const void *src, uint32_t count) {
    _asm {
            jcxz    _end
            mov     eax, ecx
            shr     ecx, 2
            rep     movsd
            mov     ecx, eax
            and     ecx, 3
            jz      _end
            rep     movsb
        _end:
            ret
    }
}

// memset
__declspec(naked) void tiny_memset(void *dst, uint32_t data, uint32_t count) {
    _asm {
            jcxz    _end
            mov     edx, ecx
            shr     ecx, 2
            rep     stosd
            mov     ecx, edx
            and     ecx, 3
            jz      _end
            rep     stosb
        _end:
            ret
    }
}

// memset, 32bit
__declspec(naked) void tiny_memsetd(void *dst, uint32_t data, uint32_t count) {
    _asm {
            shr     ecx, 2
            jcxz    _end
            rep     stosd
        _end:
            ret
    }
}

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

bool istrim(char ch) {
    return ((ch == '\xFF') || (ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n'));
}

char *tiny_rtrim(char *str) {
    char *p = str + tiny_strlen(str) - 1;
    while (istrim(*p)) *p-- = '\0';
    return str;
}

char *tiny_ltrim(char *str) {
    char *p = str;
    while (istrim(*p) && (*p != '\0')) p++;
    return p;
}

char tiny_toupper(char c) {
    return ((c >= 'a') && (c <= 'z')) ? c - ('a' - 'A') : c;
}

char* tiny_strupr(char *str) {
    char *p = str; if (str == 0) return 0;
    while (*p != 0) {*p = tiny_toupper(*p); p++;}
    return str;
}

bool tiny_strtol_store(const char *start, char **end, long *result, int radix) {
    long rtn = 0; bool negate = false, isCorrect = true;
    // bail out if null pointer or string is empty
    if ((start == 0) || (result == 0) || (*start == '\0')) return 0;
    // skip spaces
    while (*start == ' ') start++;
    // set negate flag if minus found
    if (*start == '-')  { negate = true; start++; };
    // autodetect radix if possible
    if ((radix == 0) && (*(start+1) != 0)) {
        char ch = tiny_toupper(*(start + 1));
        if (ch == 'B') radix = 2;  else
        if (ch == 'X') radix = 16; else radix = 10;
        start += 2;
    }
    // conversion loop
    while ((*start != 0) && (*start != ' ') && (isCorrect)) {
        char ch = tiny_toupper(*start);
        switch (radix) {
            case 2:
                if ((ch >= '0') && (ch <= '1')) {
                    rtn = (rtn << 1) + (ch - '0');
                } else if (ch != 'B') isCorrect = false;
                break;
            case 8:
                if ((ch >= '0') && (ch <= '7')) {
                    rtn = (rtn << 3) + (ch - '0');
                } else isCorrect = false;
                break;
            case 10:
                if ((ch >= '0') && (ch <= '9')) {
                    rtn = (rtn * 10)  + (ch - '0');
                } else isCorrect = false;
                break;
            case 16:
                if ((ch >= '0') && (ch <= '9')) {
                    rtn = (rtn << 4)  + (ch - '0');
                } else if ((ch >= 'A') && (ch <= 'F')) {
                    rtn = (rtn << 4)  + (ch - 'A' + 10);
                } else if (ch != 'X') isCorrect = false;
                
                break;
            default:
                isCorrect = false;
                break;
        }
        start++;
    }
    // negate if requested
    if (negate) rtn = -rtn;
    
    // and return end pointer
    if (end != 0) *end = (char*)start;

    // done :)
    *result = rtn;
    return isCorrect;
}

long tiny_strtol(const char *start, char **end, int radix) {
    long rtn;
    return (tiny_strtol_store(start, end, &rtn, radix)) ? rtn : 0;
}

// strlen
uint32_t tiny_strlen(const char* str) {
    uint32_t len = 0;
    while (*str++ != '\0') len++;
    return len;
}

// strstr 
const char* tiny_strstr(const char *first, const char* second) {
    if ((first == 0) || (second == 0)) return 0;
    // find first match
    while ((*first != *second) && (*first != '\0')) { first++; };
    if (*first == '\0') return 0;

    // iteratively check for full match
    while (*first != '\0') {
        // test for full match
        const char *match = first, *needle = second;
        while ((*first == *needle) && (*first != '\0') && (*needle != '\0')) {first++; needle++;};
        if (*needle == '\0') return match; else first++;
    }
    return  0;
}

// strncpy
char* tiny_strncpy(char* dst, const char* str, uint32_t dst_size) {
    uint32_t src_len = tiny_strlen(str) + 1;
    uint32_t total_size = dst_size < src_len ? dst_size : src_len;
    tiny_memcpy(dst, str, total_size);
    if (dst_size < src_len) dst[total_size - 1] = '\0';
    return dst;
}

// modifies user string! also O(n^2)
char *tiny_strtok(char* start, const char* delim, char** end) {
    char *p = start; if ((p == 0) || (*p == '\0')) return 0;
    while (*p != '\0') {
        const char *d = delim;
        while (*d != '\0') if (*p == *d++) {
            // set '\0' and break from inner loop
            *p = '\0'; 
            // ooops, no delims
            if (end != 0) *end = ++p;
            return start;
        }
        p++;
    }
    // ooops, no delims
    if (end != 0) *end = 0;
    return start;
}

#define NANOPRINTF_IMPLEMENTATION
#include "nprintf.h"
