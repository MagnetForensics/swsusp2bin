#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdint.h>
#include <sstream>
#include <map>
#include <vector>

using namespace std;

#include "swsusp.h"

#include "lzo.h"
#include "lzodefs.h"

#define __get_free_page(x) malloc(PAGE_SIZE);
#define free_page(x) free((char *)x);

#define vmalloc malloc
#define vfree free
#define printk printf
#define KERN_ERR "error: "
#define KERN_INFO "info: "

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

/* We need to remember how much compressed data we need to read. */
#define LZO_HEADER	sizeof(size_t)

/* Number of pages/bytes we'll compress at one time. */
#define LZO_UNC_PAGES	32
#define LZO_UNC_SIZE	(LZO_UNC_PAGES * PAGE_SIZE)

/* Number of pages/bytes we need for compressed data (worst case). */
#define LZO_CMP_PAGES	DIV_ROUND_UP(lzo1x_worst_compress(LZO_UNC_SIZE) + \
			             LZO_HEADER, PAGE_SIZE)
#define LZO_CMP_SIZE	(LZO_CMP_PAGES * PAGE_SIZE)