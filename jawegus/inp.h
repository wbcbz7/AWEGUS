#pragma once
#include <stdint.h>

void outp(uint16_t port, uint8_t value);
#pragma aux outp = "out dx, al" parm [dx] [al] modify exact [al]
void outpw(uint16_t port, uint16_t value);
#pragma aux outpw = "out dx, ax" parm [dx] [ax] modify exact [ax]
void outpd(uint16_t port, uint32_t value);
#pragma aux outpd = "out dx, eax" parm [dx] [eax] modify exact [eax]

uint8_t inp(uint16_t port);
#pragma aux inp = "in al, dx" parm [dx] value [al] modify exact [al]
uint16_t inpw(uint16_t port);
#pragma aux inpw = "in  ax, dx" parm [dx] value [ax] modify exact [ax]
uint32_t inpd(uint16_t port);
#pragma aux inpd = "in eax, dx" parm [dx] value [eax] modify exact [eax]
