#include <stdint.h>
#include "vmm.h"
#include "jlm.h"
#include "utils.h"
#include "gusemu.h"
#include "dmaemu.h"
#include "console.h"
#include "iotrap.h"
#include "emu8000.h"
#include "fxmath.h"
#include "irqemu.h"

// ----------------------------
// INTERNAL STATIC GUSEMU STRUCTURES (beware!)
gus_state_t gus_state;
gus_emu8k_state_t emu8k_state;

// do everything here. really

// ----------------------------
// forward declarations
void gusemu_update_active_channel_count();
void gusemu_update_timers(uint32_t flags);

// ----------------------------
void gusemu_emu8k_reset() {
    // reset EMU8000
    //emu8k_hwinit(emu8k_state.iobase);
    emu8k_initChannels(emu8k_state.iobase, GUSEMU_MAX_EMULATED_CHANNELS);
    emu8k_dramEnable(emu8k_state.iobase, true, 0);
    emu8k_setFlatEq(emu8k_state.iobase);
    // reset DRAM pointers
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALR, EMU8K_DRAM_OFFSET);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, EMU8K_DRAM_OFFSET);
}

// reset emulation
uint32_t gusemu_reset(bool full_reset, bool touch_reset_reg) {
    if (full_reset) {
        // reset memory pools
        gus_state.mem8start     = EMU8K_DRAM_OFFSET;
        gus_state.mem8len       = 0;
        gus_state.mem16start    = EMU8K_DRAM_OFFSET;
        gus_state.mem16len      = 0;

        // reset flags
        gus_state.emuflags = 0;

        // reset resources
        gus_state.iobase = 0;
        gus_state.irq    = gus_state.dma = -1;

        // reset EMU8000
        gusemu_emu8k_reset();

        // clear all GF1 registers
        for (int ch = 0; ch < 32; ch++) {
            tiny_memset(&gus_state.gf1regs.chan[ch], 0, sizeof(gus_state.gf1regs.chan[ch]));
        }
    }

    // stop emulation timer (TODO)
    gus_state.timer.flags = 0;
    // stop pending DMA stuff (TODO)
    emu8k_state.irqemu.rampmask = emu8k_state.irqemu.wavemask = emu8k_state.irqemu.rollovermask = 0;

    // reset DRAM interface emulation
    gus_state.wordlatch.w = 0;
    gus_state.wordlatch_active = 0;
    gus_state.dramwritepos = 0;
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALR, EMU8K_DRAM_OFFSET);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, EMU8K_DRAM_OFFSET);

    // NOTE: interwave docs mention that not all registers are actually reset
    // reset general registers to defaults
    gus_state.mixctrl    = 0x03;                        // mixctrl, 2x0
    gus_state.timerindex = 0;                           // timer index, 2x8
    gus_state.timerdata  = 0;                           // timer data,  2x9
    gus_state.sel_2xb    = 0;                           // 2xB register select, 2xF
    gus_state.gf1regs.active_channels   = 0xCD; // 14 active channels
    emu8k_state.active_channels         = GUSEMU_MAX_EMULATED_CHANNELS; // to trigger all channels update

    // reset GF1 registers
    gus_state.pagereg.w             = 0;
    gus_state.gf1regs.reflected_io  = 0;
    gus_state.gf1regs.timer1count.w = 0;
    gus_state.gf1regs.timer2count.w = 0;
    gus_state.gf1regs.timerctrl.w   = 0;
    gus_state.gf1regs.dmactrl.w     = 0;
    gus_state.gf1regs.dmaaddr.w     = 0;
    gus_state.gf1regs.dramlow.w     = 0;
    gus_state.gf1regs.dramhigh.w    = 0;

    // reset channel registers
    for (int ch = 0; ch < 32; ch++) {
        // reset EMU8000 state and mute channel
        tiny_memset(emu8k_state.chan + ch, 0, sizeof(emu8k_state.chan[0]));

        // reset GUS state

        gus_state.gf1regs.chan[ch].ctrl.w       = 0x0100;   // 00: control
        gus_state.gf1regs.chan[ch].ctrl_r.w     = 0x0100;   // 00: control prev
        /*
        gus_state.gf1regs.chan[ch].freq.w       = 0x0400;   // 01: freq
        gus_state.gf1regs.chan[ch].start_high.w = 0x0000;   // 02: start high
        gus_state.gf1regs.chan[ch].start_low.w  = 0x0000;   // 03: start low
        gus_state.gf1regs.chan[ch].end_high.w   = 0x0000;   // 04: end   high
        gus_state.gf1regs.chan[ch].end_low.w    = 0x0000;   // 05: end   low
        gus_state.gf1regs.chan[ch].ramp_rate.w  = 0x0000;   // 06: ramp  rate
        gus_state.gf1regs.chan[ch].ramp_start.w = 0x0000;   // 07: ramp  start
        gus_state.gf1regs.chan[ch].ramp_end.w   = 0x0000;   // 08: ramp  end
        gus_state.gf1regs.chan[ch].volume.w     = 0x0000;   // 09: volume
        gus_state.gf1regs.chan[ch].pos_low.w    = 0x0000;   // 0A: pos   high
        gus_state.gf1regs.chan[ch].pos_high.w   = 0x0000;   // 0B: pos   low
        gus_state.gf1regs.chan[ch].pan.w        = 0x0700;   // 0C: pan
        */
        gus_state.gf1regs.chan[ch].volctrl.w    = 0x0100;   // 0D: ramp  control
    }

    // stop emu8k channels
    for (int ch = 0; ch < GUSEMU_MAX_EMULATED_CHANNELS; ch++) {
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_PTRX, 0x00000000);
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_CPF,  0x00000000);
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_VTFT, 0x0000FFFF);
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_CVCF, 0x0000FFFF);
    }

    // reinit active channels
    gusemu_update_active_channel_count();

    // reinit timer stuff
    gusemu_update_timers(GUSEMU_TIMER_UPDATE_TIMERCTRL|GUSEMU_TIMER_UPDATE_TIMERDATA);

    // reset reset register (lol) if requested
    if (touch_reset_reg) gus_state.gf1regs.reset = 0;

    // ok
    return 1;
}

