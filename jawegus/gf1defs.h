#pragma once

#include <stdint.h>

// i/o port defines
enum {
    // direct ports, offset from base
    GF1_PORT_MIDI_CTRL              = 0x300 - 0x200,
    GF1_PORT_MIDI_DATA              = 0x301 - 0x200,
    GF1_PORT_CHANNEL_INDEX          = 0x302 - 0x200,
    GF1_PORT_REG_INDEX              = 0x303 - 0x200,
    GF1_PORT_DATA_LOW               = 0x304 - 0x200,
    GF1_PORT_DATA_HIGH              = 0x305 - 0x200,
//  GF1_PORT_MAX_CTRL               = 0x306 - 0x200,        // gus max, ignored
    GF1_PORT_DRAM_DATA              = 0x307 - 0x200,

    GF1_PORT_MIX_CTRL               = 0x200 - 0x200,
    GF1_PORT_IRQ_STATUS             = 0x206 - 0x200,
    GF1_PORT_TIMER_CTRL             = 0x208 - 0x200,
    GF1_PORT_TIMER_DATA             = 0x209 - 0x200,
    GF1_PORT_IRQ_DMA_CTRL           = 0x20B - 0x200,
};

// register defines
enum {
    GF1_REG_DMA_CTRL                = 0x41,
    GF1_REG_DMA_ADDR                = 0x42,
    GF1_REG_DRAM_ADDR_LOW           = 0x43,
    GF1_REG_DRAM_ADDR_HIGH          = 0x44,
    GF1_REG_TIMER_CTRL              = 0x45,
    GF1_REG_TIMER1_COUNT            = 0x46,
    GF1_REG_TIMER2_COUNT            = 0x47,
    GF1_REG_SAMPLING_FREQ           = 0x48,
    GF1_REG_SAMPLING_CTRL           = 0x49,
    GF1_REG_JOY_TRIMDAC             = 0x4B,
    GF1_REG_RESET                   = 0x4C,

    GF1_REG_CHAN_READ               = 0x80,

    GF1_REG_CHAN_CTRL               = 0x00,
    GF1_REG_CHAN_FREQ               = 0x01,
    GF1_REG_CHAN_START_HIGH         = 0x02,
    GF1_REG_CHAN_START_LOW          = 0x03,
    GF1_REG_CHAN_END_HIGH           = 0x04,
    GF1_REG_CHAN_END_LOW            = 0x05,
    GF1_REG_CHAN_RAMP_RATE          = 0x06,
    GF1_REG_CHAN_RAMP_START         = 0x07,
    GF1_REG_CHAN_RAMP_END           = 0x08,
    GF1_REG_CHAN_VOLUME             = 0x09,
    GF1_REG_CHAN_POS_HIGH           = 0x0A,
    GF1_REG_CHAN_POS_LOW            = 0x0B,
    GF1_REG_CHAN_PAN                = 0x0C,
    GF1_REG_CHAN_VOL_CTRL           = 0x0D,

    GF1_REG_ACTIVE_CHANS            = 0x0E,
    GF1_REG_IRQ_STATUS              = 0x0F,
};

// register bitfields
enum {
    // mix control (2x0)
    GF1_MIXCTRL_LINEIN      = (1 << 0),
    GF1_MIXCTRL_LINEOUT     = (1 << 1),
    GF1_MIXCTRL_MICIN       = (1 << 2),
    GF1_MIXCTRL_LATCH_EN    = (1 << 3),
    GF1_MIXCTRL_MERGE_IRQ   = (1 << 4),
    GF1_MIXCTRL_MIDI_LOOP   = (1 << 5),
    GF1_MIXCTRL_2XB_SEL_IRQ = (1 << 6),
    
    // irq status (2x6)
    GF1_IRQSTATUS_MIDI_TX   = (1 << 0),
    GF1_IRQSTATUS_MIDI_RX   = (1 << 1),
    GF1_IRQSTATUS_TIMER1    = (1 << 2),
    GF1_IRQSTATUS_TIMER2    = (1 << 3),
    GF1_IRQSTATUS_WAVE      = (1 << 5),
    GF1_IRQSTATUS_RAMP      = (1 << 6),
    GF1_IRQSTATUS_DMA_TC    = (1 << 7),

