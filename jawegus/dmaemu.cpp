// GUS DMA emulation stuff
#include <stdint.h>
#include "emu8000.h"
#include "emu8kreg.h"
#include "gusemu.h"
#include "dmaemu.h"
#include "dma.h"
#include "utils.h"
#include "console.h"
#include "irqemu.h"

// post DMA terminal count IRQ
void gusemu_post_dma_tc_irq() {
    // trigger that fake DMA transfer to set TC status bit in i8237
    dmaStart(gus_state.dma);
    dmaRequest(gus_state.dma);  // manually trigger it

    // reset DMA Enable bit
    gus_state.gf1regs.dmactrl.h &= ~(1 << 0);

    // set DMA terminal count shadow bit
    gus_state.gf1regs.dmactrl.l |= (1 << 6);
}

// update DMA terminal count IRQ
void gusemu_update_dma_tc() {
    if (gus_state.dma_tc_delay != 0) {
        gus_state.dma_tc_delay--;
        if (gus_state.dma_tc_delay == 0) {
            // post IRQ
            gusemu_post_dma_tc_irq();
            if (((gus_state.emuflags & GUSEMU_TIMER_MODE_MASK) == GUSEMU_TIMER_INSTANT) &&
                ((gus_state.timer.flags & (GUSEMU_TIMER_T1_RUNNING|GUSEMU_TIMER_T2_RUNNING)) == 0)) {
                // stop timer
                gus_state.timer.dev->stop(gus_state.timer.dev);
            }
        }
    }
}

