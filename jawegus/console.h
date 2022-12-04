#pragma once

#include <stdint.h>
#include "jlm.h"
#include "nprintf.h"

void tiny_memcpy(void *dst, const void *src, uint32_t count);
#pragma aux tiny_memcpy parm [edi] [esi] [ecx] modify [eax ecx esi edi]

void tiny_memset(void *dst, uint32_t data, uint32_t count);
#pragma aux tiny_memset parm [edi] [eax] [ecx] modify [ecx edx esi edi]

void tiny_memsetd(void *dst, uint32_t data, uint32_t count);
#pragma aux tiny_memsetd parm [edi] [eax] [ecx] modify [ecx esi edi]

void putchar(int ch);
const char* puts(const char *str);
void putchar_printf(int ch, void *ctx);

char tiny_toupper(char c);
char* tiny_strupr(char *str);
long tiny_strtol(const char *start, char **end, int radix);
bool tiny_strtol_store(const char *start, char **end, long *result, int radix);
uint32_t tiny_strlen(const char* str);
char* tiny_strncpy(char* dst, const char* str, uint32_t dst_size);
char* tiny_strtok(char* start, const char* delim, char** end);
const char* tiny_strstr(const char* first, const char* second);
char *tiny_rtrim(char *str);
char *tiny_ltrim(char *str);

#define printf(...) npf_pprintf(putchar_printf, 0, __VA_ARGS__)
