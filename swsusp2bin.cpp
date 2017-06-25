#include "swsusp2bin.h"

typedef struct _parameters_t {
    char *swapfile;
    char *memfile;
} parameters_t;

void *swsusp_header = NULL;
struct swsusp_header32 *swsusp_header32 = NULL;
struct swsusp_header64 *swsusp_header64 = NULL;

uint64_t last_highmem_page = 0;
bool no_compress_mode = false;

bool verbose_mode = false;

bool mode_32 = false;

void
help() {
    printf("    --in         swapfile\n");
    printf("    --out        extracted memory view\n");
    printf("    --verbose    display debug messages\n");
    printf("    -32          32-bits mode. (default = 64-bits)\n");
}

bool
parse(int argc,
    char **argv,
    parameters_t *out) {

    bool status = false;

    for (int i = 0; i < argc; i++) {
        string keyword = argv[i];
        if ((keyword == "--in") && ((i + 1) < argc)) {
            out->swapfile = argv[++i];
            status = true;
        }
        else if ((keyword == "--out") && ((i + 1) < argc)) {
            out->memfile = argv[++i];
        }
        else if ((keyword == "--verbose")) {
            verbose_mode = true;
        }
        else if ((keyword == "-32")) {
            mode_32 = true;
        }
    }

    return status;
}

int
get_sector_size() {
    return mode_32 ? sizeof(uint32_t) : sizeof(uint64_t);
}

int
get_max_page_entries() {
    return (PAGE_SIZE / get_sector_size()) - 1;
}

uint64_t
get_page_entry_by_index(
    void *table,
    int index
) {
    if (!table) return 0;

    swap_map_page32 *table32 = NULL;
    swap_map_page64 *table64 = NULL;

    if (index >= get_max_page_entries()) return 0;

    table32 = (swap_map_page32 *)table;
    table64 = (swap_map_page64 *)table;

    return mode_32 ? table32->entries[index] : table64->entries[index];
}

uint64_t
get_page_map_next_swap(
    void *table
) {
    if (!table) return 0;
    swap_map_page32 *table32 = NULL;
    swap_map_page64 *table64 = NULL;

    table32 = (swap_map_page32 *)table;
    table64 = (swap_map_page64 *)table;

    return mode_32 ? table32->next_swap : table64->next_swap;
}

bool
get_compress_mode()
{
    if (swsusp_header32) no_compress_mode = !!(swsusp_header32->flags & SF_NOCOMPRESS_MODE);
    else if (swsusp_header64) no_compress_mode = !!(swsusp_header64->flags & SF_NOCOMPRESS_MODE);
    else false;

    return no_compress_mode;
}

void
print_header() {
    if (!swsusp_header32 && !swsusp_header64) return;

    if (!mode_32) {
        char signature[10 + 1] = { 0 };
        memcpy_s(signature, sizeof(signature), swsusp_header64->orig_sig, sizeof(swsusp_header64->orig_sig));
        printf("swsusp_header.orig_sig = %s\n", signature);
        memset(signature, 0, sizeof(signature));
        memcpy_s(signature, sizeof(signature), swsusp_header64->sig, sizeof(swsusp_header64->sig));
        printf("swsusp_header.sig = %s\n", signature);
        printf("swsusp_header.image = %I64x\n", swsusp_header64->image);
        printf("swsusp_header.flags = %x\n", swsusp_header64->flags);
        printf("swsusp_header.flags[SF_PLATFORM_MODE] = %s\n", swsusp_header64->flags & SF_PLATFORM_MODE ? "TRUE" : "FALSE");
        printf("swsusp_header.flags[SF_NOCOMPRESS_MODE] = %s\n", swsusp_header64->flags & SF_NOCOMPRESS_MODE ? "TRUE" : "FALSE");
        printf("swsusp_header.flags[SF_CRC32_MODE] = %s\n", swsusp_header64->flags & SF_CRC32_MODE ? "TRUE" : "FALSE");
        if (swsusp_header64->flags & SF_CRC32_MODE) printf("swsusp_header.crc32 = 0x%x\n", swsusp_header64->crc32);
    }
    else {
        char signature[10 + 1] = { 0 };
        memcpy_s(signature, sizeof(signature), swsusp_header32->orig_sig, sizeof(swsusp_header32->orig_sig));
        printf("swsusp_header.orig_sig = %s\n", signature);
        memset(signature, 0, sizeof(signature));
        memcpy_s(signature, sizeof(signature), swsusp_header32->sig, sizeof(swsusp_header32->sig));
        printf("swsusp_header.sig = %s\n", signature);
        printf("swsusp_header.image = %x\n", swsusp_header32->image);
        printf("swsusp_header.flags = %x\n", swsusp_header32->flags);
        printf("swsusp_header.flags[SF_PLATFORM_MODE] = %s\n", swsusp_header32->flags & SF_PLATFORM_MODE ? "TRUE" : "FALSE");
        printf("swsusp_header.flags[SF_NOCOMPRESS_MODE] = %s\n", swsusp_header32->flags & SF_NOCOMPRESS_MODE ? "TRUE" : "FALSE");
        printf("swsusp_header.flags[SF_CRC32_MODE] = %s\n", swsusp_header32->flags & SF_CRC32_MODE ? "TRUE" : "FALSE");
        if (swsusp_header32->flags & SF_CRC32_MODE) printf("swsusp_header.crc32 = 0x%x\n", swsusp_header32->crc32);
    }
}

