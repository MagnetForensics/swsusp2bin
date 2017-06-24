#include "swsusp2bin.h"

typedef struct _parameters_t {
    char *swapfile;
    char *memfile;
} parameters_t;

struct swsusp_header *swsusp_header;
uint64_t last_highmem_page = 0;
bool no_compresse_mode = false;

bool verbose_mode = true;

bool mode_32 = false;

void
help() {
    printf("    --in         swapfile\n");
    printf("    --out        extracted memory view\n");
    printf("    --verbose    display debug messages\n");
    printf("    -32          32-bits mode. (default = 64-bits)\n");
}

int
get_sector_size() {
    return mode_32 ? sizeof(uint32_t) : sizeof(uint64_t);
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

void
print_header() {
    if (!swsusp_header) return;

    char signature[10 + 1] = { 0 };
    memcpy_s(signature, sizeof(signature), swsusp_header->orig_sig, sizeof(swsusp_header->orig_sig));
    printf("swsusp_header.orig_sig = %s\n", signature);
    memset(signature, 0, sizeof(signature));
    memcpy_s(signature, sizeof(signature), swsusp_header->sig, sizeof(swsusp_header->sig));
    printf("swsusp_header.sig = %s\n", signature);
    printf("swsusp_header.image = %I64x\n", swsusp_header->image);
    printf("swsusp_header.flags = %x\n", swsusp_header->flags);
    printf("swsusp_header.flags[SF_PLATFORM_MODE] = %s\n", swsusp_header->flags & SF_PLATFORM_MODE ? "TRUE" : "FALSE");
    printf("swsusp_header.flags[SF_NOCOMPRESS_MODE] = %s\n", swsusp_header->flags & SF_NOCOMPRESS_MODE ? "TRUE" : "FALSE"); 
    printf("swsusp_header.flags[SF_CRC32_MODE] = %s\n", swsusp_header->flags & SF_CRC32_MODE ? "TRUE" : "FALSE");
    if (swsusp_header->flags & SF_CRC32_MODE) printf("swsusp_header.crc32 = %x\n", swsusp_header->crc32);
}

bool
is_swapfile_valid() {

    if (memcmp(HIBERNATE_SIG, swsusp_header->sig, 10) == 0) {
        printf("debug: valid swsusp file.\n");
    }
    else {
        printf("error: invalid swsusp file. Swap header not found!\n");
        return false;
    }

    return true;
}

uint64_t
get_last_highmem_page(
    FILE *handle
) {
    swap_map_page *map_page = NULL;
    size_t result = 0;

    if (last_highmem_page) return last_highmem_page;

    if (!swsusp_header) return 0;

    uint64_t map_table_offset = swsusp_header->image * PAGE_SIZE;
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
        while (map_page->entries[i] && (i < MAP_PAGE_ENTRIES)) {
            uint64_t page_offset = map_page->entries[i] * PAGE_SIZE;

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

int main(
    int argc,
    char **argv)
{
    parameters_t params = { 0 };
    char *buffer = NULL;
    swap_map_page *map_page = NULL;
    FILE * file = NULL;
    FILE * out = NULL;
    size_t result;

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

    swsusp_header = (struct swsusp_header *)malloc(PAGE_SIZE);
    if (!swsusp_header) {
        printf("error: not enough memory..\n");
        goto cleanup;
    }

    result = fread(swsusp_header, 1, PAGE_SIZE, file);
    if (result != PAGE_SIZE) {
        printf("error: can't read header.\n");
        goto cleanup;
    }

    no_compresse_mode = !!(swsusp_header->flags & SF_NOCOMPRESS_MODE);

    print_header();
    if (!is_swapfile_valid()) goto cleanup;

    uint64_t map_table_offset = swsusp_header->image * PAGE_SIZE;

    if (verbose_mode) printf("image: 0x%I64x\n", map_table_offset);
    map_page = (struct swap_map_page *)malloc(PAGE_SIZE);
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
        if (fseek(file, (long)map_table_offset, SEEK_SET)) {
            printf("error: can't change the offset.\n");
            goto cleanup;
        }

        result = fread(map_page, 1, PAGE_SIZE, file);
        if (result != PAGE_SIZE) {
            printf("error: can't read page table (result = 0x%x, map_table_offset = 0x%x).\n", result, (long)map_table_offset);
            goto cleanup;
        }

        int i = 0;
        while (map_page->entries[i] && (i < MAP_PAGE_ENTRIES)) {
            if (verbose_mode) printf("[0x%llx][%4d] = 0x%llx / 0x%llx.\n", map_table_offset, i, map_page->entries[i], map_page->entries[i] * PAGE_SIZE);

            uint64_t compressed_size = 0;
            uint64_t page_offset = map_page->entries[i] * PAGE_SIZE;

            if (!no_compresse_mode) {
                fseek(file, (long)page_offset, SEEK_SET);
                fread(&compressed_size, 1, sizeof(compressed_size), file);

                if (verbose_mode) printf("debug: compressed buffer size = 0x%llx\n", compressed_size);

                if (!compressed_size || (compressed_size > lzo1x_worst_compress(LZO_UNC_SIZE))) {
                    if (verbose_mode) printf("error: Invalid LZO compressed length (worse: 0x%x)\n", lzo1x_worst_compress(LZO_UNC_SIZE));
                    err_pages_count++;
                    if (out) {
                        fseek(file, (long)page_offset, SEEK_SET);
                        fread(buffer, 1, PAGE_SIZE, file);
                        fseek(out, (long)page_offset, SEEK_SET);
                        result = fwrite(buffer, 1, PAGE_SIZE, out);
                        if (verbose_mode) printf("debug: wrote 0x%x bytes at %llx.\n", result, page_offset);
                    }

                    i += 1;
                    pages_count++;

                } else {
                    int count = (int)DIV_ROUND_UP(compressed_size, PAGE_SIZE);
                    if (verbose_mode) printf("skip %d pages...\n", count);

                    if (out) {
                        fseek(file, (long)page_offset + sizeof(compressed_size), SEEK_SET);
                        fread(compressed_buffer, 1, (size_t)compressed_size, file);

                        size_t ucmp_len = LZO_UNC_SIZE;
                        if (lzo1x_decompress_safe(compressed_buffer, (size_t)compressed_size, uncompressed_buffer, &ucmp_len) != LZO_E_OK) {
                            if (verbose_mode) printf("%s: decompression failed.\n", __FUNCTION__);

                            fseek(file, (long)page_offset, SEEK_SET);
                            fread(buffer, 1, PAGE_SIZE, file);
                            fseek(file, (long)page_offset, SEEK_SET);
                            fread(buffer, 1, PAGE_SIZE, file);
                        }
                        else {
                            for (int j = 0; j < count; j += 1) {
                                long dst_offset = (long)(map_page->entries[i + j] * PAGE_SIZE);
                                fseek(out, dst_offset, SEEK_SET);
                                result = fwrite(uncompressed_buffer + (j + PAGE_SIZE), 1, PAGE_SIZE, out);
                                if (verbose_mode) printf("debug: wrote 0x%x bytes at %x.\n", result, dst_offset);
                            }
                        }
                    }

                    i += count;
                    pages_count += LZO_UNC_PAGES;

                    continue;
                }
            } else {
                if (out) {
                    fseek(file, (long)page_offset, SEEK_SET);
                    fread(buffer, 1, PAGE_SIZE, file);
                    fseek(out, (long)page_offset, SEEK_SET);
                    result = fwrite(buffer, 1, PAGE_SIZE, out);
                    if (verbose_mode) printf("debug: wrote 0x%x bytes at %llx.\n", result, page_offset);
                }

                i += 1;
                pages_count++;
            }
        }

        printf("map_page->next_swap = 0x%llx\n", map_page->next_swap);
        if (map_page->next_swap) printf("gap is %d pages\n", (uint32_t)(map_page->next_swap - (map_table_offset / PAGE_SIZE)));
        map_table_offset = map_page->next_swap * PAGE_SIZE;
    }

    printf("debug: total page count = 0x%x (%d MB)\n", pages_count, (pages_count * PAGE_SIZE) / (1024 * 1024));
    printf("debug: total missed pages = 0x%x (%d MB)\n", err_pages_count, (err_pages_count * PAGE_SIZE) / (1024 * 1024));

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