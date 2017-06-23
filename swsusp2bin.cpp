#include "swsusp2bin.h"

typedef struct _parameters_t {
    char *swapfile;
} parameters_t;

void
help() {
    printf("    --input     swapfile\n");
}

bool
parse(int argc,
      char **argv,
      parameters_t *out) {

    bool status = false;

    for (int i = 0; i < argc; i++) {
        string keyword = argv[i];
        if ((keyword == "--input") && ((i + 1) < argc)) {
            out->swapfile = argv[++i];
            status = true;
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

int main(
    int argc,
    char **argv)
{
    parameters_t params;
    char *buffer = NULL;
    swap_map_page *map_page = NULL;
    FILE * file = NULL;
    size_t result;

    printf("swsusp2bin\n");

    if (!parse(argc, argv, &params)) {
        help();
        goto cleanup;
    }

    printf("analyzing %s...\n", params.swapfile);

    file = fopen(params.swapfile, "r");
    if (file == NULL) {
        printf("error: can't open file.\n");
        return false;
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

    print_header();
    if (!is_swapfile_valid()) goto cleanup;

    uint64_t map_table_offset = swsusp_header->image * PAGE_SIZE;

    printf("image: 0x%I64x\n", map_table_offset);

    map_page = (struct swap_map_page *)malloc(PAGE_SIZE);
    if (map_page == NULL) goto cleanup;

    int pages_count = 0;

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

        for (int i = 0; map_page->entries[i] && (i < MAP_PAGE_ENTRIES); i++) {
            printf("[0x%llx][%4d] = 0x%llx\n", map_table_offset, i, map_page->entries[i]);
            pages_count++;
        }

        printf("gap is %d pages\n", (uint32_t)(map_page->next_swap - (map_table_offset / PAGE_SIZE)));
        map_table_offset = map_page->next_swap * PAGE_SIZE;
    }

    printf("debug: total page count = 0x%x\n", pages_count);

cleanup:
    if (map_page) free(map_page);
    if (buffer) free(buffer);
    if (file) fclose(file);
    return true;
}