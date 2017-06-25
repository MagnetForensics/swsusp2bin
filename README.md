# swsusp2bin
swsusp (Software Suspend) is a kernel feature/program which is part of power management framework in the Linux kernel. It's the **default** suspend framework as of kernel 3.8.

To hibernate the system, type the following at a shell prompt as root:
```
systemctl hibernate
```
This command saves the system state on the hard disk drive and powers off the machine. When you turn the machine back on, the system then restores its state from the saved data without having to boot again. Because the system state is saved on the hard disk and not in RAM, the machine does not have to maintain electrical power to the RAM module, but as a consequence, restoring the system from hibernation is significantly slower than restoring it from suspend mode.

# Getting Started
Linux uses the LZO compression algorithm to compress the hibernation file. The function `lzo1x_decompress_safe()` is responsible of uncompressing blocks of 32 pages (4Kb each).
At the beginning of each compressed block a `size_t` variable is stored to encode the total compressed size.

Valid hibernated swap partition are marked as `S1SUSPEND` at the offset `0x0FF6`.

# Usage
## Example
```
PS L:\projects\swsusp2bin\swsusp2bin> & $s2b --in $swapFile --out out.bin
swsusp2bin
analyzing Z:\DFIR\Unalloc_714905_133146083328_136364163072\Unalloc_714905_133146083328_136364163072...
swsusp_header.orig_sig = SWAPSPACE2
swsusp_header.sig = S1SUSPEND
swsusp_header.image = 20f5b
swsusp_header.flags = 4
swsusp_header.flags[SF_PLATFORM_MODE] = FALSE
swsusp_header.flags[SF_NOCOMPRESS_MODE] = FALSE
swsusp_header.flags[SF_CRC32_MODE] = TRUE
swsusp_header.crc32 = 0x9e058672
debug: valid swsusp file.
.................................
debug: total page count = 0x184e0 (388 MB)
debug: total non compressed pages = 0x80 (0 MB)
PS L:\projects\swsusp2bin\swsusp2bin> & $s2b
swsusp2bin
    --in         swapfile
    --out        extracted memory view
    --verbose    display debug messages
    -32          32-bits mode. (default = 64-bits)
PS L:\projects\swsusp2bin\swsusp2bin>
```
## Options
```
swsusp2bin
    --in         swapfile
    --out        extracted memory view
    --verbose    display debug messages
    -32          32-bits mode. (default = 64-bits)
```
### `--in` (mandatory)
This parameter takes the swap partition file extracted from the disk as an input parameter.

### `--out` (optional)
This parameter is the output file is the decompressed hibernation file into a virtual representation of the RAM. This parameter is optional.

### `--verbose`
Enables debug messages.

### `-32`
This parameter forces the utility to read the hibernated swap file.
