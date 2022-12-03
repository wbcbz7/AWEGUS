#include <stdint.h>
#include "vmm.h"
#include "jlm.h"
#include "utils.h"
#include "gusemu.h"
#include "console.h"
#include "iotrap.h"
#include "emu8000.h"

// ----------------------------
// INTERNAL STATIC GUSEMU STRUCTURES (beware!)
gus_state_t gus_state;
gus_emu8k_state_t emu8k_state;

// do everything here. really

void gusemu_emu8k_reset() {
    // reset EMU8000
    //emu8k_hwinit(emu8k_state.iobase);
    emu8k_dramEnable(emu8k_state.iobase, true);
    emu8k_initChannels(emu8k_state.iobase, 14);
    emu8k_setFlatEq(emu8k_state.iobase);
    emu8k_disableOutput(emu8k_state.iobase);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALR, EMU8K_DRAM_OFFSET);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, EMU8K_DRAM_OFFSET);
}

// forward declarations
void gusemu_update_active_channel_count();

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
    }

    // stop emulation timer (TODO)
    // stop pending DMA stuff (TODO)
    emu8k_state.irqemu.rampmask = emu8k_state.irqemu.wavemask = emu8k_state.irqemu.rollovermask = 0;
    
    // reset DRAM interface emulation
    gus_state.wordlatch.w = 0;
    gus_state.dramreadpos = gus_state.dramwritepos = 0;
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALR, EMU8K_DRAM_OFFSET);
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, EMU8K_DRAM_OFFSET);

    // NOTE: interwave docs mention that not all registers are actually reset
    // reset general registers to defaults
    gus_state.mixctrl    = 0x03;                // mixctrl, 2x0
    gus_state.timerindex = 0;                   // timer index, 2x8
    gus_state.timerdata  = 0;                   // timer data,  2x9
    gus_state.gf1regs.active_channels = 0xCD;   // 14 active channels

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
        gus_state.gf1regs.chan[ch].volctrl.w    = 0x0100;   // 0D: ramp  control
        */
    }

    // reinit active channels
    gusemu_update_active_channel_count();

    // reset reset register (lol) if requested
    if (touch_reset_reg) gus_state.gf1regs.reset = 0;

    // ok
    return 1;
}

// initilaize emulation
uint32_t gusemu_init(gusemu_init_t *init) {
    // uninit and reset emulation completely
    gusemu_deinit();

    // set emulated resources
    gus_state.emuflags          = init->emuflags;     // TODO: parse!
    gus_state.iobase            = init->gusbase;
    gus_state.irq               = init->gusirq;
    gus_state.intr              = (gus_state.irq >= 8) ? gus_state.irq + 0x70 : gus_state.irq + 8;
    gus_state.dma               = init->gusdma;
    emu8k_state.iobase          = init->emubase;
    gus_state.timer.emu.iobase  = init->timerbase;
    gus_state.timer.emu.irq     = init->timerirq;
    gus_state.timer.emu.dma     = init->timerdma;
    gus_state.timer.emu.intr    = (init->timerirq >= 8) ? init->timerirq + 0x70 : init->timerirq + 8;

    // install port traps
    if (iotrap_install(gus_state.iobase) == 0) {
        puts("error: unable to install I/O port traps!\r\n");
        goto _fail_cleanup;
    }

    // init EMU8000 as following: 14 channels for playback, rest (interleaved) for local memory access
    gusemu_emu8k_reset();

    // TODO: IRQ/DMA virtualization init (JEMM doesn't provide any interfaces so ignore for now)

    // init memory pool for emulation
    gus_state.mem8start = EMU8K_DRAM_OFFSET;
    gus_state.mem8len   = init->memsize;
    if (gus_state.emuflags & GUSEMU_16BIT_SAMPLES) {
        gus_state.mem16start = EMU8K_DRAM_OFFSET + gus_state.mem8len;
        gus_state.mem16len   = init->memsize / 2; // since we're merging 16bit writes
    }

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

    // remove IRQ/DMA virtualization handlers (TODO)

    // do full GUS state reset
    gusemu_reset(true, true);

    return 1;
}

