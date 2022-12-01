#pragma once

// jemm-related mashed potatoes of defines, includes and enums
// --wbcbz7 14.11.2o22

// lots of this stuff ripped straight from Windows 9x DDK

/*
 *  Flags passed in new memory manager functions
 */

/* PageReserve arena values */
#define PR_PRIVATE  0x80000400	/* anywhere in private arena */
#define PR_SHARED   0x80060000	/* anywhere in shared arena */
#define PR_SYSTEM   0x80080000	/* anywhere in system arena */

/* PageReserve flags */
#define PR_FIXED    0x00000008	/* don't move during PageReAllocate */
#define PR_4MEG     0x00000001	/* allocate on 4mb boundary */
#define PR_STATIC   0x00000010	/* see PageReserve documentation */

/* PageCommit default pager handle values */
#define PD_ZEROINIT 0x00000001	/* swappable zero-initialized pages */
#define PD_NOINIT   0x00000002	/* swappable uninitialized pages */
#define PD_FIXEDZERO	0x00000003  /* fixed zero-initialized pages */
#define PD_FIXED    0x00000004	/* fixed uninitialized pages */

/* PageCommit flags */
#define PC_FIXED    0x00000008	/* pages are permanently locked */
#define PC_LOCKED   0x00000080	/* pages are made present and locked*/
#define PC_LOCKEDIFDP	0x00000100  /* pages are locked if swap via DOS */
#define PC_WRITEABLE	0x00020000  /* make the pages writeable */
#define PC_USER     0x00040000	/* make the pages ring 3 accessible */
#define PC_INCR     0x40000000	/* increment "pagerdata" each page */
#define PC_PRESENT  0x80000000	/* make pages initially present */
#define PC_STATIC   0x20000000	/* allow commit in PR_STATIC object */
#define PC_DIRTY    0x08000000	/* make pages initially dirty */
#define PC_CACHEDIS 0x00100000  /* Allocate uncached pages - new for WDM */
#define PC_CACHEWT  0x00080000  /* Allocate write through cache pages - new for WDM */
#define PC_PAGEFLUSH 0x00008000 /* Touch device mapped pages on alloc - new for WDM */

/******************************************************************************
 *  The following are the definitions for the "type of I/O" parameter passed
 *  to a I/O trap routine.
 *****************************************************************************/

#define BYTE_INPUT      0x000
#define BYTE_OUTPUT     0x004
#define WORD_INPUT      0x008
#define WORD_OUTPUT     0x00C
#define DWORD_INPUT     0x010
#define DWORD_OUTPUT    0x014

#define OUTPUT_BIT      2
#define OUTPUT          (1 << OUTPUT_BIT)
#define WORD_IO_BIT     3
#define WORD_IO         (1 << WORD_IO_BIT)
#define DWORD_IO_BIT    4
#define DWORD_IO        (1 << DWORD_IO_BIT)

#define STRING_IO_BIT   5
#define STRING_IO       (1 << STRING_IO_BIT)
#define REP_IO_BIT      6
#define REP_IO	        (1 << REP_IO_BIT)
#define ADDR_32_IO_BIT  7
#define ADDR_32_IO      (1 << ADDR_32_IO_BIT)
#define REVERSE_IO_BIT  8
#define REVERSE_IO      (1 << REVERSE_IO_BIT)

#define IO_SEG_MASK 0x0FFFF0000     /* Use this to get segment */
#define IO_SEG_SHIFT	0x10		/* Must shift right this many */
