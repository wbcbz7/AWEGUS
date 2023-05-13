#pragma once
#include <stdint.h>
#include "iotrap.h"
#include "gf1defs.h"
#include "timer.h"

#pragma pack(push, 1)
// gusemu init structure
struct gusemu_init_t {
    uint32_t emuflags;      // emulation flags (see below)
    uint32_t memsize;       // in KBYTES
    irq_timer_t *timer;     // timer object pointer

    // SB16 resources
    uint16_t sbbase;
    uint8_t  sbirq;
    uint8_t  sbdma;

    uint16_t emubase;       // emu8000 base address
    uint16_t timerbase;     // emulation timer base
    uint8_t  timerirq;      // --//--          irq
    uint8_t  timerdma;      // --//--          dma (if needed)

    uint16_t gusbase;       // emulated gus base
    uint8_t  gusirq;        // emulated gus irq
    uint8_t  gusdma;        // emulated gus dma

    int32_t  gain;              // volume gain
    int32_t  mute_threshold;
};
#pragma pack(pop)

// gusemu command line flags
struct gusemu_cmdline_t {
    bool        help;           // show help
    uint32_t    dramsize;       // override DRAM size
    bool        mono;           // force mono panning
    bool        slowdram;       // use ch28-29 only for DRAM access (slooww)
    bool        en16bit;        // enable 16bit samples
    uint32_t    irqemu;         // emulate irq
    bool        dmaemu;         // emulate dma
    bool        ignore_2x6;     // ignore 2x6 and always send IRQ
    bool        disable_fm;     // disable FM passthru, add channels 30-31 to the pool
    int32_t     gain;           // volume gain in 0.375 db units
    int32_t     mute_threshold; // mute (-inf) threshold in GF1 volume, before applying gain
};

enum {
    IRQEMU_MODE_SB16 = 0,
    IRQEMU_MODE_COM1,
    IRQEMU_MODE_COM2,
    IRQEMU_MODE_COUNT
};

// gus emulation structures

// shadow GF1 registers. note all are 16 bit
union gf1reg_t {
    uint16_t    w;
    struct {
        uint8_t l;
        uint8_t h;
    };
};

union gf1regs_channel_t {
    struct {
        gf1reg_t ctrl;
        gf1reg_t freq;
        gf1reg_t start_high;
        gf1reg_t start_low;
        gf1reg_t end_high;
        gf1reg_t end_low;
        gf1reg_t ramp_rate;
        gf1reg_t ramp_start;
        gf1reg_t ramp_end;
        gf1reg_t volume;
        gf1reg_t pos_high;
        gf1reg_t pos_low;
        gf1reg_t pan;
        gf1reg_t volctrl;
        gf1reg_t ctrl_r;        // ctrl previous value, hidden
        gf1reg_t dummy;         // for alignment
    };
    gf1reg_t     raw[0x10];
};

struct gf1regs_shadow_t {
    // per channel (0x00..0xD)
    gf1regs_channel_t chan[32];

    // global (0x40..0x4F)
    union {
        struct {
            gf1reg_t    dummy_0x40;
            gf1reg_t    dmactrl;
            gf1reg_t    dmaaddr;
            gf1reg_t    dramlow;
            gf1reg_t    dramhigh;
            gf1reg_t    timerctrl;
            gf1reg_t    timer1count;
            gf1reg_t    timer2count;
            gf1reg_t    recfreq;
            gf1reg_t    recctrl;
        };
        gf1reg_t global[0x49 - 0x40 + 1];
    };

    // special (handled separately)
    uint8_t     active_channels;
    uint8_t     reset, reset_r;
    uint8_t     irqstatus;

    // 3x2-3x5 reflected io
    uint8_t reflected_io;
};

enum {
    // enable IRQ emulation
    GUSEMU_EMULATE_IRQ      = (1 << 0),

    // enable DMA emulation
    GUSEMU_EMULATE_DMA      = (1 << 1),

    // timer emulation mode
    GUSEMU_TIMER_MODE_MASK  = (3 << 2),
    GUSEMU_TIMER_INSTANT    = (0 << 2), // none actually emulated, timer expires instantly and does not restarts
    GUSEMU_TIMER_IRQ        = (1 << 2), // run timer emulation IRQ

    // emulate 16bit samples in addition to 8bit
    GUSEMU_16BIT_SAMPLES    = (1 << 6),

    // emulate wavetable/ramp/rollover IRQs
    GUSEMU_EMULATE_WAVEIRQ  = (1 << 7),

    // fixup one-shot/looped samples to reduce clicks/whine
    GUSEMU_FIX_SAMPLE_END   = (1 << 8),

    // emulate bidirectional samples by loop doubling
    GUSEMU_EMULATE_BIDIR    = (1 << 9),

    // remove unnecessary delays
    GUSEMU_PATCH_DELAYS     = (1 << 10),

    // use mono panning
    GUSEMU_MONO_PANNING     = (1 << 11),

    // "slow DRAM" mode
    GUSEMU_SLOW_DRAM        = (1 << 12),

