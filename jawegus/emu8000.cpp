#include <stdint.h>
#include "emu8000.h"
#include "inp.h"
#include "emu8kreg.h"

static const uint16_t emu8k_iobaseDefault[] = {0x620, 0x640, 0x660, 0x680, 0xFFFF};
static const uint16_t emu8k_ioregs[] = {0x0, 0x2, 0x400, 0x402, 0x800, 0x802};

void emu8k_write(uint32_t iobase, uint32_t reg, uint32_t data) {
    uint32_t regaddr = iobase + emu8k_ioregs[(reg & EMU8K_REG_MASK) >> EMU8K_REG_ADR_SHIFT];

    outpw(iobase + 0x802, reg & 0xFF);
    if (reg & EMU8K_REG_DWORD) outpd(regaddr, data); else outpw(regaddr, data);
}

uint32_t emu8k_read(uint32_t iobase, uint32_t reg) {
    uint32_t regaddr = iobase + emu8k_ioregs[(reg & EMU8K_REG_MASK) >> EMU8K_REG_ADR_SHIFT];

    outpw(iobase + 0x802, reg & 0xFF);
    return (reg & EMU8K_REG_DWORD) ? inpd(regaddr) : inpw(regaddr);
} 

// delay in 44100hz samples
void emu8k_delay(uint32_t iobase, uint32_t samples) {
    uint32_t wallclock = emu8k_read(iobase, EMU8K_REG_WC);
    while (((inpw(iobase + 0x402) - wallclock) & 0xFFFF) >= samples); 
}

void emu8k_hwinit(uint32_t iobase) {
    emu8k_write(iobase, EMU8K_REG_HWCF1, 0x0059);
    emu8k_write(iobase, EMU8K_REG_HWCF2, 0x0020);
    emu8k_write(iobase, EMU8K_REG_HWCF4, 0x0000);
    emu8k_write(iobase, EMU8K_REG_HWCF5, 0x0083);
    emu8k_write(iobase, EMU8K_REG_HWCF6, 0x8000);
}

void emu8k_dramEnable(uint32_t iobase, bool readWrite, uint32_t startChannel) {
    // allocate even channels for read, odd for write
    for (int ch = startChannel; ch < 30; ch++) {
        emu8k_write(iobase, ch + EMU8K_REG_DCYSUSV, 0x0080);
        emu8k_write(iobase, ch + EMU8K_REG_VTFT,    0);
        emu8k_write(iobase, ch + EMU8K_REG_CVCF,    0);
        emu8k_write(iobase, ch + EMU8K_REG_PSST,    0);
        emu8k_write(iobase, ch + EMU8K_REG_CSL,     0);
        emu8k_write(iobase, ch + EMU8K_REG_PTRX,    0x40000000);
        emu8k_write(iobase, ch + EMU8K_REG_CPF,     0x40000000);
        emu8k_write(iobase, ch + EMU8K_REG_CCCA,    ((ch & 1) && (readWrite)) ? 0x04000000 : 0x06000000);
    }
}

// 0 if timeout, non-0 if done
int emu8k_waitForWriteFlush(uint32_t iobase) {
    uint32_t timeout = (1 << 12);    // ~3ms on ISA 8.33Mhz
    outpw(iobase + 0x802, EMU8K_REG_SMALW & 0xFF); 
    while ((--timeout != 0) && ((inpd(iobase + 0x400) & (1 << 31)) != 0));
    
    return timeout;
}

// 0 if timeout, non-0 if done
int emu8k_waitForReadReady(uint32_t iobase) {
    uint32_t timeout = (1 << 12);    // ~3ms on ISA 8.33Mhz
    outpw(iobase + 0x802, EMU8K_REG_SMALR & 0xFF); 
    while ((--timeout != 0) && ((inpd(iobase + 0x400) & (1 << 31)) != 0));
    
    return timeout;
}

void emu8k_dramDisable(uint32_t iobase) {
    // wait for stale data
    emu8k_waitForWriteFlush(iobase);

    // deallocate channels
    for (int ch = 0; ch < 30; ch++) 
        emu8k_write(iobase, ch + EMU8K_REG_CCCA, 0);

    // don't touch envelope generator, leave it disabled
}

int emu8k_probe(uint32_t iobase) {
    // test for 44100hz wall clock incrementing
    uint32_t timeout = (1 << 12);    // ~3ms on ISA 8.33Mhz

    uint32_t wallclock = emu8k_read(iobase, EMU8K_REG_WC);
    while ((--timeout != 0) && (wallclock == inpw(iobase + 0x402)));  // assuming EMU8K_REG_WC is selected
    if (timeout == 0) return 0;

    // check if onboard memory present
    emu8k_dramEnable(iobase, true);
    emu8k_write(iobase, EMU8K_REG_SMALW, EMU8K_DRAM_OFFSET);
    emu8k_write(iobase, EMU8K_REG_SMALD, 0x55AA);
    emu8k_write(iobase, EMU8K_REG_SMALD, 0xCAFE);
    if (emu8k_waitForWriteFlush(iobase) == 0) return 0;
    emu8k_write(iobase, EMU8K_REG_SMALR, EMU8K_DRAM_OFFSET);
    emu8k_read (iobase, EMU8K_REG_SMALD); // dummy read
    if (emu8k_read(iobase, EMU8K_REG_SMALD) != 0x55AA) return 0;
    if (emu8k_read(iobase, EMU8K_REG_SMALD) != 0xCAFE) return 0;
    emu8k_dramDisable(iobase);

    return 1;
}

