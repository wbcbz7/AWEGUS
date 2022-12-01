#include "iotrap.h"
#include "gusemu.h"

// I/O trap implementation

// trap dispatcher (copied from adlipt and slightly modified)
// well oops, watcom doesn't allow EBP to be used in register arguments
__declspec(naked) void IOTrapEntry() {
    /*
    * eax: data
    * ebx: VM handle
    * ecx: IO type
    * edx: port
    * ebp: Client_Reg_Struct
    * 
    * return: eax: data read
    */
    __asm {
        push    ebp
        mov     ebp, eax

        // get appropriate procedure
        call    getIOHandler            // corrupts edx for no reason
        test    eax, eax
        jz      skip

        // call port handler
        push    ebx
        mov     ebx, ebp
        call    eax
        pop     ebx

        // restore registers
        pop     ebp
        ret

    skip:
        pop     ebp  
        /* VMMJmp Simulate_IO */
        int     0x20
        dd      0x1001D or 0x8000
    }
}

// normalized port addresses
static uint16_t trappedPorts[] = {
    0x302, 0x303, 0x304, 0x305, 0x307, 0x200, 0x206, 0x208, 0x209, 0x20B, 0xFFFF
};

// big dispatch table
// index: {word, output, A[8], A[3:0]}
static uint32_t __trapcall (*dispatchTable[])(uint32_t, uint32_t, uint32_t) = {
    // -------------------------
    // byte read
    // 2x0 - 2xF
    0, 0, 0, 0, 0, 0, &gusemu_2x6_r8_trap, 0,
    &gusemu_2x8_r8_trap, &gusemu_2x9_r8_trap, &gusemu_2xa_r8_trap, 0, 0, 0, 0, 0,
    // 3x0 - 3xF
    0, 0, &gusemu_3x2_r8_trap, &gusemu_3x3_r8_trap, &gusemu_3x4_r8_trap, &gusemu_3x5_r8_trap, 0, &gusemu_3x7_r8_trap,
    0,0,0,0,0,0,0,0,

    // -------------------------
    // byte write
    // 2x0 - 2xF
    &gusemu_2x0_w8_trap, 0, 0, 0, 0, 0, &gusemu_debug_w8_trap, 0,
    &gusemu_2x8_w8_trap, &gusemu_2x9_w8_trap, 0, &gusemu_2xb_w8_trap, 0, 0, 0, 0,
    // 3x0 - 3xF
    0, 0, &gusemu_3x2_w8_trap, &gusemu_3x3_w8_trap, &gusemu_3x4_w8_trap, &gusemu_3x5_w8_trap, 0, &gusemu_3x7_w8_trap,
    0,0,0,0,0,0,0,0,

    // -------------------------
    // word read
    // 2x0 - 2xF
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // 3x0 - 3xF
    0, 0, &gusemu_3x2_r16_trap, 0, &gusemu_3x4_r16_trap, 0, 0, 0,
    0,0,0,0,0,0,0,0,

    // -------------------------
    // word write
    // 2x0 - 2xF
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // 3x0 - 3xF
    0, 0, &gusemu_3x2_w16_trap, 0, &gusemu_3x4_w16_trap, 0, 0, 0,
    0,0,0,0,0,0,0,0,
};

// retrieve I/O handler by port number
void* getIOHandler(uint32_t port, uint32_t type) {
    // request VMM to reduce port IO to simplier instructions in case of dword/string I/O
    //if ((type & ~(OUTPUT | WORD_IO)) != 0) return 0;

    // get index from table
    // we assume that iotrap_install() installed handlers only for ports which we defined in trappedPorts
    uint32_t idx = (port - gus_state.iobase);
    idx = (idx & 0xF) | ((idx & 0x100) >> 4) | ((type & (OUTPUT | WORD_IO)) << 3);

    return dispatchTable[idx];
}

uint32_t iotrap_install(uint32_t iobase) {
    for (int i = 0; trappedPorts[i] != 0xFFFF; i++) {
        if (Install_IO_Handler(iobase + trappedPorts[i] - 0x200, &IOTrapEntry) != 0) goto _fail;
    }
    return 1; // success

_fail:
    iotrap_uninstall(iobase);
    return 0; // failure
}

uint32_t iotrap_uninstall(uint32_t iobase) {
    for (int i = 0; trappedPorts[i] != 0xFFFF; i++) {
        Remove_IO_Handler(iobase + trappedPorts[i] - 0x200);
    }
    return 1; // success here :)
}
