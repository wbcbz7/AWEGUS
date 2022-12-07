#pragma once
/*
    x86 protected mode lowlevel stuff
    --wbcbz7 14.11.2o22
*/

#include <stdint.h>

// protected mode descriptor struct
struct pm_descriptor {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char  base_mid;

    // access byte
    unsigned char  accessed : 1;   // set by CPU
    unsigned char  rw : 1;         // read enable bit for code, write enable bit for data
    unsigned char  dc : 1;         // direction (set if grows down) bit for data, "conforming" bit for code (read the docs :)
    unsigned char  code : 1;       // set if code, clear if data
    unsigned char  type : 1;       // set for code/data, clear for special segments
    unsigned char  dpl : 2;        // privilegie level
    unsigned char  present : 1;    // set if present
    unsigned char  limit_high : 4;

    unsigned char  avl : 1;        // available for software needs
    unsigned char  longseg : 1;    // set if 64bit segment
    unsigned char  size : 1;       // set if 32bit, clear if 16/64bit
    unsigned char  limitgran : 1;  // set if limit in pages (4k units), clear if in bytes

    unsigned char  base_high;
};

enum {
    PM_SPECIAL_32BIT_INTERRUPT_GATE = 14,
    PM_SPECIAL_32BIT_TRAP_GATE      = 15,
};

// call/interrupt/task gate descriptor struct
struct pm_gate_descriptor {
    unsigned short ofs_low;
    unsigned short selector;

    unsigned char  paramcount : 5;      // parameter count
    unsigned char  res : 3;             // reserved
    
    unsigned char  special_type : 4;    // special descriptor type
    unsigned char  type : 1;            // set for code/data, clear for special segments
    unsigned char  dpl : 2;             // privilegie level
    unsigned char  present : 1;         // set if present

    unsigned short ofs_high;
};

// page table entry 
union page_directory_entry {
    struct {
        uint32_t        present : 1;                // set if present
        uint32_t        writable : 1;               // set if writable
        uint32_t        user : 1;                   // set if available at ring 3
        uint32_t        write_through : 1;
        uint32_t        cache_disable : 1;
        uint32_t        accessed : 1;               // set if page has been accessed
        uint32_t        reserved1 : 1;
        uint32_t        page_size : 1;              // set if 4MB
        uint32_t        reserved2 : 1;
        uint32_t        avl : 3;                    // available for software needs
        uint32_t        page_table_ofs : 20;        // PHYSICAL page table offset[31:12]
    };
    uint32_t            ofs;
};

union page_table_entry {
    struct {
        uint32_t        present : 1;                // set if present
        uint32_t        writable : 1;               // set if writable
        uint32_t        user : 1;                   // set if available at ring 3
        uint32_t        write_through : 1;
        uint32_t        cache_disable : 1;
        uint32_t        accessed : 1;               // set if page has been accessed
        uint32_t        dirty : 1;                  // set if page has been written
        uint32_t        reserved2 : 2;
        uint32_t        avl : 3;                    // available for software needs
        uint32_t        memory_ofs : 20;            // PHYSICAL memory offset[31:12]
    };
    uint32_t            ofs;
};

// read segment registers
uint32_t CS();
#pragma aux CS = "mov eax, cs" value [eax]
uint32_t DS();
#pragma aux DS = "mov eax, ds" value [eax]
uint32_t ES();
#pragma aux ES = "mov eax, es" value [eax]
uint32_t SS();
#pragma aux SS = "mov eax, ss" value [eax]
uint32_t FS();
#pragma aux FS = "mov eax, fs" value [eax]
uint32_t GS();
#pragma aux GS = "mov eax, gs" value [eax]
uint32_t EFLAGS();
#pragma aux EFLAGS = "pushfd" "pop eax" value [eax]

// write segment registers (danger!)
void write_DS(uint32_t);
#pragma aux write_DS = "mov ds, eax" parm [eax]
void write_ES(uint32_t);
#pragma aux write_ES = "mov es, eax" parm [eax]
void write_SS(uint32_t);
#pragma aux write_SS = "mov ss, eax" parm [eax]
void write_FS(uint32_t);
#pragma aux write_FS = "mov fs, eax" parm [eax]
void write_GS(uint32_t);
#pragma aux write_GS = "mov gs, eax" parm [eax]

// read control registers
uint32_t CR0();
#pragma aux CR0 = "mov eax, cr0" value [eax]
uint32_t CR2();
#pragma aux CR2 = "mov eax, cr2" value [eax]
uint32_t CR3();
#pragma aux CR3 = "mov eax, cr3" value [eax]
uint32_t CR4();
#pragma aux CR4 = "mov eax, cr4" value [eax]

// write control registers
void write_CR0(uint32_t);
#pragma aux write_CR0 = "mov cr0, eax" parm [eax]
void write_CR2(uint32_t);
#pragma aux write_CR2 = "mov cr2, eax" parm [eax]
void write_CR3(uint32_t);
#pragma aux write_CR3 = "mov cr3, eax" parm [eax]
void write_CR4(uint32_t);
#pragma aux write_CR4 = "mov cr4, eax" parm [eax]

#pragma pack(push, 1)
typedef struct {
    uint16_t    limit;          // size - 1
    void*       base;           // linear pointer to descriptor table
} descriptorTableAddr;
#pragma pack(pop)

// read system descriptor registers
void sgdt(void* addr);
#pragma aux sgdt = "sgdt fword ptr [eax]" parm [eax]
void sidt(void* addr);
#pragma aux sidt = "sidt fword ptr [eax]" parm [eax]
uint32_t sldt();
#pragma aux sldt = "sldt ax" value [eax]
uint32_t str();
#pragma aux str = "str ax" value [eax]

// write system descriptor registers (ABSOLUTE DANGER!)
// fixme: do via self-modifying code
/*
void lgdt(void* addr);
#pragma aux lgdt = "lgdt fword ptr [eax]" parm [eax]
void lidt(void* addr);
#pragma aux lidt = "lidt fword ptr [eax]" parm [eax]
void lldt(uint32_t);
*/
#pragma aux lldt = "lldt ax" parm [eax]
void ltr(uint32_t);
#pragma aux ltr = "ltr ax" parm [eax]

uint64_t rdtsc();
#pragma aux rdtsc = ".586" "rdtsc" value [edx eax]


void __far * idt_get(pm_gate_descriptor *idt, int intnum);
void idt_set(pm_gate_descriptor *idt, int intnum, void __far *handler, int type = 14);