// initilaize emulation
uint32_t gusemu_init(gusemu_init_t *init) {
    // uninit and reset emulation completely
    gusemu_deinit();
    gusemu_reset(true, true);

    // set emulated resources
    gus_state.emuflags          = init->emuflags;     // TODO: parse!
    gus_state.iobase            = init->gusbase;
    gus_state.irq               = init->gusirq;
    gus_state.intr              = (gus_state.irq >= 8) ? gus_state.irq + 0x70 : gus_state.irq + 8;
    gus_state.dma               = init->gusdma;
    emu8k_state.iobase          = init->emubase;

    // install port traps
    if (iotrap_install(gus_state.iobase) == 0) {
        puts("error: unable to install I/O port traps!\r\n");
        goto _fail_cleanup;
    }

    // init EMU8000 as following: 14 channels for playback, rest (interleaved) for local memory access and FM passthru
    gusemu_emu8k_reset();

    // init IRQ emulation
    if (gus_state.emuflags & GUSEMU_EMULATE_IRQ) {
        // init IRQ emulation timer here
        gus_state.timer.dev         = init->timer;

        if (gus_state.timer.dev == 0) {
            puts("error: null timer device!\r\n");
            goto _fail_cleanup;
        }

        irq_timer_resources_t timer_res = {
            init->timerbase, init->timerirq, init->timerdma, 0
        };

        // install timer IRQ handler
        if (gus_state.timer.dev->init(gus_state.timer.dev, 0, &timer_res) != 0) {
            puts("error: unable to init timer device!\r\n");
            goto _fail_cleanup;
        }

        // set callback proc
        if (gus_state.timer.dev->setCallback(gus_state.timer.dev, gusemu_timer_irq_callback, 0) != 0) {
            puts("error: unable to register timer callback!\r\n");
            goto _fail_cleanup;
        }
    }

    // init memory pool for emulation
    gus_state.mem8start = EMU8K_DRAM_OFFSET;
    gus_state.mem8len   = init->memsize;
    if (gus_state.emuflags & GUSEMU_16BIT_SAMPLES) {
        gus_state.mem16start  = EMU8K_DRAM_OFFSET + gus_state.mem8len;
        gus_state.mem16len    = init->memsize >> 1; // since we're merging 16bit writes
    }
    gus_state.drammask = (gus_state.mem8len - 1);

    // done
    return 1;

_fail_cleanup:
    gusemu_deinit();

    return 0;
}

// deinit emulation
uint32_t gusemu_deinit() {
    // uninstall port traps
    iotrap_uninstall(gus_state.iobase);

    // remove IRQ virtualization
    if (gus_state.emuflags & GUSEMU_EMULATE_IRQ) {
        gus_state.timer.dev->stop(gus_state.timer.dev);
        gus_state.timer.dev->done(gus_state.timer.dev);
    }

    // reset EMU8000
    // no need to reset emulated GUS state as we'll kill emulation anyway
    gusemu_emu8k_reset();

    return 1;
}

// -------------------------------

// update channel state (stopped, etc), trigger IRQs

// pitch conversion (6.10fx -> (2.14fx << 8))
#define GF1_EMU8K_PITCH(ch) (154350 << 12) / (ch * 11025)
static const uint32_t gf1_to_emu8k_pitchtab[] = {
    GF1_EMU8K_PITCH(14), GF1_EMU8K_PITCH(15), GF1_EMU8K_PITCH(16), GF1_EMU8K_PITCH(17),
    GF1_EMU8K_PITCH(18), GF1_EMU8K_PITCH(19), GF1_EMU8K_PITCH(20), GF1_EMU8K_PITCH(21),
    GF1_EMU8K_PITCH(22), GF1_EMU8K_PITCH(23), GF1_EMU8K_PITCH(24), GF1_EMU8K_PITCH(25),
    GF1_EMU8K_PITCH(26), GF1_EMU8K_PITCH(27), GF1_EMU8K_PITCH(28), GF1_EMU8K_PITCH(29),
    GF1_EMU8K_PITCH(30), GF1_EMU8K_PITCH(31), GF1_EMU8K_PITCH(32)
};

// translate gf1->emu8k pitch, overflow is not handled
uint32_t gusemu_translate_pitch(uint32_t gf1pitch) {
    uint32_t chans = (gus_state.gf1regs.active_channels & 31) + 1;
    return (gf1pitch * gf1_to_emu8k_pitchtab[chans - 14]) >> 8;
}

// transate gf1->emu8k volume, gf1vol in 4.12 format
uint32_t gusemu_translate_volume(uint32_t gf1vol) {
    // this one if pretty straightforward, note extra precision
    return (gf1vol == 0) ? 0 : ((4096 + (gf1vol & 0xFFF)) << (gf1vol >> 12)) >> 12; // ugh
}

// set and translate position
uint32_t gusemu_translate_pos(uint32_t ch, uint32_t pos) {
    uint32_t emupos;

    // translate for 8/16 bit samples
    if ((emu8k_state.chan[ch].flags & EMUSTATE_CHAN_16BIT)) {
        // do that weird GUS 16bit xlat
        emupos = ((pos & 0xC0000) | ((pos & 0x1FFFF) << 1)) >> 1;
        // and add 16bit pool offset
        emupos = (emupos + gus_state.mem16start);
    } else {
        // no translation required
        emupos = pos + gus_state.mem8start;
    }

    return emupos;
}

// update active channels count
void gusemu_update_active_channel_count() {
    uint32_t oldchans = emu8k_state.active_channels;
    uint32_t newchans = (gus_state.gf1regs.active_channels & 31) + 1;

    // clamp
    if (newchans < 14) newchans = 14;
    if (newchans > GUSEMU_MAX_EMULATED_CHANNELS) newchans = GUSEMU_MAX_EMULATED_CHANNELS;

    // reinit emu8k dram interface
    // wait for pending write flush
    emu8k_waitForWriteFlush(emu8k_state.iobase);
    if (newchans > oldchans) for (int ch = oldchans; ch < newchans; ch++) {
        // stop, mute and deallocate channel
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_PTRX, 0x00000000);
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_CPF,  0x00000000);
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_CCCA, 0);
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_VTFT, 0x0000FFFF);
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_CVCF, 0x0000FFFF);
        emu8k_state.chan[ch].flags |= EMUSTATE_CHAN_INVALID_POS;
    }
    emu8k_dramEnable(emu8k_state.iobase, true, newchans);
    for (int ch = newchans; ch < 32; ch++) {
        emu8k_state.chan[ch].flags |= EMUSTATE_CHAN_INVALID_POS;
    }

    // save new active channels count
    emu8k_state.active_channels = newchans;
}

// get and translate current playing position
uint32_t gusemu_get_current_pos(uint32_t ch) {
    uint32_t pos = emu8k_read(emu8k_state.iobase, ch + EMU8K_REG_CCCA) & 0x00FFFFFF;
    // translate for 8/16 bit samples
    if ((emu8k_state.chan[ch].flags & EMUSTATE_CHAN_16BIT)) {
        pos = (pos - gus_state.mem16start + 1);
        // do that weird GUS 16bit xlat
        pos = ((pos & 0x60000) << 1) | (pos & 0x1FFFF);
    } else {
        // no translation required
        pos -= gus_state.mem8start - 1;
    }
    return pos; // handle emu8k offset
};

