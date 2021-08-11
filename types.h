#ifndef __TYPES_H__
#define __TYPES_H__

#define CONFIG_LBDAF

#ifdef CONFIG_LBDAF
typedef uint64_t sector_t;
#else
typedef unsigned long sector_t;
#endif

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

#define PAGE_SIZE 0x1000

#endif
