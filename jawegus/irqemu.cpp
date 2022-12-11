#include "irqemu.h"
#include "gusemu.h"

// timer IRQ callback
// only async and reentrant functions allowed!
uint32_t gusemu_timer_irq_callback(Client_Reg_Struc* pcl, void* userPtr) {
    // read current irq status (2x6) register
    // if it's empty then IRQ is cleared (GF1 won't send new IRQ if previous has not been acknowledged!)
    bool empty_2x6 = (gus_state.emuflags & GUSEMU_IRQ_IGNORE_2X6) || (gus_state.irqstatus == 0);
    bool do_irq = false;
    bool irq_sent = false;

    // run timer emulation
    gusemu_process_timers();

    // run DMA terminal count emulation
    // TODO

    // run wave/ramp IRQ emualtion
    // TODO

    // collect IRQ status
    do_irq = (gusemu_update_irq_status() != 0);

    // send IRQ if previous 2x6 is empty, use current client context
    if (empty_2x6 && do_irq) irq_sent = gusemu_send_irq(pcl);

    // if IRQ has been sent, assume client did acknowledged it, else ask timer to ack it manually
    return irq_sent ? 1 : 0;
}

// send IRQ
bool gusemu_send_irq(Client_Reg_Struc* pcl) {
    bool irq_sent = false;

    // check if IRQ is unmasked
    // this means host application can prevent IRQ from being received
    if ((inp(0x21) & (1 << gus_state.irq)) == 0) {
        Client_Reg_Struc *cr = (pcl ?  pcl : Get_Cur_VM_Handle()->CB_Client_Pointer);
        // check client IF flag
        if (cr->Client_EFlags & (1 << 9)) {
            // clear IF flag, emulating hardware IRQ behavior
            cr->Client_EFlags &= ~(1 << 9);
            // execute interrupt
            Begin_Nest_Exec(cr);
            Exec_Int(cr, gus_state.intr);
            irq_sent = true;
            // restore IF flag
            End_Nest_Exec(cr);
            cr->Client_EFlags |= (1 << 9);
        }
    }

    // since JEMM lacks IRQ virtualization, if V86/DPMI task runs with
    // interrupts disabled, there is no possibility to schedule interrupt
    // upon virtual IF being set, emulating 8259A behavior. this means V86
    // task will lose emulated GUS ints during virtual IF being clear :(

    // upd: seems like Jemm always reflects V86 IF to real one, so it should
    // not lose interrupts. needs nore testing though!

    return irq_sent;
}

// query GF1 IRQ status aka reg 0x8F
// returns accumulated wave+ramp IRQ flags for 2x6
uint32_t gusemu_update_gf1_irq_status() {
    uint32_t gf1_irqstatus = 0xE0;
    uint32_t irqstatus_2x6 = 0;

    for (int ch = 0; ch < emu8k_state.active_channels; ch++) {
        if (gus_state.gf1regs.chan[ch].ctrl.h    & (1 << 7)) irqstatus_2x6 |= (1 << 5);
        if (gus_state.gf1regs.chan[ch].volctrl.h & (1 << 7)) irqstatus_2x6 |= (1 << 6);

        if ((gf1_irqstatus == 0xE0) &&
            (gus_state.gf1regs.chan[ch].ctrl.h    & (1 << 7)) &&
            (gus_state.gf1regs.chan[ch].volctrl.h & (1 << 7))) {

            gf1_irqstatus = (ch & 0x1F) | 0xE0;
            if (gus_state.gf1regs.chan[ch].ctrl.h    & (1 << 7)) gf1_irqstatus &= ~(1 << 6);
            if (gus_state.gf1regs.chan[ch].volctrl.h & (1 << 7)) gf1_irqstatus &= ~(1 << 7);

            // clear current channel IRQ status (apparently)
            gus_state.gf1regs.chan[ch].ctrl.h       &= ~(1 << 7);
            gus_state.gf1regs.chan[ch].volctrl.h    &= ~(1 << 7);
        }
    }

    return irqstatus_2x6;
}

// query IRQ status, returns 2x6 content
uint32_t gusemu_update_irq_status() {
    // scan through all channels, collect their IRQ status
    uint32_t irqstatus_2x6 = 0;

#if 0   // wave/ramp IRQs are disabled for now and not reported
    irqstatus_2x6 |= gusemu_update_gf1_irq_status();
#endif

    // more irqs!
    if (gus_state.timer.flags & GUSEMU_TIMER_T1_IRQ) irqstatus_2x6 |= (1 << 2); // timer 1
    if (gus_state.timer.flags & GUSEMU_TIMER_T2_IRQ) irqstatus_2x6 |= (1 << 3); // timer 2
    if ((gus_state.gf1regs.dmactrl.l & (1 << 6)) && (gus_state.gf1regs.dmactrl.h & (1 << 5)))
        irqstatus_2x6 |= (1 << 7); // DMA complete

    // save irq masks
    gus_state.irqstatus         = irqstatus_2x6;
    return irqstatus_2x6;
}