// process output enable
void gusemu_process_output_enable() {
    if (((gus_state.gf1regs.reset & 2) == 0) || (((gus_state.mixctrl & 2) != 0))) {
        gus_state.emuflags |=  GUSEMU_STATE_MUTED;
        // save current volume and mute channels
        for (int ch = 0 ; ch < emu8k_state.active_channels; ch++) {
            if ((emu8k_state.chan[ch].flags & EMUSTATE_CHAN_INVALID_POS) == 0)
                emu8k_state.chan[ch].volume = emu8k_read(emu8k_state.iobase, ch + EMU8K_REG_CVCF) >> 16;
        }
    } else {
        gus_state.emuflags &= ~GUSEMU_STATE_MUTED;
        // restore current volume
        for (int ch = 0 ; ch < emu8k_state.active_channels; ch++) {
            if ((emu8k_state.chan[ch].flags & EMUSTATE_CHAN_INVALID_POS) == 0)
                emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_CVCF, (emu8k_state.chan[ch].volume << 16) | 0xFFFF);
        }
    }
}

// process timers, called for every emulation timer tick, returns true is timer IRQ is triggered
bool gusemu_process_timers() {
    bool do_irq = false;

    // process timer 1
    if (gus_state.timer.flags & GUSEMU_TIMER_T1_RUNNING) {
        gus_state.timer.t1_pos += gus_state.timer.t1_add;
        if (gus_state.timer.t1_pos & 0x8000) {
            // overflow
            gus_state.timer.flags |= GUSEMU_TIMER_T1_OVERFLOW;
            gus_state.timer.t1_pos &= 0x7FFF;

            // IRQ check
            if (gus_state.gf1regs.timerctrl.h & (1 << 2)) {
                gus_state.timer.flags |= GUSEMU_TIMER_T1_IRQ;
                do_irq = true;
            }
        }
    }

    // process timer 2
    if (gus_state.timer.flags & GUSEMU_TIMER_T2_RUNNING) {
        gus_state.timer.t2_pos += gus_state.timer.t2_add;
        if (gus_state.timer.t2_pos & 0x8000) {
            // overflow
            gus_state.timer.flags |= GUSEMU_TIMER_T2_OVERFLOW;
            gus_state.timer.t2_pos &= 0x7FFF;

            // IRQ check
            if (gus_state.gf1regs.timerctrl.h & (1 << 3)) {
                gus_state.timer.flags |= GUSEMU_TIMER_T2_IRQ;
                do_irq = true;
            }
        }
    }

    return do_irq;
}

// calculate divisor for GUS timer value
// timer_add = pointer to 1.15fx increment for current timer, used to fix timing issues
uint32_t gusemu_calc_timer_div_delta(uint32_t timerfreq, uint32_t gusval, uint16_t *timer_add) {
    // 16.16fx timer divisor
    uint32_t divadd = imuldiv16(timerfreq, gusval, GUSEMU_GUS_TIMER_CLOCK);
    if (timer_add != 0) {
        *timer_add = idiv16(divadd & ~0xFFFF, divadd) >> 1;
    }
    return (divadd >> 16);
}

void gusemu_init_timer_delta() {
    // shortcut
    irq_timer_t *timer = gus_state.timer.dev;

    if ((gus_state.emuflags & GUSEMU_TIMER_MODE_MASK) == GUSEMU_TIMER_INSTANT) {
        // ah fuck it :D make timers overflow instantly
        gus_state.timer.t1_pos = 0x0000;
        gus_state.timer.t2_pos = 0x0000;
        gus_state.timer.t1_add = 0x8000;
        gus_state.timer.t2_add = 0x8000;
        return;
    }

    // timer 1 freq is 12500 hz, timer 2 is 4 times slower
    uint32_t t1div = (256 - gus_state.gf1regs.timer1count.h);
    uint32_t t2div = (256 - gus_state.gf1regs.timer2count.h) << 2;

    switch (gus_state.timer.flags & (GUSEMU_TIMER_T1_RUNNING|GUSEMU_TIMER_T2_RUNNING))  {
        case GUSEMU_TIMER_T1_RUNNING:
            timer->setDivisor(timer, gusemu_calc_timer_div_delta(timer->devinfo.rate, t1div, &gus_state.timer.t1_add));
            gus_state.timer.t2_add = 0x0000;
            break;
        case GUSEMU_TIMER_T2_RUNNING:
            timer->setDivisor(timer, gusemu_calc_timer_div_delta(timer->devinfo.rate, t2div, &gus_state.timer.t2_add));
            gus_state.timer.t1_add = 0x0000;
            break;
        case GUSEMU_TIMER_T1_RUNNING|GUSEMU_TIMER_T2_RUNNING:
            // this one is tricky :) find which timer is faster, and run the slower one via delta accumulator
            if (t1div <= t2div) { // timer1 is faster
                timer->setDivisor(timer, gusemu_calc_timer_div_delta(timer->devinfo.rate, t1div, &gus_state.timer.t1_add));
                gus_state.timer.t2_add = (t1div << 15) / t2div;
            } else { // timer2 is faster
                timer->setDivisor(timer, gusemu_calc_timer_div_delta(timer->devinfo.rate, t2div, &gus_state.timer.t2_add));
                gus_state.timer.t1_add = (t2div << 15) / t1div;
            }
            break;
        default:
            // alright, both timers are inactive
            gus_state.timer.t1_add = gus_state.timer.t2_add = 0;
            break;
    }
}

