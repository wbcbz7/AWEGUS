#include "utils.h"

// round down to power of two
int roundDownPot(uint32_t a) {
    if (a == 0) return 0;
    int c = 0;
    while ((a >>= 1) != 0) c++;
    return (1 << c);
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

