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

int install(char *cmdline) {
    // init structure
    gusemu_init_t init; jlm_memset(&init, 0, sizeof(init));
    init.emubase    = 0x620;
    init.gusbase    = 0x240;
    init.gusirq     = 3;
    init.gusdma     = 3;
    init.emuflags   = 0;

    // TODO: parse command line!

    // probe for EMU8000
    if (emu8k_probe(0x620) == 0) {
        puts("error: EMU8000 not found!\r\n");
        //return 0;
    }
    printf("EMU8000 found at base %X, ", 0x620);

    // check memory amount
    init.memsize = emu8k_getMemSize(0x620);
    printf("DRAM size: %d KB\r\n", init.memsize >> 9);
    if (init.memsize < 768*1024) {  // safe margin
        puts("error: you must have at least 2048 KB DRAM installed to run JAWEGUS\r\n");
        //return 0;
    }

    // calculate effective memory size
    init.memsize = init.memsize;        // TODO: 16bit support fixup

    // init emulation
    if (gusemu_init(&init) == 0) {
        puts("error: unable to initialize GUS emulation\r\n");
        return 0;
    }

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
        "Jemm's based Gravis Ultrasound \"emulator\" for Sound Blaster AWE32/64\r\n"
        "by wbcbz7 22.11.2o22\r\n"
    );
    if (reason == DLL_PROCESS_ATTACH) {
        return install(jlcomm->cmd_line);
    }
    if (reason == DLL_PROCESS_DETACH) {
        return uninstall();
    }
    return 0;
}