bool
is_swapfile_valid()
{
    char signature[10 + 1] = { 0 };
    memset(signature, 0, sizeof(signature));

    if (swsusp_header32) memcpy_s(signature, sizeof(signature), swsusp_header32->sig, sizeof(swsusp_header32->sig));
    else if (swsusp_header64) memcpy_s(signature, sizeof(signature), swsusp_header64->sig, sizeof(swsusp_header64->sig));
    else false;

    if (memcmp(HIBERNATE_SIG, signature, 10) == 0) {
        printf("debug: valid swsusp file.\n");
    }
    else {
        printf("error: invalid swsusp file. Swap header not found!\n");
        return false;
    }

    return true;
}

uint64_t
get_map_table_offset(
)
{
    uint64_t map_table_offset = 0;

    if (swsusp_header32) map_table_offset = swsusp_header32->image * PAGE_SIZE;
    else if (swsusp_header64) map_table_offset = swsusp_header64->image * PAGE_SIZE;

    return map_table_offset;
}

uint64_t
get_last_highmem_page(
    FILE *handle
) {
    swap_map_page *map_page = NULL;
    size_t result = 0;
    uint64_t map_table_offset = 0;

    if (!swsusp_header32 && !swsusp_header64) return 0;

    if (last_highmem_page) return last_highmem_page;

    map_table_offset = get_map_table_offset();
    if (!map_table_offset) goto cleanup;

    map_page = (struct swap_map_page *)malloc(PAGE_SIZE);
    if (map_page == NULL) goto cleanup;

    while (map_table_offset) {
        if (fseek(handle, (long)map_table_offset, SEEK_SET)) {
            printf("%s: error: can't change the offset.\n", __FUNCTION__);
            goto cleanup;
        }

        result = fread(map_page, 1, PAGE_SIZE, handle);
        if (result != PAGE_SIZE) {
            printf("%s: error: can't read page table (result = 0x%x, map_table_offset = 0x%x).\n", __FUNCTION__, result, (long)map_table_offset);
            goto cleanup;
        }

        int i = 0;
        while (get_page_entry_by_index(map_page, i) && (i < MAP_PAGE_ENTRIES)) {
            uint64_t page_offset = get_page_entry_by_index(map_page, i) * PAGE_SIZE;

            if (page_offset > last_highmem_page) last_highmem_page = page_offset;
            i++;
        }

        map_table_offset = map_page->next_swap * PAGE_SIZE;
        if (!map_page->next_swap) break;
    }

cleanup:
    if (map_page) free(map_page);

    return last_highmem_page;
}

bool
read_file_at(
    FILE *file,
    uint64_t offset,
    void *buffer,
    uint32_t buffer_size
)
{
    size_t size = PAGE_SIZE;
    size_t result = 0;
    bool status = false;

    if (buffer_size) size = buffer_size;

    if (fseek(file, (long)offset, SEEK_SET)) {
        printf("error: can't change the offset to 0x%llx.\n", offset);
        goto cleanup;
    }

    result = fread(buffer, 1, size, file);
    if (result != size) {
        printf("error: can't read data at 0x%llx. result = 0x%x\n", offset, result);
        goto cleanup;
    }

    status = true;

cleanup:
    return status;
}

