#pragma once

#include "jlm.h"
#include "nprintf.h"

void putchar(int ch);
const char* puts(const char *str);
void putchar_printf(int ch, void *ctx);

#define printf(...) npf_pprintf(putchar_printf, 0, __VA_ARGS__)

