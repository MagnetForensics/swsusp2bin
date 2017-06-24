#ifndef __SWSUSP_H__
#define __SWSUSP_H__

#include "types.h"
#include "rbtree.h"

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

struct swap_map_page {
    sector_t entries[MAP_PAGE_ENTRIES];
    sector_t next_swap;
};

/**
*	The swap_map_handle structure is used for handling swap in
*	a file-alike way
*/

struct swap_map_handle {
    struct swap_map_page *cur;
    sector_t cur_swap;
    sector_t first_sector;
    unsigned int k;
};

struct swsusp_header {
    char reserved[PAGE_SIZE - 20 - sizeof(sector_t) - sizeof(int) - sizeof(u32)];
    u32	crc32;
    sector_t image;
    unsigned int flags;	/* Flags to pass to the "boot" kernel */
    char	orig_sig[10];
    char	sig[10];
} __declspec(align(1));

/**
*	The following functions are used for tracing the allocated
*	swap pages, so that they can be freed in case of an error.
*/

struct swsusp_extent {
    struct rb_node node;
    unsigned long start;
    unsigned long end;
};

static struct rb_root swsusp_extents = { NULL };

#endif