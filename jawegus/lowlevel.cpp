#include <i86.h>
#include "lowlevel.h"

// get/set interrupt/exception handler using IDT
// NOTE!!!!! exception handler stack frame is defined per x86 documentation, NOT by DPMI docs!
void __far * idt_get(pm_gate_descriptor *idt, int intnum) {
    return MK_FP(idt[intnum].selector, (idt[intnum].ofs_high << 16) | (idt[intnum].ofs_low));
}

void idt_set(pm_gate_descriptor *idt, int intnum, void __far *handler, int type) {
    idt[intnum].special_type    = type;
    idt[intnum].selector        = FP_SEG(handler);
    idt[intnum].ofs_high        = FP_OFF(handler) >> 16;
    idt[intnum].ofs_low         = FP_OFF(handler) & 0xFFFF;
}