// update timer state
void gusemu_update_timers(uint32_t flags) {
    if (flags & GUSEMU_TIMER_UPDATE_TIMERDATA) {
        uint8_t timerdata = gus_state.timerdata;

        // parse new timerdata
        if (timerdata & (1 << 7)) {
            // NOTE NOTE NOTE! clears _AdLib Status_ IRQ bits ONLY!
            // to acknowledge GUS timer IRQ, clear IRQ enable bits in GF1 reg 45, then set them again
            gus_state.timer.flags &= ~(GUSEMU_TIMER_T1_OVERFLOW | GUSEMU_TIMER_T2_OVERFLOW);
            goto clear_overflow_target; // IRQ clear bit inhibits other bits!
        }

        if (timerdata & (1 << 1)) {
            // timer 2 start
            gus_state.timer.flags |=  GUSEMU_TIMER_T2_RUNNING;
            flags |= GUSEMU_TIMER_UPDATE_T2COUNT;
        } else {
            gus_state.timer.flags &= ~GUSEMU_TIMER_T2_RUNNING;
        }

        if (timerdata & (1 << 0)) {
            // timer 1 start
            gus_state.timer.flags |= GUSEMU_TIMER_T1_RUNNING;
            flags |= GUSEMU_TIMER_UPDATE_T1COUNT;
        } else {
            gus_state.timer.flags &= ~GUSEMU_TIMER_T1_RUNNING;
        }

        if (timerdata & (1 << 6))
            // timer 1 adlib mask
            gus_state.timer.flags &= ~GUSEMU_TIMER_T1_ADLIB_UNMASKED;
        else
            gus_state.timer.flags |=  GUSEMU_TIMER_T1_ADLIB_UNMASKED;

        if (timerdata & (1 << 5))
            // timer 2 adlib mask
            gus_state.timer.flags &= ~GUSEMU_TIMER_T2_ADLIB_UNMASKED;
        else
            gus_state.timer.flags |=  GUSEMU_TIMER_T2_ADLIB_UNMASKED;

        // init emulation timer
        if (((gus_state.emuflags & GUSEMU_TIMER_MODE_MASK) == GUSEMU_TIMER_IRQ) &&
            (gus_state.timer.dev != 0)) {
            if (gus_state.timer.flags & (GUSEMU_TIMER_T1_RUNNING|GUSEMU_TIMER_T2_RUNNING)) {
                gus_state.timer.dev->start(gus_state.timer.dev);
            } else {
                gus_state.timer.dev->stop (gus_state.timer.dev);
            }
        }
    }

clear_overflow_target:
    if (flags & GUSEMU_TIMER_UPDATE_TIMERCTRL) {
        uint8_t irqctrl = gus_state.gf1regs.timerctrl.h;

        if ((irqctrl & (1 << 2)) == 0)
            // clear Timer 1 IRQ
            gus_state.timer.flags &= ~GUSEMU_TIMER_T1_IRQ;
        if ((irqctrl & (1 << 3)) == 0)
            // clear Timer 2 IRQ
            gus_state.timer.flags &= ~GUSEMU_TIMER_T2_IRQ;
    }

    // initialize timer variables
    if (flags & (GUSEMU_TIMER_UPDATE_T1COUNT|GUSEMU_TIMER_UPDATE_T2COUNT)) {
        gusemu_init_timer_delta();
    }

    // process timers if instant timer emulation mode
    if ((gus_state.emuflags & GUSEMU_TIMER_MODE_MASK) == GUSEMU_TIMER_INSTANT) {
        bool empty_2x6 = (gusemu_update_irq_status() == 0);
        if (empty_2x6 && gusemu_process_timers()) gusemu_send_irq();
    }
}

// get channel control
uint32_t gusemu_get_channel_ctrl(uint32_t ch) {
    uint8_t ctrl = gus_state.gf1regs.chan[ch].ctrl.h & 0x7F;

    if (emu8k_state.chan[ch].flags & EMUSTATE_CHAN_ONESHOT) {
        // test if channel is on loop boundary
        if ((emu8k_read(emu8k_state.iobase, ch + EMU8K_REG_CCCA) & 0x00FFFFFF) >= emu8k_state.chan[ch].loopstart) {
            // mark as stopped
            ctrl |= (1 << 0);

            // check if need to fire an IRQ
            if ((emu8k_state.irqemu.wavemask & (1 << ch)) && (ctrl & (1 << 5))) {
                // set wave IRQ pending
                ctrl |= (1 << 7);
            }
        }
    }

    // writeback
    gusemu_gf1_write(GF1_REG_CHAN_CTRL, ch, ctrl << 8);
    return ctrl;
}

// get volume control
uint32_t gusemu_get_volume_ctrl(uint32_t ch) {
    uint8_t ctrl = gus_state.gf1regs.chan[ch].volctrl.h & 0x7F;
    // TODO: volume ramp emulation
    return ctrl;
}

// validate play/loop positions
bool gusemu_validate_pos(uint32_t pos, uint32_t loopstart, uint32_t loopend, uint32_t pitch, uint32_t flags) {
    bool valid = true;
    if ((flags & EMUSTATE_CHAN_ONESHOT) == 0)
        valid = (valid && (loopstart <= (loopend - 1 - (pitch >> 14))));
    return (valid && (pos < loopend));
}

// write positions to emu8k
void gusemu_write_pos(uint32_t ch, uint32_t pos) {
    emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_CCCA, pos);
}
void gusemu_write_loop_start(uint32_t ch, uint32_t pos) {
    emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_PSST, pos | (emu8k_state.chan[ch].pan << 24));
}
void gusemu_write_loop_end(uint32_t ch, uint32_t pos) {
    // TODO: chorus
    emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_CSL, pos);
}

