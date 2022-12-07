#pragma once
#include <stdint.h>
#include "jlm.h"
#include "console.h"

// "debug" watchpoint using palette :)
#define RASTER(r, g, b) {outp(0x3c8, 0);outp(0x3c9, (r));outp(0x3c9, (g));outp(0x3c9, (b));}

// round down to power of two
int roundDownPot(uint32_t a);

// get environment block (list of ASCIIZ strings)
const char* getenvblock();

// get environment string, case-insensitive
const char* getenv(const char* key, const char *env = NULL);

// reserve memory block
void *jlm_memreserve(uint32_t pages);

// allocate memory block, page granular
void* jlm_pmalloc(uint32_t pages, bool writeable = true);

// free memory block (NOTE: ALWAYS provide number of pages commited, or else leak will occur!)
void jlm_pfree(void* addr, uint32_t pages);

// allocate DOS memory, returns segment!
uint32_t getdosmem(uint32_t paragraphs);

// free DOS memory
uint32_t freedosmem(uint32_t segment);
