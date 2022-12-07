#include <i86.h>
#include "irqpass.h"
#include "console.h"

void gusemu_irq_passup_install(void *callback, pm_gate_descriptor *idt, uint32_t irq, uint32_t intr) {
    // init static fields
    gusemu_irq_passup_handler = (void (*)())((uint8_t*)callback - (uint8_t*)&gusemu_irq_passup_handler - 4);
    gusemu_irq_passup_intr_num = intr;
    gusemu_irq_passup_irq_mask = (1 << (irq & 7));
    gusemu_irq_passup_pic_base = gusemu_irq_passup_pic_base2 = (irq & 8 ? 0xA0 : 0x20);

    // set handlers
    gusemu_irq_passup_oldhandler = (void (*__far)())idt_get(idt, intr);
    idt_set(idt, intr, (void __far *)&gusemu_irq_passup, PM_SPECIAL_32BIT_INTERRUPT_GATE);
}

void gusemu_irq_passup_remove(pm_gate_descriptor *idt, uint32_t intr) {
    idt_set(idt, intr, (void __far *)gusemu_irq_passup_oldhandler, PM_SPECIAL_32BIT_INTERRUPT_GATE);
}