bool
write_file_at(
    FILE *file,
    uint64_t offset,
    void *buffer,
    uint32_t buffer_size
)
{
    size_t size = PAGE_SIZE;
    size_t result = 0;
    bool status = false;

    if (buffer_size) size = buffer_size;

    if (fseek(file, (long)offset, SEEK_SET)) {
        printf("error: can't change the offset to 0x%llx.\n", offset);
        goto cleanup;
    }

    result = fwrite(buffer, 1, size, file);
    if (result != size) {
        printf("error: can't write data at 0x%llx. result = 0x%x\n", offset, result);
        goto cleanup;
    }
    if (verbose_mode) printf("debug: wrote data at 0x%llx.\n", offset);
    status = true;

cleanup:
    return status;
}

bool
readwrite(
    FILE *in,
    FILE *out,
    uint64_t offset,
    void *buffer,
    uint32_t buffer_size
) {
    bool status = false;

    if (!out || !in || !buffer_size || !buffer) return false;

    if (!read_file_at(in, offset, buffer, buffer_size)) {
        printf("error: Can't read memory block.\n");
        goto cleanup;
    }

    if (!write_file_at(out, offset, buffer, buffer_size)) {
        printf("error: Can't write memory block.\n");
        goto cleanup;
    }

    if (verbose_mode) printf("debug: wrote 0x%x bytes at %llx.\n", buffer_size, offset);

    status = true;
cleanup:
    return status;
}

bool
read_swsusp_header(
    FILE *file
)
{
    bool status = false;

    swsusp_header = (void *)malloc(PAGE_SIZE);
    if (!swsusp_header) {
        printf("error: not enough memory..\n");
        goto cleanup;
    }

    if (!read_file_at(file, 0, swsusp_header, PAGE_SIZE)) {
        printf("error: can't read header.\n");
        goto cleanup;
    }

    if (mode_32) swsusp_header32 = (struct swsusp_header32 *)swsusp_header;
    else swsusp_header64 = (struct swsusp_header64 *)swsusp_header;

    status = true;
cleanup:
    return status;
}

