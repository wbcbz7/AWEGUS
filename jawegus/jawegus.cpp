#include <stddef.h>
#include <stdint.h>
#include "jlm.h"
#include "inp.h"
#include "console.h"
#include "utils.h"
#include "iotrap.h"
#include "gusemu.h"
#include "emu8000.h"

extern "C" struct vxd_desc_block ddb = {
    0,  /* Next */
    0,  /* Version */
    UNDEFINED_DEVICE_ID,  /* Req_Device_Number */
    0,  /* Dev_Major_Version */
    1,  /* Dev_Minor_Version */
    0,  /* Flags */
    "JAWEGUS",  /* Name */
    UNDEFINED_INIT_ORDER,  /* Init_Order */
    0,  /* Control_Proc */
    0,  /* V86_API_Proc */
    0,  /* PM_API_Proc */
    0,  /* V86_API_CSIP */
    0,  /* PM_API_CSIP */
    0,  /* Reference_Data */
    0,  /* Service_Table_Ptr */
    0,  /* Service_Table_Size */
    0,  /* Win32_Service_Table */
    0,  /* Prev */
    sizeof(ddb),  /* Size */
};

// gusemu init structure
gusemu_init_t init_data;

// --------------
// scan environment
bool get_environment_values() {
    char temp[128];
    const char *envblock = getenvblock();

    // scan for BLASTER variable
    const char *blasterEnv = getenv("BLASTER", envblock);
    if (blasterEnv == 0) {
        printf("error: BLASTER variable is empty or not set!\r\n");
        return false;
    }
    tiny_strncpy(temp, blasterEnv, sizeof(temp));

    // tokenize
    char *now = temp, *next = 0;
    while ((now = tiny_strtok(now, " ", &next)) != NULL) {
        switch(tiny_toupper(*now)) {
            case 'A': init_data.sbbase  = tiny_strtol(now+1, 0, 16); break;
            case 'E': init_data.emubase = tiny_strtol(now+1, 0, 16); break;
            case 'I': init_data.sbirq   = tiny_strtol(now+1, 0, 10); break;
            case 'D': init_data.sbdma   = tiny_strtol(now+1, 0, 10); break;
            default:  break;
        }
        now = next;
    }

    // scan for ULTRASND variable then
    // ULTRASND=<base>,<play_dma>,<rec_dma>,<gus_irq>,<midi_irq>
    const char *ultrasndEnv = getenv("ULTRASND", envblock);
    if (ultrasndEnv == 0) {
        printf("error: ULTRASND variable is empty or not set!\r\n");
        return false;
    }
    tiny_strncpy(temp, ultrasndEnv, sizeof(temp));

    // and tokenize it lol
    now = temp, *next = 0; uint32_t idx = 0;
    while ((now = tiny_strtok(now, ",", &next)) != NULL) {
        switch(idx) {
            case 0: init_data.gusbase = tiny_strtol(now, 0, 16); break;
            case 1: init_data.gusdma  = tiny_strtol(now, 0, 10); break;
            case 3: init_data.gusirq  = tiny_strtol(now, 0, 10); break;
            default:  break;
        }
        now = next; idx++;
    }

    return true;
}

int install(char *cmdline) {
    // init structure
    tiny_memset(&init_data, 0, sizeof(init_data));

    // get environment variables
    if (get_environment_values() == false) return 0;

    // TODO: parse command line!

    // probe for EMU8000
    if (emu8k_probe(init_data.emubase) == 0) {
        puts("error: EMU8000 not found!\r\n");
        return 0;
    }
    printf("EMU8000 found at base %X, ", init_data.emubase);

    // check memory amount
    init_data.memsize = emu8k_getMemSize(init_data.emubase);
    printf("DRAM size: %d KB\r\n", init_data.memsize >> 9);
    if (init_data.memsize < 768*1024) {  // safe margin
        puts("error: you must have at least 2048 K of sound DRAM installed to run AWEGUS\r\n");
        return 0;
    }

    // calculate effective memory size
    init_data.memsize = init_data.memsize;        // TODO: 16bit support fixup

    // init emulation
    if (gusemu_init(&init_data) == 0) {
        puts("error: unable to initialize GUS emulation\r\n");
        return 0;
    } else printf("GUS emulation at port %X installed\r\n", init_data.gusbase);

    return 1;
}

int uninstall() {
    // deinit GUS emulation
    gusemu_deinit();

    // done
    printf("done, now run AWEUTIL /S or UNISOUND to reinit EMU8000 properly :)\r\n");
    return 1;
}

int __stdcall DllMain(int module, int reason, struct jlcomm *jlcomm) {
    puts(
        "AWEGUS - Gravis Ultrasound emulator for AWE32/64, Jemm-based, v.0.1\r\n"
        "by wbcbz7 o3.12.2o22\r\n"
    );
    if (reason == DLL_PROCESS_ATTACH) {
        return install(jlcomm->cmd_line);
    }
    if (reason == DLL_PROCESS_DETACH) {
        return uninstall();
    }
    return 0;
}