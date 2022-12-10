#include "tim_uart.h"
#include "inp.h"
#include "irqpass.h"
#include "lowlevel.h"

static void parse_uart_irq(uint32_t combase, uint8_t reason) {
    switch (reason & 0x06) {
        case 0: inp (combase + 5); break;
        case 2: inp (combase); break;
        case 4: outp(combase, 0); break;
        case 6: inp (combase + 6); break;
    }
}

static void clear_uart_irq(uint32_t combase) {
    while (true) {
        uint8_t irqreason = inp(combase + 2);
        if ((irqreason & 1) == 0) parse_uart_irq(combase, irqreason); else break;
    }
}

// -----------------------

// static stuff
static irq_timer_uart_t *uarttimer = &irq_timer_uart;

// ---------------
// interrupt handler

static void timer_uart_handler(Client_Reg_Struc *pcl) {
    irq_timer_uart_t *self = uarttimer;

    // acknowledge UART, send "next" character to retrigger interrupt
    uint8_t irqreason = inp(self->devinfo.iobase + 2);
    if ((irqreason & 7) == 2) outp(self->devinfo.iobase + 0, 0x55);

    // call callback
    if (self->callback(pcl, self->userPtr) == 0) {
        // do specific EOI
        if (self->devinfo.irq >= 8) {
            outp(0x20, 0x60 | (self->devinfo.irq & 7));
            outp(0x20, 0x62);       // ack IRQ2
        } else {
            outp(0x20, 0x60 | (self->devinfo.irq & 7));
        }
    }
}
#pragma aux timer_uart_handler parm [ebx]

// -----------------------

static uint32_t timer_uart_set_divisor(void *s, uint32_t divisor) {
    irq_timer_uart_t *self = (irq_timer_uart_t*)s;

    self->divisor = divisor == 0 ? 1 : divisor;
    outp (self->devinfo.iobase + 3, 0x83);                   // 8N1, load divisor
    outp (self->devinfo.iobase + 0, self->divisor & 0xFF);   // load divisor
    outp (self->devinfo.iobase + 1, self->divisor >> 8);     // load divisor
    outp (self->devinfo.iobase + 3, 0x03);                   // 8N1

    return 0;
}

static uint32_t timer_uart_init(void *s, uint32_t flags, irq_timer_resources_t* resources) {
    irq_timer_uart_t *self = (irq_timer_uart_t*)s;

    // save resoures
    self->intr = 0; // used to determine if IRQ emulation has been installed
    self->devinfo = *resources;
    if (self->devinfo.irq > 7) return 1;        // high IRQs are not supported yet

    // set time constant
    self->devinfo.rate = 11520;

    // setup UART
    clear_uart_irq(self->devinfo.iobase);   // clear pending interrupts
    outp (self->devinfo.iobase + 2, 0x00);  // FIFO disable
    outp (self->devinfo.iobase + 4, 0x08);  // out 2 (irq enable) high

    // set dummy divisor (100)
    self->divisor = 100;
    timer_uart_set_divisor(s, self->divisor);

    // get IDT
    sidt(&self->idtDesc); self->idt = (pm_gate_descriptor*)self->idtDesc.base;

    // save IRQ mask, unmask UART interrupt
    self->intr = self->devinfo.irq + 8;
    self->irqmask = inp(0x21);
    outp(0x21, self->irqmask & ~(1 << self->devinfo.irq));

    // setup IRQ handler
    gusemu_irq_passup_install(timer_uart_handler, self->idt, self->devinfo.irq, self->intr);

    // ok
    return 0;
}

static uint32_t timer_uart_get_rate(void *s) {
    return ((irq_timer_uart_t*)s)->devinfo.rate;
}

static uint32_t timer_uart_set_callback(void *s, uint32_t (*callback)(Client_Reg_Struc*, void*), void *userPtr) {
    irq_timer_uart_t *self = (irq_timer_uart_t*)s;
    if (callback == 0) return 1;

    // save callback and user pointer
    self->callback = callback;
    self->userPtr = userPtr;

    return 0;
}

static uint32_t timer_uart_done(void *s) {
    irq_timer_uart_t *self = (irq_timer_uart_t*)s;
    
    // stop the timer
    self->stop(self);

    if (self->intr != 0) {
        // restore interrupt
        gusemu_irq_passup_remove(self->idt, self->intr);

        // mask interrupt back
        outp(0x21, (self->irqmask & (1 << self->devinfo.irq)) | (inp(0x21) & ~((1 << self->devinfo.irq))));
    }

    return 0;
}

static uint32_t timer_uart_start(void *s) {
    irq_timer_uart_t *self = (irq_timer_uart_t*)s;

    // set static object
    uarttimer = self;

    // start transfer
    outp(self->devinfo.iobase + 1, 0x02);  // enable transmit interrupts, disable others

    // kickstart interrupt
    outp(self->devinfo.iobase + 0, 0x55);

    return 0;
}

static uint32_t timer_uart_stop(void *s) {
    irq_timer_uart_t *self = (irq_timer_uart_t*)s;

    // stop transfer
    outp (self->devinfo.iobase + 1, 0x00);  // disable interrupts
    clear_uart_irq(self->devinfo.iobase);   // clear pending interrupts

    return 0;
}

// ---------- static object!
irq_timer_uart_t irq_timer_uart = {
    &timer_uart_init,
    &timer_uart_done,
    &timer_uart_set_divisor,
    &timer_uart_set_callback,
    &timer_uart_start,
    &timer_uart_stop,
};