// probe DRAM installed, returns memory size in 16bit words
uint32_t emu8k_getMemSize(uint32_t iobase) {
    emu8k_dramEnable(iobase, true);
    emu8k_write(iobase, EMU8K_REG_SMALW, EMU8K_DRAM_OFFSET);
    emu8k_write(iobase, EMU8K_REG_SMALD, 0x55AA);
    emu8k_write(iobase, EMU8K_REG_SMALD, 0xCAFE);
    uint32_t offset = EMU8K_DRAM_OFFSET;
    while (offset < 14*1024*1024) {
        // wait for stale data
        if (emu8k_waitForWriteFlush(iobase) == 0) return 0;

        // test start of DRAM
        emu8k_write(iobase, EMU8K_REG_SMALR, EMU8K_DRAM_OFFSET);
        emu8k_read (iobase, EMU8K_REG_SMALD); // dummy read
        if (emu8k_read(iobase, EMU8K_REG_SMALD) != 0x55AA) break;
        if (emu8k_read(iobase, EMU8K_REG_SMALD) != 0xCAFE) break;

        offset += 0x8000;    // 32k words
        emu8k_write(iobase, EMU8K_REG_SMALW, offset);
        emu8k_write(iobase, EMU8K_REG_SMALD, 0xDEAD);
        emu8k_write(iobase, EMU8K_REG_SMALD, 0xF00D);
    }
    emu8k_dramDisable(iobase);

    return (offset - EMU8K_DRAM_OFFSET); // in words
}

// upload it straight, no loop fix/ramping
void emu8k_uploadSample(uint32_t iobase, uint32_t start, int16_t* data, uint32_t samples) {
    emu8k_write(iobase, EMU8K_REG_SMALW, start);
    _asm {
        mov     esi, [data]
        mov     ecx, [samples]
        mov     edx, [iobase]
        add     edx, 0x400
        rep     outsw
    }
}

// set flat equalizer (actually the only thing we touch in effect engine :)
void emu8k_setFlatEq(uint32_t iobase) {
    uint16_t bassData[]   = {0xC208, 0xC308, 0x0001};
    uint16_t trebleData[] = {0x821E, 0xD208, 0x031E, 0xD308, 0x021E, 0xD208, 0x831E, 0xD308, 0x0002};

    emu8k_write(iobase, EMU8K_REG_INIT4 + 0x01, bassData[0]);
    emu8k_write(iobase, EMU8K_REG_INIT4 + 0x11, bassData[1]);
    emu8k_write(iobase, EMU8K_REG_INIT3 + 0x11, trebleData[0]);
    emu8k_write(iobase, EMU8K_REG_INIT3 + 0x13, trebleData[1]);
    emu8k_write(iobase, EMU8K_REG_INIT3 + 0x1B, trebleData[2]);
    emu8k_write(iobase, EMU8K_REG_INIT4 + 0x07, trebleData[3]);
    emu8k_write(iobase, EMU8K_REG_INIT4 + 0x0B, trebleData[4]);
    emu8k_write(iobase, EMU8K_REG_INIT4 + 0x0D, trebleData[5]);
    emu8k_write(iobase, EMU8K_REG_INIT4 + 0x17, trebleData[6]);
    emu8k_write(iobase, EMU8K_REG_INIT4 + 0x19, trebleData[7]);
    uint16_t mix = bassData[2] + trebleData[8];
    emu8k_write(iobase, EMU8K_REG_INIT4 + 0x15, mix + 0x0262);
    emu8k_write(iobase, EMU8K_REG_INIT4 + 0x1D, mix + 0x8362);
}

void emu8k_disableOutput(uint32_t iobase) {
    emu8k_write(iobase, EMU8K_REG_HWCF3, 0x0000);
}

void emu8k_enableOutput(uint32_t iobase) {
    emu8k_write(iobase, EMU8K_REG_HWCF3, 0x0004);
}

int emu8k_initChannels(uint32_t iobase, uint32_t channels, uint32_t startChannel, bool initEnvelope) {
    // clear channel data
    for (int ch = startChannel; ch < (startChannel + channels); ch++) {
        // init envelope generator
        emu8k_write(iobase, ch + EMU8K_REG_DCYSUSV,  0x0080); // disable envelope generator
        if (initEnvelope) {
            emu8k_write(iobase, ch + EMU8K_REG_ENVVOL,   0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_ENVVAL,   0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_DCYSUS,   0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_ATKHLDV,  0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_LFO1VAL,  0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_ATKHLD,   0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_LFO2VAL,  0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_IP,       0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_IFATN,    0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_PEFE,     0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_FMMOD,    0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_TREMFREQ, 0x0000);
            emu8k_write(iobase, ch + EMU8K_REG_FM2FRQ2,  0x0000);
        }

        // init sound generator
        emu8k_write(iobase, ch + EMU8K_REG_PTRX,     0x00000000);
        emu8k_write(iobase, ch + EMU8K_REG_VTFT,     0x0000FFFF);
        //emu8k_write(iobase, ch + EMU8K_REG_PSST,     0x00000000);
        //emu8k_write(iobase, ch + EMU8K_REG_CSL,      0x00000000);
        //emu8k_write(iobase, ch + EMU8K_REG_CCCA,     0x00000000);
        emu8k_write(iobase, ch + EMU8K_REG_CPF,      0x00000000);
        emu8k_write(iobase, ch + EMU8K_REG_CVCF,     0x0000FFFF);
    }

    return 1;
}