    // timer ctrl (2x8)
    GF1_TIMERCTRL_TIMER1_TC = (1 << 6),
    GF1_TIMERCTRL_TIMER2_TC = (1 << 5),

    // timer data (2x9)
    GF1_TIMERDATA_IRQ_CLEAR     = (1 << 7),
    GF1_TIMERDATA_TIMER1_MASK   = (1 << 6),
    GF1_TIMERDATA_TIMER2_MASK   = (1 << 5),
    GF1_TIMERDATA_TIMER2_START  = (1 << 1),
    GF1_TIMERDATA_TIMER1_START  = (1 << 0),

    // irq/dma control (2xB)
    GF1_IRQCTRL_IRQ1_SHIFT      = 0,
    GF1_IRQCTRL_IRQ2_SHIFT      = 3,
    GF1_IRQCTRL_IRQ1_MASK       = (7 << GF1_IRQCTRL_IRQ1_SHIFT),
    GF1_IRQCTRL_IRQ2_MASK       = (7 << GF1_IRQCTRL_IRQ2_SHIFT),
    GF1_IRQCTRL_IRQ_MERGE       = (1 << 6),

    GF1_DMACTRL_DMA1_SHIFT      = 0,
    GF1_DMACTRL_DMA2_SHIFT      = 3,
    GF1_DMACTRL_DMA1_MASK       = (7 << GF1_DMACTRL_DMA1_SHIFT),
    GF1_DMACTRL_DMA2_MASK       = (7 << GF1_DMACTRL_DMA2_SHIFT),
    GF1_DMACTRL_DMA_MERGE       = (1 << 6),

    // DRAM DMA control (reg 0x41)
    GF1_DMACTRL_INVERT_MSB      = (1 << 7),
    GF1_DMACTRL_DATA_16BIT      = (1 << 6),
    GF1_DMACTRL_IRQ_PENDING     = (1 << 6),
    GF1_DMACTRL_IRQ_EN          = (1 << 5),
    GF1_DMACTRL_RATE_MASK       = (3 << 3),
    GF1_DMACTRL_CHAN_16BIT      = (1 << 2),
    GF1_DMACTRL_DIR             = (1 << 1),
    GF1_DMACTRL_START           = (1 << 0),

    // timer control (reg 0x45)
    GF1_TIMERCTRL_TIMER2_IRQ_EN = (1 << 3),
    GF1_TIMERCTRL_TIMER1_IRQ_EN = (1 << 2),

    // reset (0x4C)
    GF1_RESET_MASTER            = (1 << 0),
    GF1_RESET_DAC_ENABLE        = (1 << 1),
    GF1_RESET_MASTER_IRQ_EN     = (1 << 3),

    // channel/voice control (0x00)
    GF1_CHANCTRL_STOPPED        = (1 << 0),
    GF1_CHANCTRL_FORCE_STOP     = (1 << 1),
    GF1_CHANCTRL_16BIT          = (1 << 2),
    GF1_CHANCTRL_LOOP           = (1 << 3),
    GF1_CHANCTRL_BIDIR          = (1 << 4),
    GF1_CHANCTRL_IRQ_EN         = (1 << 5),
    GF1_CHANCTRL_DIR            = (1 << 6),
    GF1_CHANCTRL_IRQ_PENDING    = (1 << 7),

    GF1_VOLCTRL_STOPPED         = (1 << 0),
    GF1_VOLCTRL_FORCE_STOP      = (1 << 1),
    GF1_VOLCTRL_ROLLOVER        = (1 << 2),
    GF1_VOLCTRL_LOOP            = (1 << 3),
    GF1_VOLCTRL_BIDIR           = (1 << 4),
    GF1_VOLCTRL_IRQ_EN          = (1 << 5),
    GF1_VOLCTRL_DIR             = (1 << 6),
    GF1_VOLCTRL_IRQ_PENDING     = (1 << 7),

    GF1_IRQ_CHAN_MASK           = (31 << 0),
    GF1_IRQ_RAMP_PENDING        = (1 << 6),
    GF1_IRQ_WAVE_PENDING        = (1 << 7),
};
