#include "tim_sb.h"
#include "inp.h"
#include "irqpass.h"
#include "lowlevel.h"
#include "utils.h"
#include "dma.h"

static void sbDspWrite(uint32_t base, uint8_t data) {
    int timeout = (1ULL << 10); while (((inp(base + 0xC) & 0x80) == 0x80) && (--timeout != 0));
    outp(base + 0xC, data);
}

static uint32_t sbDspRead(uint32_t base) {
    int timeout = (1ULL << 10); while (((inp(base + 0xE) & 0x80) == 0) && (--timeout != 0));
    uint8_t data = inp(base + 0xA);
    return data;
}

static inline void sbAck8Bit(uint32_t base) {
    inp(base + 0xE);
}

static bool sbDspReset(uint32_t base) {
    uint32_t timeout;
    
    // trigger reset
    outp(base + 0x6, 1); 
    
    // wait a bit (i.e. read from unused port)
    timeout = 400; while (--timeout) inp(0x80); // should not screw everything up
    
    // remove reset and wait for 0xAA in read buffer 
    outp(base + 0x6, 0); 
    
    timeout = 20; bool detected = false;
    while((--timeout) && (!detected)) {
        if (sbDspRead(base) == 0xAA) detected = true;
    }

    return detected;
}

static uint32_t sbTimeConstant(int32_t rate) {
    if (rate < 4000) return 0; else return 256 - (1000000 / rate);
}

// -----------------------

// static stuff
static irq_timer_sb_t *sbtimer = &irq_timer_sb;

// ---------------
// interrupt handler

// TODO: fix prototype
static void timer_sb_handler(Client_Reg_Struc *pcl) {
    irq_timer_sb_t *self = sbtimer;

    // ack SB interrupt
    sbAck8Bit(self->devinfo.iobase);

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
#pragma aux timer_sb_handler parm [ebx]

// ---------------

static uint32_t timer_sb_set_divisor(void *s, uint32_t divisor) {
    irq_timer_sb_t *self = (irq_timer_sb_t*)s;

    self->divisor = divisor - 1;
    sbDspWrite(self->devinfo.iobase, 0x48);
    sbDspWrite(self->devinfo.iobase, self->divisor & 0xFF);
    sbDspWrite(self->devinfo.iobase, self->divisor >> 8);

    return 0;
}

static uint32_t timer_sb_init(void *s, uint32_t flags, irq_timer_resources_t* resources) {
    irq_timer_sb_t *self = (irq_timer_sb_t*)s;

    // save resoures
    self->intr = 0; // used to determine if IRQ emulation has been installed
    self->devinfo = *resources;
    if (self->devinfo.irq > 7) return 1;        // high IRQs are not supported yet
    
    // check if device is available
    if (sbDspReset(self->devinfo.iobase) == false) return 1;

    // allocate memory for DMA buffer, prepare SB time constant
    self->dosmemhandle = getdosmem(1);
    self->blockptr = (uint8_t*)(((uint32_t)self->dosmemhandle) << 4);
    tiny_memset(self->blockptr, 0x80, 16);

    // set time constant
    if (self->devinfo.rate == 0) self->devinfo.rate = 5000;
    if ((self->timeConstant = sbTimeConstant(self->devinfo.rate)) == 0) return 1; // unknown rate!
    sbDspWrite(self->devinfo.iobase, 0x40);
    sbDspWrite(self->devinfo.iobase, self->timeConstant);

    // set dummy divisor (100)
    self->divisor = 100;
    timer_sb_set_divisor(s, self->divisor);

    // get IDT
    sidt(&self->idtDesc); self->idt = (pm_gate_descriptor*)self->idtDesc.base;

    // set IRQ mask
    self->intr = self->devinfo.irq + 8;
    self->irqmask = inp(0x21);
    outp(0x21, self->irqmask & ~(1 << self->devinfo.irq));

    // setup IRQ handler
    gusemu_irq_passup_install((void*)timer_sb_handler, self->idt, self->devinfo.irq, self->intr);

    // setup DMA controller, but don't unmask it yet
    dmaSetup(self->devinfo.dma, self->blockptr, 1, dmaModeRead | dmaModeInc | dmaModeAutoInit | dmaModeSingle);

    // ok
    return 0;
}

static uint32_t timer_sb_get_rate(void *s) {
    return ((irq_timer_sb_t*)s)->devinfo.rate;
}

static uint32_t timer_sb_set_callback(void *s, uint32_t (*callback)(Client_Reg_Struc*, void*), void *userPtr) {
    irq_timer_sb_t *self = (irq_timer_sb_t*)s;
    if (callback == 0) return 1;

    // save callback and user pointer
    self->callback = callback;
    self->userPtr = userPtr;

    return 0;
}

static uint32_t timer_sb_done(void *s) {
    irq_timer_sb_t *self = (irq_timer_sb_t*)s;

    // stop the timer
    self->stop(self);

    // reset SB
    sbDspReset(self->devinfo.iobase);

    if (self->intr != 0) {
        // restore interrupt
        gusemu_irq_passup_remove(self->idt, self->intr);

        // mask interrupt back
        outp(0x21, (self->irqmask & (1 << self->devinfo.irq)) | (inp(0x21) & ~((1 << self->devinfo.irq))));
    }

    // deallocate memory
    if (self->dosmemhandle != 0) freedosmem(self->dosmemhandle);

    return 0;
}

static uint32_t timer_sb_start(void *s) {
    irq_timer_sb_t *self = (irq_timer_sb_t*)s;

    // set static object
    sbtimer = self;

    // start transfer
    sbDspWrite(self->devinfo.iobase, 0x1C);

    // unmask DMA
    dmaStart(self->devinfo.dma);

    return 0;
}

static uint32_t timer_sb_stop(void *s) {
    irq_timer_sb_t *self = (irq_timer_sb_t*)s;

    // stop transfer
    sbDspWrite(self->devinfo.iobase, 0xD0);

    // mask DMA
    dmaStop(self->devinfo.dma);

    return 0;
}

// ---------- static object!
irq_timer_sb_t irq_timer_sb = {
    &timer_sb_init,
    &timer_sb_done,
    &timer_sb_set_divisor,
    &timer_sb_set_callback,
    &timer_sb_start,
    &timer_sb_stop,
};

