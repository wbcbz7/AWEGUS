#pragma once
#include "vmm.h"
#include "jlm.h"
#include "inp.h"

// wmm_trapcall calling convention (a hacky one)
#pragma aux __vmm_trapcall parm [edx] [eax] [ecx] [ebx] [ebp] value [eax] modify exact [gs]
#define __vmm_trapcall __declspec(__pragma("__vmm_trapcall"))

// trapcall calling convention
#pragma aux __trapcall parm [edx] [ebx] [ecx] value [eax] modify exact [gs]
#define __trapcall __declspec(__pragma("__trapcall"))

typedef uint32_t __trapcall (*io_trap_proc) (uint32_t port, uint32_t data, uint32_t flags);
typedef uint32_t __vmm_trapcall (*vmm_io_trap_proc) (uint32_t port, uint32_t data, uint32_t flags, uint32_t vmid, Client_Reg_Struc* cr);

// get trap address
void* getIOHandler(uint32_t port, uint32_t type);
#pragma aux getIOHandler parm [edx] [ecx] value [eax] modify exact [gs]

// default trap entrypoint
void IOTrapEntry();

// install traps at iobase
uint32_t iotrap_install(uint32_t iobase);

// remove traps
uint32_t iotrap_uninstall(uint32_t iobase);

