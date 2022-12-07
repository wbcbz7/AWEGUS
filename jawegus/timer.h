#pragma once

#include <stdint.h>
#include "jlm.h"
#include "lowlevel.h"

struct irq_timer_resources_t {
    uint32_t iobase;
    uint32_t irq;
    uint32_t dma;
    uint32_t rate;      // 0 if default
};

enum {
    IRQTIMER_CREATE_SB = 0,
    IRQTIMER_CREATE_UART,
};

enum {
    IRQTIMER_INIT_PASS_CLIENT_REGSTRUCT = (1 << 0),
};

// timer callback declatration

struct irq_timer_t {
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

};