    // ignore non-zero IRQ status (2x6) in IRQ emualtion
    GUSEMU_IRQ_IGNORE_2X6   = (1 << 13),

    // disable FM passthrough
    GUSEMU_DISABLE_FM       = (1 << 14),

    // -------------------
    // gus is muted
    GUSEMU_STATE_MUTED      = (1 << 31),
};

enum {
    GUSEMU_TIMER_T1_RUNNING         = (1 << 0),
    GUSEMU_TIMER_T1_OVERFLOW        = (1 << 1),
    GUSEMU_TIMER_T1_IRQ             = (1 << 2),
    GUSEMU_TIMER_T1_ADLIB_UNMASKED  = (1 << 3),
    GUSEMU_TIMER_T2_RUNNING         = (1 << 5),
    GUSEMU_TIMER_T2_OVERFLOW        = (1 << 6),
    GUSEMU_TIMER_T2_IRQ             = (1 << 7),
    GUSEMU_TIMER_T2_ADLIB_UNMASKED  = (1 << 8),

    GUSEMU_TIMER_DMA_TC_IRQ_POSTED  = (1 << 9)
};

// global GUS state
struct gus_state_t {
    // emulation flags!
    uint32_t emuflags;

    // GUS emulation resources
    uint16_t    iobase;         // I/O base port
    uint8_t     irq, intr;      // irq - physical GF1/Timer IRQ, intr - corresponding software interrupt number
    uint8_t     dma;            // physical DMA channel
    uint8_t     dma_tc_delay;   // delay in timer ticks for DMA TC IRQ

    // timer emulation data
    struct {
        // both timers are emulated as following:
        // since timer counter readback is unsupported on GUS, we can safely emulate one(!) timer
        // by running other IRQ-based timer (IRQ0, SB16 playback, COM port, etc.., called "emulation timer")
        // at GUS timer rate and firing "expire" bit and/or GUS IRQ from emulation timer IRQ
        // if both timers are enabled, emulation timer is running at fastest from two GUS timers rate
        // and "slower" GUS timer is emulated by delta incrementing.
        // also, we can support RTC periodic IRQ as emulation timer by those accumulators
        
        // wavetable/ramp emulation procedure runs as "third" timer. may use "faster" timer frequency,
        // or run as free-running timer
        uint32_t flags;

        uint16_t t1_pos; // 1.15fx, that's sufficient enough
        uint16_t t1_add; // --//--

        uint16_t t2_pos; // --//--
        uint16_t t2_add; // --//--

        // emulation timer device
        irq_timer_t *dev;
    } timer;
    
    // --------------------------------
    // GUS HARDWARE STATE

    // GF1 shadow registers
    gf1regs_shadow_t    gf1regs;

    // IRQ/DMA control (2xB)
    uint8_t     irq_2xb, dma_2xb; 

    // 2xB Register Select (2xF)
    uint8_t     sel_2xb;

    // Timer Index (2x8, currently selected register, read via 2xA)
    uint8_t     timerindex;
    // Timer Data (2x9 reg 0x04)
    uint8_t     timerdata;
    // Timer Data latch (2x9 reg other than 0x04)
    uint8_t     timerlatch;

    // Mix Control Register (2x0)
    uint8_t     mixctrl;

    // IRQ Status Register (2x6)
    uint8_t     irqstatus;

    // Page/Channel Index (3x2) and Register Index (3x3)
    union {
        struct {
            uint8_t channel;
            uint8_t reg;
        };
        uint16_t w;
    } pagereg;

    // --------------------------------
    // helper stuff

    // last DRAM write pointer (reads are always uncached)
    uint32_t dramwritepos;

    // DRAM address mask
    uint32_t drammask;

    // memory pools start
    uint32_t mem8start,  mem8len;
    uint32_t mem16start, mem16len;

    // 16bit data latch
    gf1reg_t wordlatch;
    uint8_t  wordlatch_active;
    uint8_t  dummy3;

    // --------------------------------
    // volume gain/threshold
    int32_t  gain;
    int32_t  gain_init;
    int32_t  mute_threshold;

};

// GUS->EMU8K emulation state
struct gus_emu8k_state_t {
    uint32_t iobase;

    // lookup bitfields for wavetable/ramp IRQ emulation
    struct {
        uint32_t wavemask;
        uint32_t rampmask;
        uint32_t rollovermask;
    } irqemu;

    // channel flags/cache {
    struct emu8k_chan_t {
        uint32_t flags, update_flags;
        uint32_t pos, loopstart, loopend;
        uint16_t freq, volume;
        uint8_t  pan, reverb, chorus, dummy;
        uint16_t logpitch, logvolume;
    } chan[32];

    // real active channels count
    uint32_t active_channels;
    // maximum active channels permitted
    uint32_t max_channels;
};

// ----------------
// externs!
extern gus_state_t gus_state;
extern gus_emu8k_state_t emu8k_state;
extern gusemu_cmdline_t gusemu_cmdline;

// reset emulated gus state
uint32_t gusemu_reset(bool full_reset, bool touch_reset_reg);

// init gusemu
uint32_t gusemu_init(gusemu_init_t *init);

