#pragma once
#include <stdint.h>
#include "lowlevel.h"
#include "jlm.h"

extern "C" {
    void __far  gusemu_irq_passup();
    
    extern uint8_t     gusemu_irq_passup_pic_base, gusemu_irq_passup_pic_base2;
    extern uint8_t     gusemu_irq_passup_irq_mask;
    extern uint32_t    gusemu_irq_passup_intr_num;
    extern void __far (*gusemu_irq_passup_oldhandler)();
    extern void       (*gusemu_irq_passup_handler)();
}

// install passup handler
void gusemu_irq_passup_install(void *callback, pm_gate_descriptor *idt, uint32_t irq, uint32_t intr);

// remove passup handler
void gusemu_irq_passup_remove(pm_gate_descriptor *idt, uint32_t intr);
