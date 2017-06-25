#ifndef __SWSUSP_H__
#define __SWSUSP_H__

#include "types.h"

/*
* Flags that can be passed from the hibernatig hernel to the "boot" kernel in
* the image header.
*/
#define SF_PLATFORM_MODE	1
#define SF_NOCOMPRESS_MODE	2
#define SF_CRC32_MODE	    4

#define HIBERNATE_SIG	"S1SUSPEND"

/*
*	The swap map is a data structure used for keeping track of each page
*	written to a swap partition.  It consists of many swap_map_page
*	structures that contain each an array of MAP_PAGE_ENTRIES swap entries.
*	These structures are stored on the swap and linked together with the
*	help of the .next_swap member.
*
*	The swap map is created during suspend.  The swap map pages are
*	allocated and populated one at a time, so we only need one memory
*	page to set up the entire structure.
*
*	During resume we also only need to use one swap_map_page structure
*	at a time.
*/

#define MAP_PAGE_ENTRIES	(PAGE_SIZE / sizeof(sector_t) - 1)
#define MAP_PAGE_ENTRIES32	(PAGE_SIZE / sizeof(uint32_t) - 1)
#define MAP_PAGE_ENTRIES64	(PAGE_SIZE / sizeof(uint64_t) - 1)

struct swap_map_page {
    sector_t entries[MAP_PAGE_ENTRIES];
    sector_t next_swap;
};

struct swap_map_page32 {
    uint32_t entries[MAP_PAGE_ENTRIES32];
    uint32_t next_swap;
};

struct swap_map_page64 {
    uint64_t entries[MAP_PAGE_ENTRIES64];
    uint64_t next_swap;
};

struct swsusp_header32 {
    char reserved[PAGE_SIZE - 20 - sizeof(uint32_t) - sizeof(int) - sizeof(u32)];
    u32	crc32;
    uint32_t image;
    unsigned int flags;	/* Flags to pass to the "boot" kernel */
    char	orig_sig[10];
    char	sig[10];
} __declspec(align(1));

struct swsusp_header64 {
    char reserved[PAGE_SIZE - 20 - sizeof(uint64_t) - sizeof(int) - sizeof(u32)];
    u32	crc32;
    uint64_t image;
    unsigned int flags;	/* Flags to pass to the "boot" kernel */
    char	orig_sig[10];
    char	sig[10];
} __declspec(align(1));

#endif