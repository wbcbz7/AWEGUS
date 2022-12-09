#ifndef __FXMATH_H
#define __FXMATH_H
#include <math.h>

#ifndef min
#define min(a, b)      ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b)      ((a) > (b) ? (a) : (b))
#endif

#define sgn(a)         ((a) < (0) ? (-1) : ((a) > (0) ? (1) : (0)))
#define clamp(a, l, h) ((a) > (h) ? (h) : ((a) < (l) ? (l) : (a)))

/*
long abs(long a);
#pragma aux abs = \
    "mov    edx, eax" \
    "sar    edx, 31"  \
    "xor    eax, edx" \
    "sub    eax, edx" \
    parm [eax] value [eax] modify [eax edx]
*/
// fatmap2 ripoff :]

inline long ceilx(long a) {return (a + 0xFFFF) >> 16;}

long imul16(long x, long y);        // (x * y) >> 16
#pragma aux imul16 = \
    " imul  edx        "\
    " shrd  eax,edx,16 "\
    parm [eax] [edx] value [eax]


long imul14(long x, long y);        // (x * y) >> 14
#pragma aux imul14 = \
    " imul  edx        "\
    " shrd  eax,edx,14 "\
    parm [eax] [edx] value [eax]


long idiv16(long x, long y);        // (x << 16) / y
#pragma aux idiv16 = \
    " mov   edx,eax    "\
    " sar   edx,16     "\
    " shl   eax,16     "\
    " idiv  ebx        "\
    parm [eax] [ebx] modify exact [eax edx] value [eax]

long imuldiv(long x, long y, long z);   // (x * y) / z, 64 bit precision
#pragma aux imuldiv = \
    " imul  ebx "     \
    " idiv  ecx "     \
    parm [eax] [ebx] [ecx] modify exact [eax edx] value [eax]
   
#endif   
    