// send IRQ
void gusemu_send_irq() {
    Client_Reg_Struc *cr = jlm_BeginNestedExec();
    Exec_Int(cr, gus_state.intr);
    End_Nest_Exec(cr);
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
    // this one if pretty straightforward
    return ((4096 + (gf1vol & 0xFFF)) << (gf1vol >> 12)) >> 12; // ugh
}

// set and translate position
uint32_t gusemu_translate_pos(uint32_t ch, uint32_t pos) {
    uint32_t emupos;
    
    // translate for 8/16 bit samples
    if ((emu8k_state.chan[ch].flags & EMUSTATE_CHAN_16BIT)) {
        // do that weird GUS 16bit xlat
        emupos = (pos & 0xC0000) | ((pos >> 1) & 0x1FFFF);
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
    uint32_t chans = (gus_state.gf1regs.active_channels & 31) + 1;
    if (chans <= GUSEMU_MAX_EMULATED_CHANNELS) chans = GUSEMU_MAX_EMULATED_CHANNELS;
    emu8k_state.active_channels = chans;

    // reinit emu8k dram interface
    emu8k_dramEnable(emu8k_state.iobase, true, chans);
}

// get and translate current playing position
uint32_t gusemu_get_current_pos(uint32_t ch) {
    uint32_t pos = emu8k_read(emu8k_state.iobase, ch + EMU8K_REG_CCCA) & 0x00FFFFFF;
    // translate for 8/16 bit samples
    if ((emu8k_state.chan[ch].flags & EMUSTATE_CHAN_16BIT)) {
        pos = (pos - gus_state.mem16start);
        // do that weird GUS 16bit xlat
        pos = (pos & 0xC0000) | ((pos << 1) & 0x3FFFF);
    } else {
        // no translation required
        pos -= gus_state.mem8start;
    }
    return pos;
};

// process output enable
void gusemu_process_output_enable() {
    // TODO: mute all channels except 30&31 (to make FM passthru work)
    ((gus_state.gf1regs.reset & 2) == 0) || (((gus_state.mixctrl & 2) != 0)) ?
        emu8k_disableOutput(emu8k_state.iobase) : emu8k_enableOutput(emu8k_state.iobase);
}

// update DRAM pointers (unused?)
void gusemu_update_dram_pointers() {
    // gus_state.gf1regs.dramhigh is 8bit register, and 8bit regs in GF1 are hibyte
    gus_state.dramreadpos   = ((gus_state.gf1regs.dramhigh.h << 16) | (gus_state.gf1regs.dramlow.w)) & 0xFFFFF;
    gus_state.dramwritepos  = gus_state.dramreadpos;
}

// process timers, called for every emulation timer tick
void gusemu_process_timers() {
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

    // trigger IRQ
    if (do_irq) gusemu_send_irq();
}

void gusemu_init_timer_delta() {
    if ((gus_state.emuflags & GUSEMU_TIMER_MODE_MASK) == GUSEMU_TIMER_INSTANT) {
        // ah fuck it :D make timers overflow instantly
        gus_state.timer.t1_pos = 0x0000;
        gus_state.timer.t1_pos = 0x0000;
        gus_state.timer.t1_add = 0x8000;
        gus_state.timer.t2_add = 0x8000;
        return;
    }

    // timer 1 freq is 12600 hz, timer 2 is 4 times slower
    // find faster out of two timers
    uint32_t t1freq = ((12600 << 16) / (256 - gus_state.gf1regs.timer1count.h));
    uint32_t t2freq = ((12600 << 14) / (256 - gus_state.gf1regs.timer2count.h));
}

// query IRQ status
void gusemu_update_irq_status() {
    // scan through all channels, collect their IRQ status
    uint32_t gf1_irqstatus = gus_state.gf1regs.irqstatus;
    uint32_t irqstatus_2x6 = 0;
    for (int ch = 0; ch < 32; ch++) {
        if (gus_state.gf1regs.chan[ch].ctrl.h    & (1 << 7))  irqstatus_2x6 |= (1 << 5);
        if (gus_state.gf1regs.chan[ch].volctrl.h & (1 << 7))  irqstatus_2x6 |= (1 << 6);

        if ((gf1_irqstatus == 0) &&
            (gus_state.gf1regs.chan[ch].ctrl.h    & (1 << 7)) && 
            (gus_state.gf1regs.chan[ch].volctrl.h & (1 << 7))) {

            gf1_irqstatus = (ch & 0x1F) | 0x20;
            if (gus_state.gf1regs.chan[ch].ctrl.h    & (1 << 7)) gf1_irqstatus |= (1 << 6);
            if (gus_state.gf1regs.chan[ch].volctrl.h & (1 << 7)) gf1_irqstatus |= (1 << 7);
        }
    }
    
    // more irqs!
    if (gus_state.timer.flags & GUSEMU_TIMER_T1_IRQ) irqstatus_2x6 |= (1 << 2);
    if (gus_state.timer.flags & GUSEMU_TIMER_T2_IRQ) irqstatus_2x6 |= (1 << 3);
    if (gus_state.gf1regs.dmactrl.h & (1 << 7))      irqstatus_2x6 |= (1 << 7);

    // save irq masks
    gus_state.irqstatus         = irqstatus_2x6;
    gus_state.gf1regs.irqstatus = gf1_irqstatus;
}

// update timer state
void gusemu_update_timers(uint32_t flags) {
    if (flags & GUSEMU_TIMER_UPDATE_TIMERDATA) {
        uint8_t timerdata = gus_state.timerdata;

        // parse new timerdata
        if (timerdata & (1 << 7)) {
            // timer IRQ clear
            gus_state.timer.flags &= ~(GUSEMU_TIMER_T1_IRQ | GUSEMU_TIMER_T2_IRQ | GUSEMU_TIMER_T1_OVERFLOW | GUSEMU_TIMER_T2_OVERFLOW);
            timerdata &= ~(1 << 7); // IRQ clear bit inhibits other bits!
        }

        if (timerdata & (1 << 1)) {
            // timer 2 start
            gus_state.timer.flags |= GUSEMU_TIMER_T2_RUNNING;
            flags |= GUSEMU_TIMER_UPDATE_T2COUNT;
        }

        if (timerdata & (1 << 0)) {
            // timer 1 start
            gus_state.timer.flags |= GUSEMU_TIMER_T1_RUNNING;
            flags |= GUSEMU_TIMER_UPDATE_T1COUNT;
        }

        if (timerdata & (1 << 6))
            // timer 1 mask
            gus_state.timer.flags &= ~GUSEMU_TIMER_T1_RUNNING;
        
        if (timerdata & (1 << 5))
            // timer 2 mask
            gus_state.timer.flags &= ~GUSEMU_TIMER_T2_RUNNING;
    }

    // initialize timer variables
    if (flags & (GUSEMU_TIMER_UPDATE_T1COUNT|GUSEMU_TIMER_UPDATE_T2COUNT))
        gusemu_init_timer_delta();

    // TODO: init emulation timer

    // process timers if instant timer emulation mode
    if ((gus_state.emuflags & GUSEMU_TIMER_MODE_MASK) == GUSEMU_TIMER_INSTANT)
        gusemu_process_timers();
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
bool gusemu_validate_pos(uint32_t pos, uint32_t loopstart, uint32_t loopend, uint32_t pitch) {
    return ((loopstart <= (loopend - 1 - (pitch >> 14))) && (pos <= (loopend - 1 - (pitch >> 14))));
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
// upd: this needs to be refactored, due to emu8k playback quirks
void gusemu_update_channel(uint32_t ch, uint32_t flags) {
    // channels 28-31 are not supported for now and silently ignored
    // also do not let emulation to tamper with dram-assigned channels
    if (ch >= emu8k_state.active_channels) return; 

    gus_emu8k_state_t::emu8k_chan_t *emuchan = &emu8k_state.chan[ch];
    gf1regs_channel_t *guschan = &gus_state.gf1regs.chan[ch];

    // process panning
    if (flags & GUSEMU_CHAN_UPDATE_PAN) {
        // convert panning from 0..F to 0..FF range
        uint32_t pan = (15 - (guschan->pan.h & 0x0F));
        pan = (pan | (pan << 4));
        emuchan->pan = pan;
    }

    // process volume
    // test if volume ramps required
    if ((guschan->volctrl.h & 3) == 0) {
        // skip them entirely (until ramp emulation implemented)
        if (guschan->volctrl.h & (1 << 6)) {
            // ramp down
            guschan->volume.w = guschan->ramp_start.w;
            flags |= GUSEMU_CHAN_UPDATE_VOLUME;
        } else {
            // ramp up
            guschan->volume.w = guschan->ramp_end.w;
            flags |= GUSEMU_CHAN_UPDATE_VOLUME;
        }
        // mark ramp as finished
        guschan->volctrl.h |= 1;
    }
    if (flags & GUSEMU_CHAN_UPDATE_VOLUME) {
        emuchan->volume = gusemu_translate_volume(guschan->volume.w);
    }

    // update channel parameters
    if (flags & GUSEMU_CHAN_UPDATE_CTRL) {
        if (guschan->ctrl.h & (1 << 3)) {
            // looped sample
            emuchan->flags &= ~EMUSTATE_CHAN_ONESHOT;
        } else {
            // one-shot sample
            emuchan->flags |=  EMUSTATE_CHAN_ONESHOT;
        }
        // update loop start if loop bit changed
        if ((guschan->ctrl.h ^ guschan->ctrl_r.h) & (1 << 3))
            flags |= GUSEMU_CHAN_UPDATE_LOOPSTART;
        
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
        
        // update loop start for oneshot samples
        if (emuchan->flags & EMUSTATE_CHAN_ONESHOT) {
            flags |= GUSEMU_CHAN_UPDATE_LOOPSTART;
        }
    }

    // here be dragons. and bugs. you know :)
    // ---------------------
    // process positions 
    uint32_t guschanpos   = (((uint32_t)guschan->pos_high.w   << 7) | (guschan->pos_low.w   >> 9)) & 0xFFFFF;
    uint32_t gusloopstart = (((uint32_t)guschan->start_high.w << 7) | (guschan->start_low.w >> 9)) & 0xFFFFF;
    uint32_t gusloopend   = (((uint32_t)guschan->end_high.w   << 7) | (guschan->end_low.w   >> 9)) & 0xFFFFF;
    bool     force_update = (emuchan->flags & EMUSTATE_CHAN_INVALID_POS);

    // validate them
    if (gusemu_validate_pos(guschanpos, gusloopstart, gusloopend, emuchan->freq)) {
        // fine, update emu8k registers
        if ((force_update) || (flags & GUSEMU_CHAN_UPDATE_LOOPSTART)) {
            // fix for oneshot (non-looped) samples
            if (emuchan->flags & EMUSTATE_CHAN_ONESHOT) {
                emuchan->loopstart = emuchan->loopend - (1 + (emuchan->freq>>14));
            } else {
                emuchan->loopstart = (gusemu_translate_pos(ch, gusloopstart) - 1) & 0x00FFFFFF;
            }
            gusemu_write_loop_start(ch, emuchan->loopstart);
        }
        if ((force_update) || (flags & GUSEMU_CHAN_UPDATE_LOOPEND)) {
            emuchan->loopend = (gusemu_translate_pos(ch, gusloopend) - 1) & 0x00FFFFFF;
            gusemu_write_loop_end(ch, emuchan->loopend);
        }
        if ((force_update) || (flags & GUSEMU_CHAN_UPDATE_POS)) {
            emuchan->pos = (gusemu_translate_pos(ch, guschanpos) - 1) & 0x00FFFFFF;
            gusemu_write_pos(ch, emuchan->pos);
        }
        if ((force_update) || (flags & GUSEMU_CHAN_UPDATE_FREQ)) {
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
            emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_VTFT, (emuchan->volume << 16) | 0xFFFF);
        }
        // reset inavlid position flag
        emuchan->flags &= ~EMUSTATE_CHAN_INVALID_POS;
    } else if (!(emuchan->flags & EMUSTATE_CHAN_INVALID_POS)) {
        // invalid position!
#if 1
        // save current position if it's not the cause of illegal pos
        if (!(flags & GUSEMU_CHAN_UPDATE_POS)) {
            uint32_t guspos = gusemu_get_current_pos(ch);
            guschan->pos_high.w = guspos >> 7;
            guschan->pos_low.w  = guspos << 9;
        }
        // then mute and stop channel
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_PTRX, 0);
        emu8k_write(emu8k_state.iobase, ch + EMU8K_REG_VTFT, 0x0000FFFF);   // leave filter always off
#endif
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
            gus_state.gf1regs.dmactrl.w = data;
            // no DMA emulation for now
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
        case 0x45: // Timer Control
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
                gus_state.gf1regs.chan[ch].volctrl.w = data & 0xFF00;
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
    }
}

// dispatch GF1 read
uint32_t gusemu_gf1_read(uint32_t reg, uint32_t ch) {
    uint32_t data;
    if ((reg >= 0x80) && (ch >= 32)) return 0xFFFF; // unknown register!

    switch (reg) {
        // global
        case 0x41: // DRAM DMA Control
            data = gus_state.gf1regs.dmactrl.w & (0x3F00); // ignore DMA IRQ Pending bit for now
            break;
        case 0x49: // Sampling Control
            data = gus_state.gf1regs.recctrl.w;
            break;
        case 0x4C: // reset
            // right after reset (bit0 0->1 pulse) GUS always return bit 0 only
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
            gus_state.gf1regs.irqstatus = 0;
            gusemu_update_irq_status();
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
            data = 0xFFFF;
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

    // optimize for sequential reads
    //if (gus_state.dramreadpos != newreadpos) {
        gus_state.dramreadpos  = newreadpos;
        emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALR, EMU8K_DRAM_OFFSET + gus_state.dramreadpos);
        emu8k_read (emu8k_state.iobase, EMU8K_REG_SMALD); // flush stale data!
    //}
    uint32_t rtn = (emu8k_read(emu8k_state.iobase, EMU8K_REG_SMALD) >> 8) & 0xFF;
    gus_state.dramreadpos++;
    return rtn;
}

// TODO: emulate 16bit sample pool write
uint32_t __trapcall gusemu_3x7_w8_trap (uint32_t port, uint32_t data, uint32_t flags) {
    uint32_t newwritepos = (gus_state.gf1regs.dramlow.w | (((uint32_t)gus_state.gf1regs.dramhigh.h) << 16));

    // optimize for sequential writes
    //if (gus_state.dramwritepos != newwritepos) {
        gus_state.dramwritepos  = newwritepos;
        emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALW, EMU8K_DRAM_OFFSET + gus_state.dramwritepos);
    //}
    // convert to 16 bit
    emu8k_write(emu8k_state.iobase, EMU8K_REG_SMALD, data << 8);
    gus_state.dramwritepos++;

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
    gusemu_update_irq_status();
    return gus_state.irqstatus;
}

// port 2x8/2x9 (timer stuff) trap
uint32_t __trapcall gusemu_2x8_r8_trap(uint32_t port, uint32_t data, uint32_t flags) {
    uint32_t timerctrl = 0;
    if (gus_state.timer.flags & GUSEMU_TIMER_T1_OVERFLOW) timerctrl |= ((1 << 6) | (1 << 7));
    if (gus_state.timer.flags & GUSEMU_TIMER_T2_OVERFLOW) timerctrl |= ((1 << 5) | (1 << 7));
    return timerctrl;
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
    if (gus_state.mixctrl & 0x40)  {
        // update IRQ
        gus_state.irq_2xb = data;
    } else {
        // update DMA
        gus_state.dma_2xb = data;
    }
    
    // TODO: reinit IRQ/DMA virtualization

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
        default:
            break;
    }
    printf("\r\n");
    return data;
}