// alright, now to the very core of emulation
// update channel state
void gusemu_update_channel(uint32_t ch, uint32_t flags) {
    gus_emu8k_state_t::emu8k_chan_t *emuchan = &emu8k_state.chan[ch];
    gf1regs_channel_t *guschan = &gus_state.gf1regs.chan[ch];

    // process panning
    if (flags & GUSEMU_CHAN_UPDATE_PAN) {
        // convert panning from 0..F to 0..FF range
        uint32_t pan = (15 - (guschan->pan.h & 0x0F));
        pan = gus_state.emuflags & GUSEMU_MONO_PANNING ? 0x80 : (pan | (pan << 4));
        emuchan->pan = pan;
    }

    // process volume control
    if (flags & (GUSEMU_CHAN_UPDATE_VOLCTRL|GUSEMU_CHAN_UPDATE_RAMPSTART|GUSEMU_CHAN_UPDATE_RAMPEND)) {
        // test if volume ramps required
        if (((guschan->volctrl.h & 3) == 0) &&
            ((guschan->ramp_rate.h & 0x3F) > 0))    // if increment==0 then ramp is effectively no-op
        {
            // skip them entirely (until ramp emulation implemented)
            if (guschan->volctrl.h & (1 << 6)) {
                // ramp down
                guschan->volume.w = guschan->ramp_start.w;
            } else {
                // ramp up
                guschan->volume.w = guschan->ramp_end.w;
            }
            // request updating volume
            flags |= GUSEMU_CHAN_UPDATE_VOLUME;
            // mark ramp as finished
            guschan->volctrl.h |= 1;
        }
    }

    // update volume
    if (flags & GUSEMU_CHAN_UPDATE_VOLUME) {
        emuchan->volume = gusemu_translate_volume(guschan->volume.w);
    }

    // update channel parameters
    if (flags & GUSEMU_CHAN_UPDATE_CTRL) {
        if (guschan->ctrl.h & (1 << 3)) {
            // looped sample
            emuchan->flags &= ~EMUSTATE_CHAN_ONESHOT;
            flags |= GUSEMU_CHAN_UPDATE_LOOPSTART;
        } else {
            // one-shot sample
            emuchan->flags |=  EMUSTATE_CHAN_ONESHOT;
            flags |= GUSEMU_CHAN_UPDATE_LOOPSTART;
        }
        // update loop start if loop bit changed
        //if ((guschan->ctrl.h ^ guschan->ctrl_r.h) & (1 << 3))
        //    flags |= GUSEMU_CHAN_UPDATE_LOOPSTART;

        if (!(guschan->ctrl.h & 3) && (guschan->ctrl_r.h & 3))
            flags |= GUSEMU_CHAN_UPDATE_START;
        if ((guschan->ctrl.h & 3) && !(guschan->ctrl_r.h & 3))
            flags |= GUSEMU_CHAN_UPDATE_STOP;

        // update positions if 8/16bit changed
        if ((guschan->ctrl.h ^ guschan->ctrl_r.h) & (1 << 2)) {
            if (guschan->ctrl.h & (1 << 2)) {
                // 16bit
                emuchan->flags |=  EMUSTATE_CHAN_16BIT;
            } else {
                // 8bit
                emuchan->flags &= ~EMUSTATE_CHAN_16BIT;
            }
            flags |= GUSEMU_CHAN_UPDATE_LOOPSTART|GUSEMU_CHAN_UPDATE_LOOPEND|GUSEMU_CHAN_UPDATE_POS;
        }

#if 0
        // save play position on play->stop edge, but only if position is valid!
        if ((guschan->ctrl.h & 3) && !(guschan->ctrl_r.h & 3) && !(emuchan->state & EMUSTATE_CHAN_INVALID_POS)) {
            uint32_t guspos = gusemu_get_current_pos(ch);
            guschan->pos_high.w = guspos >> 7;
            guschan->pos_low.w  = guspos << 9;
        }
#endif
    }

    // update pitch
    if (flags & GUSEMU_CHAN_UPDATE_FREQ) {
        uint32_t tempfreq = gusemu_translate_pitch(guschan->freq.w);
        // check if pitch >= 4.0 (emu8k can't play samples higher than 176khz)
        if (tempfreq >= 0x10000) {
            // allow slight overflow, cap to 0xFFFF
            if (tempfreq <= 0x10040) tempfreq = 0xFFFF; else tempfreq = 0;
        }
        emuchan->freq = tempfreq;
    }

    // update loop start for oneshot samples
    if ((emuchan->flags & EMUSTATE_CHAN_ONESHOT) &&
        (flags & (GUSEMU_CHAN_UPDATE_FREQ | GUSEMU_CHAN_UPDATE_LOOPEND))) {
        flags |= GUSEMU_CHAN_UPDATE_LOOPSTART;
    }

    // stop here if channel is above the limit
    // also do not let emulation to tamper with dram-assigned channels
    if (ch >= emu8k_state.active_channels) return;

    // here be dragons. and bugs. you know :)
    // ---------------------
    // process positions
    uint32_t guschanpos   = (((uint32_t)guschan->pos_high.w   << 7) | (guschan->pos_low.w   >> 9)) & 0xFFFFF;
    uint32_t gusloopstart = (((uint32_t)guschan->start_high.w << 7) | (guschan->start_low.w >> 9)) & 0xFFFFF;
    uint32_t gusloopend   = (((uint32_t)guschan->end_high.w   << 7) | (guschan->end_low.w   >> 9)) & 0xFFFFF;
    bool     force_update = (emuchan->flags & EMUSTATE_CHAN_INVALID_POS);
    bool     pos_is_valid = gusemu_validate_pos(guschanpos, gusloopstart, gusloopend, emuchan->freq, emuchan->flags);

    // validate them
    if (pos_is_valid) {
        // fine, update emu8k registers
        if ((force_update) || (flags & GUSEMU_CHAN_UPDATE_LOOPEND)) {
            emuchan->loopend = (gusemu_translate_pos(ch, gusloopend) - 1) & 0x00FFFFFF;
            gusemu_write_loop_end(ch, emuchan->loopend);
        }
        if ((force_update) || (flags & GUSEMU_CHAN_UPDATE_LOOPSTART)) {
            // fix for oneshot (non-looped) samples
            if (emuchan->flags & EMUSTATE_CHAN_ONESHOT) {
                emuchan->loopstart = emuchan->loopend - (1 + (emuchan->freq>>14));
            } else {
                emuchan->loopstart = (gusemu_translate_pos(ch, gusloopstart) - 1) & 0x00FFFFFF;
            }
            gusemu_write_loop_start(ch, emuchan->loopstart);
        }
        if ((force_update) || (flags & GUSEMU_CHAN_UPDATE_POS)) {
            emuchan->pos = (gusemu_translate_pos(ch, guschanpos) - 1) & 0x00FFFFFF;
            gusemu_write_pos(ch, emuchan->pos);
        }
        if ((force_update) || (flags & (GUSEMU_CHAN_UPDATE_FREQ | GUSEMU_CHAN_UPDATE_CTRL))) {
            // TODO: reverb?
            // process channel start/stop
            if (guschan->ctrl.w & 3) {
                // held it stopped
                emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_PTRX, 0 | 0);
            } else {
                emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_PTRX, (emuchan->freq << 16) | 0);
            }
        }
        if ((force_update) || (flags & GUSEMU_CHAN_UPDATE_VOLUME)) {
            if (gus_state.emuflags & GUSEMU_STATE_MUTED) {
                emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_VTFT, 0 | 0xFFFF);
            } else {
                emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_VTFT, (emuchan->volume << 16) | 0xFFFF);
            }
        }
        // reset invalid position flag
        emuchan->flags &= ~EMUSTATE_CHAN_INVALID_POS;
    } else if (!(emuchan->flags & EMUSTATE_CHAN_INVALID_POS)) {
        // invalid position!
#if 0
        // save current position if it's not the cause of illegal pos
        if (!(flags & GUSEMU_CHAN_UPDATE_POS)) {
            uint32_t guspos = gusemu_get_current_pos(ch);
            guschan->pos_high.w =  guspos >> 7;
            guschan->pos_low.w  =  guspos << 9;
        }
#endif
        // then mute and stop channel
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_PTRX, 0);
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_VTFT, 0x0000FFFF);   // leave filter always off
        // mark channel as invalid position
        emuchan->flags |= EMUSTATE_CHAN_INVALID_POS;
    }

}

// -------------------------------

