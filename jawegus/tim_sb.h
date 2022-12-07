#pragma once

#include "timer.h"

struct irq_timer_sb_t {
    // init timer
    uint32_t (*init)(void *self, uint32_t flags, irq_timer_resources_t* resources);

    // done timer
    uint32_t (*done)(void *self);

    // set divisor
    uint32_t (*setDivisor)(void *self, uint32_t div);

    // set callback
    uint32_t (*setCallback)(void *self, uint32_t (*callback)(Client_Reg_Struc*, void*), void *userPtr);

    // start timer
    uint32_t (*start)(void *self);

    // stop timer
    uint32_t (*stop)(void *self);

    // --------------
    // private data here
    
    // device resources
    irq_timer_resources_t devinfo;

    // callback and user pointer
    uint32_t (*callback)(Client_Reg_Struc*, void*);
    void *userPtr;

    // old timer ISR
    void __far *oldhandler;

    // current divisor (minus one)
    uint32_t divisor;

    // old IRQ mask
    uint8_t irqmask;

    // interrupt number
    uint8_t intr;

    // time constant
    uint8_t timeConstant;

    // DOS memory block (just 16 bytes! because we cant allocate less :)
    uint16_t dosmemhandle;
    uint8_t *blockptr;

    // IDT
    descriptorTableAddr idtDesc;
    pm_gate_descriptor *idt;
};

// static SB timer struct
extern irq_timer_sb_t irq_timer_sb;
