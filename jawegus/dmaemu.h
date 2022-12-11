#pragma once
#include <stdint.h>

enum {
    GUSEMU_DMA_OK = 0,
    GUSEMU_DMA_DMACTRL_ERROR,
    GUSEMU_DMA_8237_ERROR,

    GUSEMU_DMA_IRQ = (1 << 31),
};

// run DMA emulation
uint32_t gusemu_dma_start();

// update DMA terminal count IRQ
void gusemu_update_dma_tc();
