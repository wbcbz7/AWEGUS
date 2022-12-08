#include <stddef.h>
#include <stdint.h>
#include "jlm.h"
#include "inp.h"
#include "console.h"
#include "utils.h"
#include "iotrap.h"
#include "gusemu.h"
#include "emu8000.h"
#include "cmdline.h"

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

// global command line info
struct {
    bool     help;
    uint32_t dramsize;
} cmdflags;

// command line info
cmdline_params_t cmdline_params[] = {
    {'?', CMD_FLAG_BOOL,    "HELP",     &cmdflags.help, 0},
    {'H', CMD_FLAG_BOOL,    "HELP",     &cmdflags.help, 0},
    {'S', CMD_FLAG_BOOL,    "SLOWDRAM", &gusemu_cmdline.slowdram, 0},
    {'M', CMD_FLAG_BOOL,    "MONO",     &gusemu_cmdline.mono, 0},
    {'W', CMD_FLAG_BOOL,    "16BIT",    &gusemu_cmdline.en16bit, 0},
    {0,   CMD_FLAG_INT,     "MEM",      &cmdflags.dramsize, 0},
    {'D', CMD_FLAG_BOOL,    "DMA",      &gusemu_cmdline.dmaemu, 0},
};

// --------------

// show help
void showHelp() {
    puts(
        " usage: JLOAD JAWEGUS.EXE [params...]\r\n"
        " parameters:\r\n"
        "\r\n"
        " -?, -h, --help   - this help\r\n"
        " -w, --16bit      - enable 16bit samples (needs 1.5x more DRAM, slower upload)\r\n"
        " -m, --mono       - force mono panning\r\n"
        " -s, --slowdram   - disable DRAM write position autoincrement\r\n"
        "     --mem=[x]    - limit emulated GUS DRAM to x kbytes\r\n"
        " -i[x], --irq=[x] - enable IRQ emulation, x: \r\n"
        "       0 - use SB16 in dummy playback mode (default)\r\n"
        "       1 - use COM1 at 0x3F8/IRQ4\r\n"
        "       2 - use COM2 at 0x2F8/IRQ3\r\n"
        " -d,  --dma       - enable DMA emulation (HIGHLY EXPERIMENTAL!)\r\n"
        "\r\n"
    );
}

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

    // validate fields
    if (init_data.gusirq >= 8) {
        printf("error: GUS emulated IRQ must be 7 or less!\r\n");
        return false;
    }
    if (init_data.gusdma >= 4) {
        printf("error: GUS emulated DMA must be 3 or less!\r\n");
        return false;
    }

    return true;
}

int install(char *cmdline) {
    // init structure
    tiny_memset(&init_data, 0, sizeof(init_data));
    tiny_memset(&cmdflags, 0, sizeof(cmdflags));
    tiny_memset(&gusemu_cmdline, 0, sizeof(gusemu_cmdline));

    // parse command line
    if (parse_cmdline(cmdline, cmdline_params, sizeof(cmdline_params)/sizeof(cmdline_params[0])) != 0)
        return 0; 

    if (cmdflags.help){
        showHelp();
        return 0;
    }

    // get environment variables
    if (get_environment_values() == false) return 0;

    // probe for EMU8000
    if (emu8k_probe(init_data.emubase) == 0) {
        puts("error: EMU8000 not found!\r\n");
        return 0;
    }
    printf("EMU8000 found at base %X, ", init_data.emubase);

    // check memory amount and determine GUS emulated memory size
    uint32_t memsize = emu8k_getMemSize(init_data.emubase);
    printf("DRAM size: %d KB\r\n", memsize >> 9);
    if (memsize < 768*1024) {  // safe margin
        puts("error: you must have at least 2048 K of sound DRAM installed to run AWEGUS\r\n");
        return 0;
    }
    if ((cmdflags.dramsize != 0) && ((cmdflags.dramsize << 10) <= memsize))
        init_data.memsize = cmdflags.dramsize << 10;
    else 
        init_data.memsize = memsize;
    if (gusemu_cmdline.en16bit) init_data.memsize = (init_data.memsize * 2) / 3;
    if (init_data.memsize >= 1024*1024) init_data.memsize = 1024*1024;
    init_data.memsize = roundDownPot(init_data.memsize);

    if (init_data.memsize < 256*1024) {
        printf("error: insufficient memory to handle GUS emulation!\r\n");
        return 0;
    }

    // set data flags
    if (gusemu_cmdline.en16bit) init_data.emuflags |= GUSEMU_16BIT_SAMPLES;
    if (gusemu_cmdline.mono)    init_data.emuflags |= GUSEMU_MONO_PANNING;
    if (gusemu_cmdline.dmaemu)  init_data.emuflags |= GUSEMU_EMULATE_DMA;

    // init emulation
    if (gusemu_init(&init_data) == 0) {
        puts("error: unable to initialize GUS emulation\r\n");
        return 0;
    } else {
        printf("GUS emulation at port %X installed, %d KB DRAM available\r\n",
                init_data.gusbase, init_data.memsize >> 10);
        if (gusemu_cmdline.dmaemu)
            printf("DMA emulation at DMA %d installed\r\n", init_data.gusdma);
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
        "AWEGUS - Gravis Ultrasound emulator for AWE32/64, Jemm-based, v.0.12\r\n"
        "by wbcbz7 o5.12.2o22\r\n"
    );
    if (reason == DLL_PROCESS_ATTACH) {
        return install(jlcomm->cmd_line);
    }
    if (reason == DLL_PROCESS_DETACH) {
        return uninstall();
    }
    return 0;
}