#pragma once
#include <stdint.h>
#include "jlm.h"
#include "utils.h"
#include "timer.h"
#include "lowlevel.h"

// query IRQ status, returns new 2x6 content
uint32_t gusemu_update_irq_status();

// query GF1 wavetable/ramp IRQ status, return mask for 2x6
uint32_t gusemu_update_gf1_irq_status();

// timer IRQ callback
uint32_t gusemu_timer_irq_callback(Client_Reg_Struc*, void*);

// send virtual IRQ to V86/DPMI task
bool     gusemu_send_irq(Client_Reg_Struc* cr = 0);