// dispatch state update after write
void gusemu_gf1_write(uint32_t reg, uint32_t ch, uint32_t data) {
    if (reg >= 0x40) switch (reg) {
        // global
        case 0x41: // DRAM DMA Control
            gus_state.gf1regs.dmactrl.w = data & 0xFF00; // + clear DMA TC shadow bit
            // run DMA emulation
            if ((gus_state.emuflags & GUSEMU_EMULATE_DMA) && (gus_state.gf1regs.dmactrl.h & 1))
                gusemu_dma_start();
            break;
        case 0x42: // DMA Start Address
            gus_state.gf1regs.dmaaddr.w = data;
            break;
        case 0x43: // DRAM I/O Address (A[15:0])
            gus_state.gf1regs.dramlow.w = data;
            break;
        case 0x44: // DRAM I/O Address (4'b0, A[19:16])
            gus_state.gf1regs.dramhigh.w = data & 0x0F00;
            break;
        case 0x45: // Timer IRQ Control
            gus_state.gf1regs.timerctrl.w = data & 0x0C00;
            gusemu_update_timers(GUSEMU_TIMER_UPDATE_TIMERCTRL);
            break;
        case 0x46: // Timer 1 Count
            gus_state.gf1regs.timer1count.w = data & 0xFF00;
            gusemu_update_timers(GUSEMU_TIMER_UPDATE_T1COUNT);
            break;
        case 0x47: // Timer 2 Count
            gus_state.gf1regs.timer2count.w = data & 0xFF00;
            gusemu_update_timers(GUSEMU_TIMER_UPDATE_T2COUNT);
            break;
        case 0x48: // Sampling Frequency (dummy)
            gus_state.gf1regs.recfreq.w = data;
            break;
        case 0x49: // Sampling Control   (dummy but emulated, as on Interwave)
            gus_state.gf1regs.recctrl.w = data & 0xBF00;
            break;
        case 0x4C: // Reset
            gus_state.gf1regs.reset_r = gus_state.gf1regs.reset;
            gus_state.gf1regs.reset = data >> 8;
            if ((gus_state.gf1regs.reset & 1) == 0)
                gusemu_reset(false, false);
            else {
                gusemu_process_output_enable();
            }
            break;
        default:
            break;

        return;
    }

    if ((reg <= 0x0D) && (ch < 32)) {
        uint32_t update_flags = 0;
        switch (reg) {
            // local
            case 0x00: // Voice Control
                gus_state.gf1regs.chan[ch].ctrl_r.w = gus_state.gf1regs.chan[ch].ctrl.w;
                gus_state.gf1regs.chan[ch].ctrl.w   = data & 0x7F00;
                update_flags |= GUSEMU_CHAN_UPDATE_CTRL;
                break;
            case 0x01: // Frequency Control
                gus_state.gf1regs.chan[ch].freq.w = data;
                update_flags |= GUSEMU_CHAN_UPDATE_FREQ;
                break;
            case 0x02: // Starting Address HIGH
                gus_state.gf1regs.chan[ch].start_high.w = data;
                update_flags |= GUSEMU_CHAN_UPDATE_LOOPSTART;
                break;
            case 0x03: // Starting Address LOW
                gus_state.gf1regs.chan[ch].start_low.w  = data;
                update_flags |= GUSEMU_CHAN_UPDATE_LOOPSTART;
                break;
            case 0x04: // End Address HIGH
                gus_state.gf1regs.chan[ch].end_high.w = data;
                update_flags |= GUSEMU_CHAN_UPDATE_LOOPEND;
                break;
            case 0x05: // End Address LOW
                gus_state.gf1regs.chan[ch].end_low.w  = data;
                update_flags |= GUSEMU_CHAN_UPDATE_LOOPEND;
                break;
            case 0x06: // Volume Ramp Rate
                gus_state.gf1regs.chan[ch].ramp_rate.w  = data & 0xFF00;
                update_flags |= GUSEMU_CHAN_UPDATE_RAMPRATE;
                break;
            case 0x07: // Volume Start
                gus_state.gf1regs.chan[ch].ramp_start.w  = data & 0xFF00;
                update_flags |= GUSEMU_CHAN_UPDATE_RAMPSTART;
                break;
            case 0x08: // Volume End
                gus_state.gf1regs.chan[ch].ramp_end.w  = data & 0xFF00;
                update_flags |= GUSEMU_CHAN_UPDATE_RAMPEND;
                break;
            case 0x09: // Current Volume
                gus_state.gf1regs.chan[ch].volume.w  = data & 0xFFF0;
                update_flags |= GUSEMU_CHAN_UPDATE_VOLUME;
                break;
            case 0x0A: // Current Address HIGH
                gus_state.gf1regs.chan[ch].pos_high.w  = data;
                update_flags |= GUSEMU_CHAN_UPDATE_POS;
                break;
            case 0x0B: // Current Address LOW
                gus_state.gf1regs.chan[ch].pos_low.w  = data;
                update_flags |= GUSEMU_CHAN_UPDATE_POS;
                break;
            case 0x0C: // Pan Position (4'b0, Pan)
                gus_state.gf1regs.chan[ch].pan.w      = data & 0x0F00;
                update_flags |= GUSEMU_CHAN_UPDATE_PAN;
                break;
            case 0x0D: // Volume Control
                gus_state.gf1regs.chan[ch].volctrl.w = data & 0x7F00;
                update_flags |= GUSEMU_CHAN_UPDATE_VOLCTRL;
                break;

            default:
                // unknown register!
                break;
        }
        // update channel data if requested
        if (update_flags != 0) gusemu_update_channel(ch, update_flags);
    }

    if (reg == 0x0E) { // Active Voices (Voice independent)
        gus_state.gf1regs.active_channels = data >> 8;
        gusemu_update_active_channel_count();
    }
}

// dispatch GF1 read
uint32_t gusemu_gf1_read(uint32_t reg, uint32_t ch) {
    uint32_t data;
    if ((reg >= 0x80) && (ch >= 32)) return 0; // unknown register!

    switch (reg) {
        // global
        case 0x41: // DRAM DMA Control
            // report DRAM TC in bit 6, then acknowledge and clear it
            data = (gus_state.gf1regs.dmactrl.w & 0xBF00) | ((gus_state.gf1regs.dmactrl.l & 0x40) << 8);
            gus_state.gf1regs.dmactrl.l &= ~(1 << 6);
            break;
        case 0x42: // DMA Start Address
            data = gus_state.gf1regs.dmaaddr.w;
            break;
        case 0x49: // Sampling Control
            data = gus_state.gf1regs.recctrl.w;
            break;
        case 0x4C: // reset
            // right after reset (bit0 0->1 pulse) GUS always returns bit 0 only
            // you need to set bits 1-2 again to make them effective
            if ((gus_state.gf1regs.reset_r == 0) && (gus_state.gf1regs.reset & 1))
                data = 1;
            else
                data = gus_state.gf1regs.reset;
            // reset register is duplicated in both halves
            data = (data << 8) | data;
            break;

        // channel
        case 0x80: // voice control
            data = gusemu_get_channel_ctrl(ch) << 8;
            break;
        case 0x8D: // volume control
            data = gusemu_get_volume_ctrl(ch) << 8;
            break;
        case 0x8A: // current pos high
            data = gusemu_get_current_pos(ch) >> 7;
            break;
        case 0x8B: // current pos low
            data = gusemu_get_current_pos(ch) << 9;
            break;
        case 0x8F: // IRQ status
            gusemu_update_gf1_irq_status();
            data = gus_state.gf1regs.irqstatus << 8;
            break;
        case 0x89: // current volume
            // TODO: if volume ramping would be implemented, process this
            // return what we have written before for now
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87:
        case 0x88:
        case 0x8C: // other registers - return cached values
            data = gus_state.gf1regs.chan[ch].raw[reg - 0x80].w;
            break;
        case 0x8E:
            data = gus_state.gf1regs.active_channels << 8;
            break;

        // unknown
        default:
            data = 0;
            break;
    }

    return data & 0xFFFF;
}

