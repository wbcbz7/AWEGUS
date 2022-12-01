#include "utils.h"

// a couple fo string functions


// memcpy, with count == 0 guard
__declspec(naked) void jlm_memcpy(void *dst, void *src, uint32_t count) {
    _asm {
            jcxz    _end
            mov     eax, ecx
            shr     ecx, 2
            rep     movsd
            mov     ecx, eax
            and     ecx, not 3
            jz      _end
            rep     movsb
        _end:
            ret
    }
}

// memset
__declspec(naked) void jlm_memset(void *dst, uint32_t data, uint32_t count) {
    _asm {
            jcxz    _end
            mov     edx, ecx
            shr     ecx, 2
            rep     stosd
            mov     ecx, edx
            and     ecx, not 3
            jz      _end
            rep     stosb
        _end:
            ret
    }
}

// memset, 32bit
__declspec(naked) void jlm_memsetd(void *dst, uint32_t data, uint32_t count) {
    _asm {
            shr     ecx, 2
            jcxz    _end
            rep     stosd
        _end:
            ret
    }
}

// get environment block (list of ASCIIZ strings)
const char* getenvblock() {
    Client_Reg_Struc *pcl = jlm_BeginNestedExec();
    pcl->Client_EAX = 0x6200;
    pcl->Client_EBX = 0;            // clear 16 hi order bits
    Exec_Int(pcl, 0x21);
    End_Nest_Exec(pcl);
    return (const char*)(*(uint16_t*)((pcl->Client_EBX << 4) + 0x2C) << 4);
}

// get environment string, case-insensitive
const char* getenv(const char* key, const char *env) {
    if (env == 0) env = getenvblock();
    
    // scan strings
    const char *value = key;
    while (*env != '\0') {
        value = key;
        while ((*value != 0) && (*value++ == *env++)); 
        if (*env == '=') return (env + 1); // found!
        // else scan thru end of string
        while (*env++ != '\0');
    }
    return NULL;
}

// reserve memory block
void *jlm_memreserve(uint32_t pages) {
    return (void*)_PageReserve(PR_SYSTEM, pages, 0);
}

// allocate memory block, page granular
void* jlm_pmalloc(uint32_t pages, bool writeable) {
    void* linaddr = jlm_memreserve(pages); if (linaddr == NULL) return NULL;
    if (_PageCommit((uint32_t)linaddr >> 12, pages, PD_FIXED, 0, PC_FIXED | (writeable ? PC_WRITEABLE : 0)) != 0) return NULL;
    return (void*)linaddr;
}

// free memory block (NOTE: ALWAYS provide number of pages commited, or else leak will occur!)
void jlm_pfree(void* addr, uint32_t pages) {
    _PageDecommit((uint32_t)addr >> 12, pages, 0);
    _PageFree((void*)addr, 0);
}