/*
    DMA emulation works as simple as it could be :D
    - check if "start DMA" in DMA Control (GF1 reg 0x41) is triggered
    - validate all fields
    - read start address and count from DMA controller
    - if ((count == 0) || (count = 0xFFFF)), ignore transfer
    - copy data to EMU8K onboard DRAM, yielding periodically to process IRQs
    - fake i8237 status TC bit set with 1 byte verify block transfer
      + set address/count regs to vaules at the end of DMA transfer
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
    outpw(emu8k_state.iobase + 0x802, EMU8K_REG_SMALD & 0xFF);
    while (count_now != 0) {
        uint32_t currentCount = (count_now >= 4) ? 4 : count_now;

        // read by 4 bytes, this will slow things down on misaligned addresses but I don't care
        uint32_t sample = (*(uint32_t*)src_now) ^ xormask;

        // NOTE - flush write buffer for every byte transferred, because DRAM upload time depends
        // on channel count allocated for DRAM, and apparently EMU8000 can't hold IOCHRDY too
        // long while write buffer full, due to ISA refresh cycles
        uint32_t sampleCount = currentCount;
        if (gus_state.emuflags & GUSEMU_SLOW_DRAM) while (sampleCount-- > 0) {
            emu8k_waitForWriteFlush(emu8k_state.iobase);
            emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, gus_state.mem8start + dst_now);
            emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALD, (sample & 0xFF) << 8);
            sample >>= 8;
            dst_now++;
        } else while (sampleCount-- > 0) {
            //emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALD, (sample & 0xFF) << 8);
            outpw(emu8k_state.iobase + 0x400, (sample & 0xFF) << 8);
            sample >>= 8;
        }

        src_now   += currentCount;
        count_now -= currentCount;
    }

    // check if 16bit samples enabled
    if ((gus_state.emuflags & GUSEMU_16BIT_SAMPLES) == 0) return 0;

    // do the same but for 16bit samples now :)
    count_now  = count;
    src_now    = src;
    dst_now    = dst >> 1;

    // flush stale data and set starting address
    emu8k_waitForWriteFlush(emu8k_state.iobase);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, gus_state.mem16start + dst_now);
    outpw(emu8k_state.iobase + 0x802, EMU8K_REG_SMALD & 0xFF);
    while (count_now != 0) {
        uint32_t currentCount = (count_now >= 4) ? 4 : count_now;

        // read by 4 bytes, this will slow things down on misaligned addresses but I don't care
        uint32_t sample = (*(uint32_t*)src_now) ^ xormask;

        uint32_t sampleCount = currentCount >> 1;
        if (gus_state.emuflags & GUSEMU_SLOW_DRAM) while (sampleCount-- > 0) {
            emu8k_waitForWriteFlush(emu8k_state.iobase);
            emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, gus_state.mem16start + dst_now);
            emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALD, sample & 0xFFFF);
            sample >>= 16;
            dst_now++;
        } else while (sampleCount-- > 0) {
            //emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALD, sample & 0xFFFF);
            outpw(emu8k_state.iobase + 0x400, sample & 0xFFFF);
            sample >>= 16;
        }

        src_now   += currentCount;
        count_now -= currentCount;
    }
    // flush stale data
    emu8k_waitForWriteFlush(emu8k_state.iobase);

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

    // flush stale data
    emu8k_waitForWriteFlush(emu8k_state.iobase);
    return 0;
}

uint32_t gusemu_dma_start() {
    uint32_t dmactrl = gus_state.gf1regs.dmactrl.h;

    // validate flags (dma started, playback, 8bit DMA)
    if ((dmactrl & 0x03) != 0x01) {
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
    uint32_t maxcount, countmask;
    uint32_t i8237_addr  = dmaGetCurrentAddress(gus_state.dma);
    uint32_t i8237_count = dmaGetCurrentCount(gus_state.dma);
    uint32_t dmapage     = dmaGetPage(gus_state.dma);

    // count == 0 or -1 - skip DMA, nothing to transfer
    if (i8237_count == 0xFFFF || i8237_count == 0) return GUSEMU_DMA_8237_ERROR; else i8237_count++;

    if (gus_state.dma >= 4) {
        // translate for 16-bit DMA channel
        maxcount      = 0x20000; countmask = 0x1FFFF;
        i8237_addr  <<= 1;
        i8237_count <<= 1;
        dmapage      &= 0xFE;
    } else {
        maxcount = 0x10000; countmask = 0xFFFF;
    }

    uint8_t *linsrc = (uint8_t*)((dmapage << 16) + i8237_addr);

    // translate GUS DMA address for 16-bit channels
    if ((gus_state.dma >= 4) && (dmactrl & 4))
        dmaaddr = (dmaaddr & 0xC0000) | ((dmaaddr & 0x1FFFF) << 1); 

    // run transfers
    if ((((i8237_addr + i8237_count) ^ i8237_addr) & ~countmask) != 0) {
        // crosses page boundary, break into 2 transfers
        uint32_t count1 = maxcount - i8237_addr;
        uint32_t count2 = i8237_count - count1;
        gusemu_dma_upload(dmaaddr,          linsrc,                    count1, dmactrl, xormask);
        gusemu_dma_upload(dmaaddr + count1, (uint8_t*)(dmapage << 16), count2, dmactrl, xormask);
        i8237_addr = count2;
    } else {
        // inside single page, copy as is
        gusemu_dma_upload(dmaaddr, linsrc, i8237_count, dmactrl, xormask);
        i8237_addr = (i8237_addr + i8237_count) & countmask;
    }
    i8237_addr >>= (gus_state.dma >= 4 ? 1 : 0);

    // fake block+verify 1 byte DMA transfer to fill TC bits in i8237 status register
    dmaSet8237Address(gus_state.dma, i8237_addr - 1);
    dmaSet8237Count(gus_state.dma, 0);
    dmaSetMode(gus_state.dma, dmaModeBlock | dmaModeInc | dmaModeVerify);

    switch((gus_state.emuflags & GUSEMU_TIMER_MODE_MASK)) {
        case GUSEMU_TIMER_IRQ:
            // post DMA TC delay
            if (gus_state.timer.flags & (GUSEMU_TIMER_T1_RUNNING|GUSEMU_TIMER_T2_RUNNING)) {
                // alright, timers are active. schedule DMA TC IRQ
                gus_state.dma_tc_delay = 2;
            } else {
                // needs to run timer by hand
                gus_state.dma_tc_delay = 1;
                gus_state.timer.dev->setDivisor(gus_state.timer.dev, 100);
                gus_state.timer.dev->start(gus_state.timer.dev);
            }
        case GUSEMU_TIMER_INSTANT:
            // post DMA TC delay
            gus_state.dma_tc_delay = 1;

            // process DMA TC stuff
            gusemu_update_dma_tc();

            // throw an IRQ right here
            if (gus_state.gf1regs.dmactrl.h & (1 << 5)) {
                bool do_irq = (gus_state.irqstatus == 0);
                gusemu_update_irq_status();
                if (do_irq) gusemu_send_irq();
            }
            break;
        default: 
            break;
    }

    return GUSEMU_DMA_OK;
}