// astolfo.png
// port 3x2/3x3 trap
uint32_t __trapcall gusemu_3x2_r8_trap (uint32_t port, uint32_t data, uint32_t flags) {
    return gus_state.pagereg.channel;
};
uint32_t __trapcall gusemu_3x3_r8_trap (uint32_t port, uint32_t data, uint32_t flags) {
    return gus_state.pagereg.reg;
    // TODO: reading 3x3 reflects last IO to 3x2-3x5 (not on Interwave)
};
uint32_t __trapcall gusemu_3x2_w8_trap (uint32_t port, uint32_t data, uint32_t flags) {
    gus_state.pagereg.channel = data;
    return data;
}
uint32_t __trapcall gusemu_3x3_w8_trap (uint32_t port, uint32_t data, uint32_t flags)  {
    gus_state.pagereg.reg = data;
    return data;
}
uint32_t __trapcall gusemu_3x2_r16_trap(uint32_t port, uint32_t data, uint32_t flags) {
    return gus_state.pagereg.w; // is this really works that way?
}
uint32_t __trapcall gusemu_3x2_w16_trap(uint32_t port, uint32_t data, uint32_t flags) {
    // performance shortcut for apps writing both channel and index by word write to 3x2
    // real GUS will not respond by asserting IOCS16 and ISA bus controller will break
    // one word transaction to two byte to ports 3x2 (low) and 3x3 (high)
    // but for performance reasons we'll left it that way :)
    gus_state.pagereg.w = data;
    return data;
}

// port 3x4/3x5 trap
uint32_t __trapcall gusemu_3x4_r8_trap (uint32_t port, uint32_t data, uint32_t flags) {
    return gusemu_gf1_read(gus_state.pagereg.reg, gus_state.pagereg.channel) & 0xFF;
};
uint32_t __trapcall gusemu_3x5_r8_trap (uint32_t port, uint32_t data, uint32_t flags) {
    return gusemu_gf1_read(gus_state.pagereg.reg, gus_state.pagereg.channel) >> 8;
}
uint32_t __trapcall gusemu_3x4_r16_trap(uint32_t port, uint32_t data, uint32_t flags) {
    return gusemu_gf1_read(gus_state.pagereg.reg, gus_state.pagereg.channel) & 0xFFFF;
}

uint32_t __trapcall gusemu_3x4_w8_trap (uint32_t port, uint32_t data, uint32_t flags) {
    // read current register
    uint32_t readdata = gusemu_gf1_read(gus_state.pagereg.reg, gus_state.pagereg.channel);
    readdata = (readdata & 0xFF00) | (data & 0x00FF);
    // execute write and state update
    gusemu_gf1_write(gus_state.pagereg.reg, gus_state.pagereg.channel, readdata);
    return data;
}
uint32_t __trapcall gusemu_3x5_w8_trap (uint32_t port, uint32_t data, uint32_t flags) {
    // DRAM Address shortcut (performance optimization)
    if (gus_state.pagereg.reg == 0x44) {
        gus_state.gf1regs.dramhigh.h = data & 0x0F;
        return data;
    }

    // read current register
    uint32_t readdata = gusemu_gf1_read(gus_state.pagereg.reg, gus_state.pagereg.channel);
    readdata = (readdata & 0x00FF) | (data << 8);
    // execute write and state update
    gusemu_gf1_write(gus_state.pagereg.reg, gus_state.pagereg.channel, readdata);
    return data;
}
uint32_t __trapcall gusemu_3x4_w16_trap(uint32_t port, uint32_t data, uint32_t flags) {
    // DRAM Address shortcut (performance optimization)
    if (gus_state.pagereg.reg == 0x43) {
        gus_state.gf1regs.dramlow.w = data;
        return data;
    }

    // dummy read for updating GF1 state
    gusemu_gf1_read (gus_state.pagereg.reg, gus_state.pagereg.channel);
    gusemu_gf1_write(gus_state.pagereg.reg, gus_state.pagereg.channel, data);
    return data;
}

// port 3x7 (DRAM data) trap
// DRAM access emulated via 8bit sample pool

// DRAM read - read from 8bit sample pool solely
uint32_t __trapcall gusemu_3x7_r8_trap (uint32_t port, uint32_t data, uint32_t flags) {
    uint32_t newreadpos = (gus_state.gf1regs.dramlow.w | (((uint32_t)gus_state.gf1regs.dramhigh.h) << 16));
    if (newreadpos > gus_state.drammask) return 0;

    emu8k_waitForWriteFlush(emu8k_state.iobase); // TODO: flush only if last access was write
    emu8k_waitForReadReady(emu8k_state.iobase);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALR, gus_state.mem8start + newreadpos);
    emu8k_waitForReadReady(emu8k_state.iobase);
    emu8k_read (emu8k_state.iobase, EMU8K_REG_SMALD);
    emu8k_waitForReadReady(emu8k_state.iobase);
    uint32_t rtn = (emu8k_read(emu8k_state.iobase, EMU8K_REG_SMALD) >> 8) & 0xFF;
    return rtn;
}

// DRAM write, 8bit samples only
uint32_t __trapcall gusemu_3x7_w8_trap (uint32_t port, uint32_t data, uint32_t flags) {
    uint32_t newwritepos = (gus_state.gf1regs.dramlow.w | (((uint32_t)gus_state.gf1regs.dramhigh.h) << 16));
    if (newwritepos > gus_state.drammask) return 0;

    // optimize for sequential writes
    if ((gus_state.emuflags & GUSEMU_SLOW_DRAM) || (gus_state.dramwritepos != newwritepos)) {
        gus_state.dramwritepos = newwritepos;
        emu8k_waitForWriteFlush(emu8k_state.iobase);
        emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, gus_state.mem8start + newwritepos);
    }
    // convert to 16 bit
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALD, data << 8);
    gus_state.dramwritepos++;

    return data;
}

