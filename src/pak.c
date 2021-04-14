#if defined(__amd64) || defined(_M_AMD64)
#else
#error System is not x86_64
#endif

#ifdef _WIN32
#include <io.h>
#elif __linux__
#include <unistd.h>
#include <sys/types.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pak.h>
#include <pwc.h>

int main(int argc, char *argv[]) {
    // Usage
    if (argc < 3) {
        printf("Usage: <UnrealPak> <pakFile> -List | %s <pakFile> <outputDir>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *fp_pak = fopen(argv[1],"rb");
    if (fp_pak == NULL) {
        printf("Can't open PAK file.\n");
        return EXIT_FAILURE;
    }

    const char *output_dir = argv[2];

    // if run directly instead of piped
    if (isatty(fileno(stdin))) {
        printf("Warning: It would be preferable if info is piped into this program instead of direct input.\n");
        printf("Usage: <UnrealPak> <pakFile> -List | %s <pakFile> <outputDir>\n", argv[0]);
        printf("[Waiting for input...]:\n");
    }

    // stats for nerds
    int stotal = 0, ssuccess = 0, signored = 0, serror = 0, sinvalid = 0;
    clock_t start = clock();

    size_t charsize = sizeof(char);
    char buf[1024];

    // Get from stdin (piped from UnrealPak)
    while (fgets(buf, (sizeof(buf) / charsize) - 1, stdin) != NULL) {
        struct pak_record_info rec = {
            .path = {0},
            .sha1 = {0},
            .compression = {0},
            .offset = 0,
            .size = 0,
        };

        // Get first quote
        char *path_start = strchr(buf, '"');
        if (path_start == NULL) continue;
        path_start += charsize;

        // Get second quote
        char *path_end = strchr(path_start, '"');
        if (path_end == NULL) continue;
        path_end -= charsize;

        // Length
        size_t path_len = ((path_end - path_start) / charsize) + charsize;
        // always leave space for zero terminator when dealing with strings
        if (path_len > sizeof(rec.path) + (strlen(output_dir) * charsize) - charsize) {
            printf("WARNING: file name is too long for input: %s\n", buf);
            continue;
        }

        // Copy dir and substring (skip first quote)
        strcat(rec.path, output_dir);
        strncat(rec.path, path_start, path_len);

        char *match_offset;

        // Get pak header offset
        match_offset = strstr(buf, "offset:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 8;
        if (!sscanf(match_offset, "%llu", &rec.offset)) continue;

        // Get data size
        match_offset = strstr(buf, "size:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 6;
        if(!sscanf(match_offset, "%llu", &rec.size)) continue;

        // Get sha1 of data
        match_offset = strstr(buf, "sha1:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 6;
        memcpy(&rec.sha1, match_offset, charsize * 40);
        
        // Get compression name
        match_offset = strstr(buf, "compression:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 13;
        match_offset[(strlen(match_offset) - 2) * charsize] = '\0';
        if(!sscanf(match_offset, "%s", rec.compression)) continue;

        switch (processRecord(fp_pak, rec)) {
            case PAK_SUCCESS:
                if(print_verbose) printf("Done parsing file at %08llX!\n", rec.offset);
                ssuccess++;
                break;
            case PAK_OUTFILE_ERR:
                printf("Error opening output file [%s]! Have you ran UnrealPak -Extract first?\n", rec.path);
                printf("Or make sure output directory is present before running this.\n");
                serror++;
                break;
            case PAK_UNSUPPORTED_COMPRESSION:
                signored++;
                break;
            case PAK_EOF:
                printf("Error: Reached EOF before obtaining required amount of data...\n");
                printf("       @ %08llX [%s]\n", rec.offset, rec.path);
                serror++;
                break;
            case PAK_NO_MEMORY:
                printf("Error: Download more RAM.\n");
                printf("       @ %08llX [%s]\n", rec.offset, rec.path);
                serror++;
                break;
            case PAK_INVALID:
                printf("Error: Invalid pak block.\n");
                printf("       @ %08llX [%s]\n", rec.offset, rec.path);
                sinvalid++;
                if (sinvalid > 10) {
                    printf("FATAL: Wrong PAK file probably. Quitting.\n");
                }
                return EXIT_FAILURE;
            case PAK_WTF:
                printf("Error: wtf\n");
                printf("       @ %08llX [%s]\n", rec.offset, rec.path);
                serror++;
        }

        stotal++;
    }
    clock_t end = clock();

    fclose(fp_pak);
    printf("Done processing files in %.4fs, total %d. (%d succeeded, %d ignored, %d errored)",
           (double)(end - start) / CLOCKS_PER_SEC, stotal, ssuccess, signored, serror);

    return EXIT_SUCCESS;
}

enum PAK_ERROR processRecord(FILE *fp_pak, struct pak_record_info rec) {
    if (strcmp("PWC", rec.compression)) return PAK_UNSUPPORTED_COMPRESSION;

    struct pak_metadata meta;
    off_t mark;

    // Seek to data header offset
    fseeko(fp_pak, rec.offset, SEEK_SET);

    // Check for pak header lead zeroes
    uint64_t header = UINT64_MAX;
    fread(&header, 8, 1, fp_pak);
    if (header) return PAK_INVALID;


    fread(&meta.size_compressed, sizeof(meta.size_compressed), 1, fp_pak);
    fread(&meta.size_decompressed, sizeof(meta.size_decompressed), 1, fp_pak);
    // skip flags and sha
    fseeko(fp_pak, 24, SEEK_CUR);
    fread(&meta.block_count, sizeof(meta.block_count), 1, fp_pak);

    mark = ftello(fp_pak);

    // Skip offsets
    fseeko(fp_pak, (16 * meta.block_count) + 1, SEEK_CUR);
    if (!fread(&meta.block_unpacked, sizeof(meta.block_unpacked), 1, fp_pak)) return PAK_EOF;

    FILE *fp_out;
    fp_out = fopen(rec.path, "wb");
    if (fp_out == NULL) return PAK_OUTFILE_ERR;

    for (uint32_t i = 0; i < meta.block_count; i++) {
        // offsets from the start of meta (rec.offset+block_)
        uint64_t block_start;
        uint64_t block_end;

        // Jump to last offset table mark
        fseeko(fp_pak, mark, SEEK_SET);

        // Read start offset
        if(!fread(&block_start, sizeof(block_start), 1, fp_pak)) {
            fclose(fp_out);
            return PAK_EOF;
        }

        // Read end offset
        if(!fread(&block_end, sizeof(block_end), 1, fp_pak)) {
            fclose(fp_out);
            return PAK_EOF;
        }

        uint64_t block_size = block_end - block_start;

        if (!block_size || block_size > meta.size_compressed) {
            fclose(fp_out);
            return PAK_WTF;
        }

        // Mark next offset in offset table
        mark = ftello(fp_pak);

        uint8_t *data_compressed = malloc(block_size);
        if (data_compressed == NULL) {
            fclose(fp_out);
            return PAK_NO_MEMORY;
        }

        // Jump to and read compressed block
        fseeko(fp_pak, rec.offset + block_start, SEEK_SET);
        if(!fread(data_compressed, block_size, 1, fp_pak)) {
            free(data_compressed);
            fclose(fp_out);
            return PAK_EOF;
        }

        uint8_t *data_decompressed = malloc(meta.block_unpacked);
        if (data_decompressed == NULL) {
            free(data_compressed);
            fclose(fp_out);
            return PAK_NO_MEMORY;
        }

        // Decompress block
        size_t uncompressed_size = decompressPWC(data_decompressed, data_compressed, block_size);
        if (uncompressed_size != meta.block_unpacked && i != meta.block_count - 1) {
            // only last block should have uncompressed size < block_unpacked, but we continue anyway
            printf("PWC: Warning: Block %d is not of the same length as block_unpacked %08X.\n", i, meta.block_unpacked);
        }

        fwrite(data_decompressed, uncompressed_size, 1, fp_out);

        free(data_compressed);
        free(data_decompressed);
    }
    fclose(fp_out);

    return PAK_SUCCESS;
}