int main(
    int argc,
    char **argv)
{
    parameters_t params = { 0 };
    char *buffer = NULL;
    void *map_page = NULL;
    FILE * file = NULL;
    FILE * out = NULL;

    unsigned char *uncompressed_buffer = NULL;
    unsigned char *compressed_buffer = NULL;

    printf("swsusp2bin\n");

    if (!parse(argc, argv, &params)) {
        help();
        goto cleanup;
    }

    printf("analyzing %s...\n", params.swapfile);

    file = fopen(params.swapfile, "rb");
    if (file == NULL) {
        printf("error: can't open file.\n");
        goto cleanup;
    }

    if (params.memfile) {
        out = fopen(params.memfile, "wb");
        if (out == NULL) {
            printf("error: can't create file.\n");
            goto cleanup;
        }
    }

    if (!read_swsusp_header(file)) goto cleanup;

    get_compress_mode();
    print_header();

    if (!is_swapfile_valid()) goto cleanup;

    uint64_t map_table_offset = get_map_table_offset();

    if (verbose_mode) printf("image: 0x%I64x\n", map_table_offset);
    map_page = (void *)malloc(PAGE_SIZE);
    if (map_page == NULL) goto cleanup;

    buffer = (char *)malloc(PAGE_SIZE);
    if (!buffer) goto cleanup;

    if (out) {
        uncompressed_buffer = (unsigned char *)malloc(LZO_UNC_SIZE);
        if (!uncompressed_buffer) goto cleanup;

        compressed_buffer = (unsigned char *)malloc(LZO_UNC_SIZE);
        if (!compressed_buffer) goto cleanup;
    }

    int pages_count = 0;
    int err_pages_count = 0;

    while (map_table_offset) {
        if (!read_file_at(file, map_table_offset, map_page, PAGE_SIZE)) {
            printf("error: Can't read page table (map_table_offset = 0x%x).\n", (long)map_table_offset);
            goto cleanup;
        }

        int i = 0;
        while (get_page_entry_by_index(map_page, i) && (i < get_max_page_entries())) {
            uint64_t map_page_entry = get_page_entry_by_index(map_page, i);
            if (verbose_mode) printf("[0x%llx][%4d] = 0x%llx / 0x%llx.\n", map_table_offset, i, map_page_entry, map_page_entry * PAGE_SIZE);

            uint64_t compressed_size = 0;
            uint64_t page_offset = map_page_entry * PAGE_SIZE;

            if (!no_compress_mode) {
                if (!read_file_at(file, page_offset, &compressed_size, get_sector_size())) {
                    printf("error: Can't read compressed size in the block header.\n");
                    goto cleanup;
                }
                if (verbose_mode) printf("debug: compressed buffer size = 0x%llx\n", compressed_size);

                if (!compressed_size || (compressed_size > lzo1x_worst_compress(LZO_UNC_SIZE))) {
                    if (verbose_mode) printf("error: Invalid LZO compressed length (worse: 0x%x)\n", lzo1x_worst_compress(LZO_UNC_SIZE));
                    err_pages_count++;

                    if (out) {
                        readwrite(file, out, page_offset, buffer, PAGE_SIZE);
                    }

                    i += 1;
                    pages_count++;

                } else {
                    int compressed_size_in_pages = (int)DIV_ROUND_UP(compressed_size, PAGE_SIZE);
                    if (verbose_mode) printf("skip %d pages...\n", compressed_size_in_pages);

                    if (out) {
                        if (!read_file_at(file, page_offset + get_sector_size(), compressed_buffer, (uint32_t)compressed_size)) {
                            printf("error: Can't read compressed block.\n");
                            goto cleanup;
                        }

                        size_t ucmp_len = LZO_UNC_SIZE;
                        memset(uncompressed_buffer, 0, ucmp_len);
                        if (lzo1x_decompress_safe(compressed_buffer, (size_t)compressed_size, uncompressed_buffer, &ucmp_len) != LZO_E_OK) {
                            if (verbose_mode) printf("%s: decompression failed.\n", __FUNCTION__);
                            // page isn't compressed. we force write it.
                            readwrite(file, out, page_offset, buffer, PAGE_SIZE);
                        }
                        else {
                            for (int j = 0; j < compressed_size_in_pages; j += 1) {
                                uint64_t dst_offset = get_page_entry_by_index(map_page, i + j) * PAGE_SIZE;
                                if (!write_file_at(out, dst_offset, uncompressed_buffer + (j + PAGE_SIZE), PAGE_SIZE)) {
                                    printf("error: Can't write uncompressed block.\n");
                                    goto cleanup;
                                }
                            }
                        }
                    }

                    i += compressed_size_in_pages;
                    pages_count += LZO_UNC_PAGES;

                    continue;
                }
            } else {
                if (out) {
                    readwrite(file, out, page_offset, buffer, PAGE_SIZE);
                }

                i += 1;
                pages_count += 1;
            }
        }

        uint64_t next_swap = get_page_map_next_swap(map_page);
        if (verbose_mode) printf("map_page->next_swap = 0x%llx\n", next_swap);
        if (next_swap) {
            if (verbose_mode) printf("gap between the two page map table is %d pages\n", (uint32_t)(next_swap - (map_table_offset / PAGE_SIZE)));
            else printf(".");
        }
        map_table_offset = next_swap * PAGE_SIZE;
    }
    printf("\n");

    printf("debug: total page count = 0x%x (%d MB)\n", pages_count, (pages_count * PAGE_SIZE) / (1024 * 1024));
    printf("debug: total non compressed pages = 0x%x (%d MB)\n", err_pages_count, (err_pages_count * PAGE_SIZE) / (1024 * 1024));

cleanup:
    if (uncompressed_buffer) free(uncompressed_buffer);
    if (compressed_buffer) free(compressed_buffer);
    if (swsusp_header) free(swsusp_header);
    if (map_page) free(map_page);
    if (buffer) free(buffer);
    if (out) fclose(out);
    if (file) fclose(file);

    return true;
}