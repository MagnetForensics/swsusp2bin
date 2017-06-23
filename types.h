#ifndef __TYPES_H__
#define __TYPES_H__

#define CONFIG_LBDAF

#ifdef CONFIG_LBDAF
typedef uint64_t sector_t;
typedef uint64_t blkcnt_t;
#else
typedef unsigned long sector_t;
typedef unsigned long blkcnt_t;
#endif

#define PAGE_SIZE 0x1000

#endif