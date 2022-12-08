// GUS DMA emulation stuff
#include <stdint.h>
#include "emu8000.h"
#include "emu8kreg.h"
#include "gusemu.h"
#include "dmaemu.h"
#include "dma.h"
#include "utils.h"
#include "console.h"

/*
    DMA emulation works as simple as it could be :D
    - check if "start DMA" in DMA Control (GF1 reg 0x41) is triggered
    - validate all fields
    - read start address and count from DMA controller
    - if ((count == 0) || (count = 0xFFFF)), ignore transfer
    - copy data to EMU8K onboard DRAM, yielding periodically for processing IRQs
    - schedule terminal count IRQ event if requested

    fortunately, GUS doesn't have its own DMA Count register, using i8237 TC instead
*/

uint32_t gusemu_dma_upload_block(uint32_t dst, uint8_t* src, uint32_t count, uint32_t xormask) {
    if (count == 0) return 0;
    
    uint32_t count_now  = count;
    uint8_t *src_now    = src;
    uint32_t dst_now    = dst;

    // flush stale data and set starting address
    emu8k_waitForWriteFlush(emu8k_state.iobase);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, gus_state.mem8start + dst_now);
    while (count_now != 0) {
        uint32_t currentCount = (count_now >= 4) ? 4 : count_now;

        // read by 4 bytes, this will slow things down on misaligned addresses but I don't care
        uint32_t sample = (*(uint32_t*)src_now) ^ xormask;
        RASTER((sample & 0x3F), ((sample>>8) & 0x3F), ((sample>>16) & 0x3F));

        // NOTE - flush write buffer for every byte transferred, because DRAM upload time depends
        // on channel count allocated for DRAM, and apparently EMU8000 can't hold IOCHRDY too
        // long while write buffer full, due to ISA refresh cycles
        uint32_t sampleCount = currentCount;
        while (sampleCount-- > 0) {
            emu8k_waitForWriteFlush(emu8k_state.iobase);
            emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, gus_state.mem8start + dst_now);
            emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALD, (sample & 0xFF) << 8);
            sample >>= 8;
            dst_now++;
        }

        src_now   += currentCount;
        count_now -= currentCount;
    }
    RASTER(0,0,0);

    // check if 16bit samples enabled
    if ((gus_state.emuflags & GUSEMU_16BIT_SAMPLES) == 0) return 0;

    // do the same but for 16bit samples now :)
    count_now  = count;
    src_now    = src;
    dst_now    = dst >> 1;

    // flush stale data and set starting address
    emu8k_waitForWriteFlush(emu8k_state.iobase);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, gus_state.mem16start + dst_now);
    while (count_now != 0) {
        uint32_t currentCount = (count_now >= 4) ? 4 : count_now;

        // read by 4 bytes, this will slow things down on misaligned addresses but I don't care
        uint32_t sample = (*(uint32_t*)src_now) ^ xormask;

        // NOTE - flush write buffer for every byte transferred, because DRAM upload time depends
        // on channel count allocated for DRAM, and apparently EMU8000 can't hold IOCHRDY too
        // long while write buffer full, due to ISA refresh cycles
        uint32_t sampleCount = currentCount >> 1;
        while (sampleCount--) {
            emu8k_waitForWriteFlush(emu8k_state.iobase);
            emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, gus_state.mem8start + dst_now);
            emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALD, sample & 0xFFFF);
            sample >>= 16;
            dst_now++;
        }

        src_now   += currentCount;
        count_now -= currentCount;
    }

    return 0;
}

const uint32_t DMA_BLOCK_SIZE = 256;    // transfer 256 bytes per block, then yield and send next block

// upload contigous data blocks via DMA
// dst - GUS destination address, src - memory source, count - data count in bytes
uint32_t gusemu_dma_upload(uint32_t dst, uint8_t* src, uint32_t count, uint32_t dmactrl, uint32_t xormask) {
    if (count == 0) return 1;

    while (count != 0) {
        uint32_t currentCount = (count >= DMA_BLOCK_SIZE) ? DMA_BLOCK_SIZE : count;
        
        // upload
        gusemu_dma_upload_block(dst, src, currentCount, xormask);

        // do yield
        // TODO

        // switch to next block
        src   += currentCount;
        dst   += currentCount;
        count -= currentCount;
    }

    return 0;
}

uint32_t gusemu_dma_start() {
    uint32_t dmactrl = gus_state.gf1regs.dmactrl.h;

    // validate flags (dma started, playback, 8bit DMA)
    if ((dmactrl & 0x07) != 0x01) {
        return GUSEMU_DMA_DMACTRL_ERROR;
    }

    // generate XOR masks for bit inversion
    uint32_t xormask = 0;
    switch (dmactrl & 0xC0) {
        case 0x80: xormask = 0x80808080; break;
        case 0xC0: xormask = 0x80008000; break;
        default:   xormask = 0; break;
    }

    // GUS DRAM destination address, always aligned at 16 byte boundary
    uint32_t dmaaddr = gus_state.gf1regs.dmaaddr.w << 4; 

    // read 8237 stuff
    uint32_t i8237_addr  = dmaGetCurrentAddress(gus_state.dma);
    uint32_t i8237_count = dmaGetCurrentCount(gus_state.dma);
    uint32_t dmapage     = dmaGetPage(gus_state.dma);
    uint8_t *linsrc      = (uint8_t*)((dmapage << 16) + i8237_addr);

    // count == 0 or -1 - skip DMA, nothing to transfer
    if (i8237_count == 0xFFFF || i8237_count == 0) return GUSEMU_DMA_8237_ERROR; else i8237_count++;

    // run transfers
    if ((((i8237_addr + i8237_count) ^ i8237_addr) & ~0xFFFF) != 0) {
        // crosses page boundary, break into 2 transfers
        uint32_t count1 = 0x10000 - i8237_addr;
        uint32_t count2 = i8237_count - count1;
        gusemu_dma_upload(dmaaddr,          linsrc,                    count1, dmactrl, xormask);
        gusemu_dma_upload(dmaaddr + count1, (uint8_t*)(dmapage << 16), count2, dmactrl, xormask);
        i8237_addr = count2 & 0xFFFF;
    } else {
        // inside single page, copy as is
        gusemu_dma_upload(dmaaddr, linsrc, i8237_count, dmactrl, xormask);
        i8237_addr = (i8237_addr + i8237_count) & 0xFFFF;
    }

    // write out i8237 register values to fake DMA transfer complete
    dmaSet8237Address(gus_state.dma, i8237_addr);
    dmaSet8237Count(gus_state.dma, 0xFFFF);

    // reset DMA Enable bit
    gus_state.gf1regs.dmactrl.h &= ~(1 << 0);

#if 0  // this should be done by IRQ emulation handler, except for the instant timer IRQ case
    // set DMA terminal count shadow bit
    gus_state.gf1regs.dmactrl.l |=  (1 << 6);
#endif

    // TODO: schedule IRQ if requested
    return GUSEMU_DMA_OK;
}