// DRAM write, 16bit samples support
uint32_t __trapcall gusemu_3x7_w8_trap_16bit(uint32_t port, uint32_t data, uint32_t flags) {
    uint32_t newwritepos = (gus_state.gf1regs.dramlow.w | (((uint32_t)gus_state.gf1regs.dramhigh.h) << 16));
    if (newwritepos > gus_state.drammask) return 0;

    // write 8bit sample expanded to 16bit
    emu8k_waitForWriteFlush(emu8k_state.iobase);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, gus_state.mem8start + newwritepos);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALD, data << 8);

    // test if 16bit sample
    if ((gus_state.wordlatch_active) && (gus_state.dramwritepos == (newwritepos - 1))) {
        // fill high byte
        gus_state.wordlatch.h = data & 0xFF;
        // write 16bit sample
        emu8k_waitForWriteFlush(emu8k_state.iobase);
        emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, gus_state.mem16start + (newwritepos >> 1));
        emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALD, gus_state.wordlatch.w);
        // reset wordlatch
        gus_state.wordlatch_active = 0;
    } else if ((newwritepos & 1) == 0) {
        // fill word latch
        gus_state.wordlatch.l = data & 0xFF;
        gus_state.wordlatch_active = 1;
    } else {
        gus_state.wordlatch_active = 0;
    }

    // save dram write position
    gus_state.dramwritepos = newwritepos;

    return data;
}

// port 2x0 (mix control register) trap
uint32_t __trapcall gusemu_2x0_w8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    // save data
    gus_state.mixctrl = data;

    // update EMU8K state
    gusemu_process_output_enable();

    return data;
}

// port 2x6 (IRQ status) trap
uint32_t __trapcall gusemu_2x6_r8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    return gusemu_update_irq_status();
}

// port 2x8/2x9 (timer stuff) trap
uint32_t __trapcall gusemu_2x8_r8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    uint32_t timerstatus = 0;
    if ((gus_state.timer.flags & (GUSEMU_TIMER_T1_ADLIB_UNMASKED|GUSEMU_TIMER_T1_OVERFLOW)) == (GUSEMU_TIMER_T1_ADLIB_UNMASKED|GUSEMU_TIMER_T1_OVERFLOW))
        timerstatus |= ((1 << 6) | (1 << 7));
    if ((gus_state.timer.flags & (GUSEMU_TIMER_T2_ADLIB_UNMASKED|GUSEMU_TIMER_T2_OVERFLOW)) == (GUSEMU_TIMER_T2_ADLIB_UNMASKED|GUSEMU_TIMER_T2_OVERFLOW))
        timerstatus |= ((1 << 5) | (1 << 7));
    return timerstatus;
}
uint32_t __trapcall gusemu_2x8_w8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    gus_state.timerindex = data;
    return data;
}
uint32_t __trapcall gusemu_2x9_w8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    if (gus_state.timerindex == 0x04) {
        gus_state.timerdata = data;
        gusemu_update_timers(GUSEMU_TIMER_UPDATE_TIMERDATA);
    } else {
        gus_state.timerlatch = data;
    }
    return data;
}
uint32_t __trapcall gusemu_2x9_r8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    return gus_state.timerlatch;
}

// port 2xA (Timer Index readback) trap
uint32_t __trapcall gusemu_2xa_r8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    return gus_state.timerindex;
}

// port 2xB (IRQ/DMA Control) trap
uint32_t __trapcall gusemu_2xb_w8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    switch (gus_state.sel_2xb & 7) {
    case 0: // IRQ/DMA Control
        if (gus_state.mixctrl & 0x40)  {
            // update IRQ
            gus_state.irq_2xb = data;
            switch (gus_state.irq_2xb & 7) {
                case  1: gus_state.irq = 9;  gus_state.intr = 0x71;  break;
                case  2: gus_state.irq = 5;  gus_state.intr = 8 + 5; break;
                case  3: gus_state.irq = 3;  gus_state.intr = 8 + 3; break;
                case  4: gus_state.irq = 7;  gus_state.intr = 8 + 7; break;
#if 0   // TODO: high IRQ support
                case  5: gus_state.irq = 11; gus_state.intr = 0x73; break;
                case  6: gus_state.irq = 12; gus_state.intr = 0x74; break;
                case  7: gus_state.irq = 15; gus_state.intr = 0x7F; break;
#endif
                default: break;
            }
        } else {
            // update DMA
            gus_state.dma_2xb = data;
            switch (gus_state.dma_2xb & 7) {
                case  1: gus_state.dma = 1; break;
                case  2: gus_state.dma = 3; break;
                case  3: gus_state.dma = 5; break;
                case  4: gus_state.dma = 6; break;
                case  5: gus_state.dma = 7; break;
                case  6: gus_state.dma = 0; break;  // interwave only?
                default: break;
            }
        }
        break;
    case 5: // "Write a 0 to clear IRQs on power-up" 
        if (data == 0) gus_state.irqstatus = 0;
        break;
    default: break;
    }

    // TODO: reinit IRQ/DMA virtualization

    return data;
}

// port 2xF (2xB Register Select) trap
uint32_t __trapcall gusemu_2xf_w8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    gus_state.sel_2xb = data;
    return data;
}

// dummy trap - block access to i/o ports (writes do nothing, reads return -1)
uint32_t __trapcall gusemu_dummy_trap(uint32_t port, uint32_t data, uint32_t flags) {
    return 0xFFFFFFFF;
}

// debug break trap - dumps gusemu state upon assertion
// data:
//      00..1F - read channel GF1 state via gusemu_gf1_read (emulate GUS I/O)
//      20..3F - read channel GF1 state directly
//      40     - read GF1 global regs   via gusemu_gf1_read (emulate GUS I/O)
//      41     - read GF1 global regs   directly
//      42     - read IRQ/DMA settings (2xB), 2xB select (2xF) and IRQ status (2x6)
uint32_t __trapcall gusemu_debug_w8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    if (data < 0x20) {
        // read GF1 channel registers
        for (int i = 0; i < 0x10; i++) {
            if ((i & 7) == 0) printf("\r\n");
            printf("%04X ", gusemu_gf1_read(0x80 + i, data & 0x1F));
        }
    } else if (data < 0x40) {
        // dump GF1 channel registers
        for (int i = 0; i < 0x10; i++) {
            if ((i & 7) == 0) printf("\r\n");
            printf("%04X ", gus_state.gf1regs.chan[data - 0x20].raw[i].w);
        }
    } else switch (data) {
        case 0x40:
            // read GF1 local registers
            for (int i = 0x40; i < 0x49; i++) {
                printf("%04X ", gusemu_gf1_read(i, 0));
            }
            break;
        case 0x41:
            // dump GF1 local registers
            for (int i = 0x40; i < 0x49; i++) {
                printf("%04X ", gus_state.gf1regs.global[data - 0x40].w);
            }
            break;
        case 0x42:
            // read IRQ/DMA settings (2xB), 2xB select and IRQ status (2x6)
            printf("%02X %02X %02X %02X\n",
                gus_state.irq_2xb, gus_state.dma_2xb, gus_state.sel_2xb, gus_state.irqstatus);
            break;
        default:
            break;
    }
    printf("\r\n");
    return data;
}