// deinit gusemu
uint32_t gusemu_deinit();

// port 3x2/3x3 trap
uint32_t __trapcall gusemu_3x2_r8_trap (uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x3_r8_trap (uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x2_w8_trap (uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x3_w8_trap (uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x2_r16_trap(uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x2_w16_trap(uint32_t port, uint32_t data, uint32_t flags);

// port 3x4/3x5 trap
uint32_t __trapcall gusemu_3x4_r8_trap (uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x5_r8_trap (uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x4_w8_trap (uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x5_w8_trap (uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x4_r16_trap(uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x4_w16_trap(uint32_t port, uint32_t data, uint32_t flags);

// port 3x7 (DRAM data) trap
uint32_t __trapcall gusemu_3x7_r8_trap (uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x7_w8_trap (uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_3x7_w8_trap_16bit(uint32_t port, uint32_t data, uint32_t flags);

// port 2x0 (mix control register) trap
uint32_t __trapcall gusemu_2x0_w8_trap(uint32_t port, uint32_t data, uint32_t flags);

// port 2x6 (IRQ status) trap
uint32_t __trapcall gusemu_2x6_r8_trap(uint32_t port, uint32_t data, uint32_t flags);

// port 2x8/2x9 (timer stuff) trap
uint32_t __trapcall gusemu_2x8_r8_trap(uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_2x8_w8_trap(uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_2x9_r8_trap(uint32_t port, uint32_t data, uint32_t flags);
uint32_t __trapcall gusemu_2x9_w8_trap(uint32_t port, uint32_t data, uint32_t flags);

// port 2xA (Timer Index readback) trap
uint32_t __trapcall gusemu_2xa_r8_trap(uint32_t port, uint32_t data, uint32_t flags);

// port 2xB (IRQ/DMA Control) trap
uint32_t __trapcall gusemu_2xb_w8_trap(uint32_t port, uint32_t data, uint32_t flags);

// port 2xF (2xB Register Select) trap
uint32_t __trapcall gusemu_2xf_w8_trap(uint32_t port, uint32_t data, uint32_t flags);

// dummy trap - block access to i/o ports (writes do nothing, reads return -1)
uint32_t __trapcall gusemu_dummy_trap(uint32_t port, uint32_t data, uint32_t flags);

// debug break trap - dumps gusemu state upon assertion
uint32_t __trapcall gusemu_debug_w8_trap(uint32_t port, uint32_t data, uint32_t flags);

// ----------------
void gusemu_gf1_write(uint32_t reg, uint32_t ch, uint32_t data);
uint32_t gusemu_gf1_read(uint32_t reg, uint32_t ch);

// ----------------

// functions exported for IRQ emulation
// TODO: it's becoming messy!

bool gusemu_process_timers();

// ----------------

// channel update flags
enum {
    GUSEMU_CHAN_UPDATE_CTRL         = (1 <<  0),
    GUSEMU_CHAN_UPDATE_FREQ         = (1 <<  1),
    GUSEMU_CHAN_UPDATE_LOOPSTART    = (1 <<  2),
    GUSEMU_CHAN_UPDATE_LOOPEND      = (1 <<  3),
    GUSEMU_CHAN_UPDATE_RAMPRATE     = (1 <<  4),
    GUSEMU_CHAN_UPDATE_RAMPSTART    = (1 <<  5),
    GUSEMU_CHAN_UPDATE_RAMPEND      = (1 <<  6),
    GUSEMU_CHAN_UPDATE_VOLUME       = (1 <<  7),
    GUSEMU_CHAN_UPDATE_POS          = (1 <<  8),
    GUSEMU_CHAN_UPDATE_PAN          = (1 <<  9),
    GUSEMU_CHAN_UPDATE_VOLCTRL      = (1 << 10),

    // specical conditions
    GUSEMU_CHAN_UPDATE_START        = (1 << 11),
    GUSEMU_CHAN_UPDATE_STOP         = (1 << 12),
};

enum {
    EMUSTATE_CHAN_ONESHOT           = (1 << 0),
    EMUSTATE_CHAN_16BIT             = (1 << 1),
    EMUSTATE_CHAN_BIDIR             = (1 << 2),
    EMUSTATE_CHAN_INVALID_POS       = (1 << 3),
    EMUSTATE_CHAN_RAMP_IN_PROGRESS  = (1 << 4),
};

// timer update flags
enum {
    GUSEMU_TIMER_UPDATE_TIMERDATA   = (1 << 0), // 2x9
    GUSEMU_TIMER_UPDATE_TIMERCTRL   = (1 << 1), // GF1 REG 0x45!!
    GUSEMU_TIMER_UPDATE_T1COUNT     = (1 << 2), // GF1 REG 0x46
    GUSEMU_TIMER_UPDATE_T2COUNT     = (1 << 3), // GF1 REG 0x47
};

// global constants
enum {
    GUSEMU_GUS_TIMER_CLOCK              = 12500,
    GUSEMU_MAX_EMULATED_CHANNELS        = 28,
    GUSEMU_MAX_EMULATED_CHANNELS_NOFM   = 30,
};
