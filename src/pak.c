#if defined(__amd64) || defined(_M_AMD64)
#else
#error System is not x86_64
#endif

#if defined(_WIN32)
#include <io.h>
#else
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
        printf("Usage:   <UnrealPak> <pakFile> -List | %s <pakFile> <outputDir>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *fp_pak = fopen(argv[1],"rb");
    if (fp_pak == NULL) {
        printf("Can't open PAK file.\n");
        return EXIT_FAILURE;
    }
    const char* output_dir = argv[2];

    if (isatty(fileno(stdin))) {    // if run directly instead of piped
        printf("Warning: It would be preferable if info is piped into this program instead of direct input.\n");
        printf("Usage:   <UnrealPak> <pakFile> -List | %s <pakFile> <outputDir>\n", argv[0]);
        printf("[Waiting for input...]:\n");
    }

    // stats for nerds
    int stotal = 0, ssuccess = 0, signored = 0, serror = 0, sinvalid = 0;
    clock_t start = clock();

    // Get from stdin (piped from UnrealPak)
    size_t charsize = sizeof(char);
    char buf[1024];
    while (fgets(buf, sizeof(buf) / charsize, stdin) != NULL) {
        struct pak_record_info rec = {
            .path = {0},
            .sha1 = {0},
            .compr = {0},
            .off = 0,
            .size = 0,
        };

        // Get first quote
        char *path_start = strchr(buf, '"');
        if (path_start == NULL) continue;
        path_start += charsize;

        // Get second quote (starting first+1)
        char *path_end = strchr(path_start, '"');
        if (path_end == NULL) continue;
        path_end -= charsize;

        // Length minus the end quote
        size_t path_len = ((path_end-path_start) / charsize) + charsize;
        // always leave space for zero terminator when dealing with strings
        if (path_len > ((sizeof(rec.path)+strlen(output_dir)) / charsize) - charsize) {
            printf("WARNING: file name is too long for input: %s\n", buf);
            continue;
        }

        // Copy dir and substring (skip first quote)
        strcat(rec.path, output_dir);
        strncat(rec.path, path_start, path_len);

        char *match_offset;

        match_offset = strstr(buf, "offset:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 8;
        if (!sscanf(match_offset, "%llu", &rec.off)) continue;

        match_offset = strstr(buf, "size:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 6;
        if(!sscanf(match_offset, "%llu", &rec.size)) continue;

        match_offset = strstr(buf, "sha1:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 6;
        memcpy(&rec.sha1, match_offset, charsize * 40);
        
        match_offset = strstr(buf, "compression:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 13;

        size_t compsize = strlen(match_offset);
        match_offset[compsize - (2 * charsize)] = '\0';
        if(!sscanf(match_offset, "%s", rec.compr)) continue;

        enum PAK_ERROR proc_err = processRecord(fp_pak, rec);
        switch (proc_err) {
            case PAK_SUCCESS:
                if(print_verbose) printf("Done parsing file at %08llX!\n", rec.off);
                ssuccess++;
                break;
            case PAK_OUTFILE_ERR:
                printf("Error opening output file [%s]! Have you ran UnrealPak -Extract first?\n", rec.path);
                printf("Or make sure output directory is present before running this.\n");
                serror++;
                break;
            case PAK_UNSUPPORTED_COMPRESSION: signored++; break;    // ignore
            case PAK_EOF:
                printf("Error: Reached EOF before obtaining required amount of data...\n");
                printf("       @ %08llX [%s]\n", rec.off, rec.path);
                serror++;
                break;
            case PAK_NO_MEMORY:
                printf("Error: Download more RAM.\n");
                printf("       @ %08llX [%s]\n", rec.off, rec.path);
                serror++;
                break;
            case PAK_INVALID:
                printf("Error: Invalid pak block.\n");
                printf("       @ %08llX [%s]\n", rec.off, rec.path);
                sinvalid++;
                if (sinvalid > 10) {
                    printf("FATAL: Wrong PAK file probably. Quitting.\n");
                }
                return EXIT_FAILURE;
            default:
                printf("Error: wtf\n");
                printf("       @ %08llX [%s]\n", rec.off, rec.path);
                serror++;
        }

        stotal++;
    }
    clock_t end = clock();

    fclose(fp_pak);
    printf("Done processing files in %.4fs, total %d. (%d succeeded, %d ignored, %d errored)", (double)(end-start)/CLOCKS_PER_SEC , stotal, ssuccess, signored, serror);

    return EXIT_SUCCESS;
}

enum PAK_ERROR processRecord(FILE *fp_pak, struct pak_record_info rec) {
    if (strcmp("PWC", rec.compr) != 0) return PAK_UNSUPPORTED_COMPRESSION;

    struct pak_metadata meta;
    size_t count_read;
    off64_t mark;

    fseeko(fp_pak, rec.off, SEEK_SET);    // NOTE: Non-ISO compliant, alternative can be multiple fseek calls? (which is unsafe?)

    uint64_t zeroes = UINT64_MAX;
    fread(&zeroes, 8, 1, fp_pak);
    if (zeroes) return PAK_INVALID;

    fread(&meta.size_compr, sizeof(meta.size_compr), 1, fp_pak);
    fread(&meta.size_decompr, sizeof(meta.size_decompr), 1, fp_pak);
    fseeko(fp_pak, 4+20, SEEK_CUR);
    fread(&meta.block_count, sizeof(meta.block_count), 1, fp_pak);
    mark = ftello(fp_pak);              // *push*

    fseeko(fp_pak, (16*meta.block_count) + 1, SEEK_CUR);
    count_read = fread(&meta.block_unp_size, sizeof(meta.block_unp_size), 1, fp_pak);
    if (count_read == 0) return PAK_EOF;

    FILE *fo;
    fo = fopen(rec.path, "wb");
    if (fo == NULL) return PAK_OUTFILE_ERR;

    fseeko(fp_pak, mark, SEEK_SET);     // *pop*
    for (uint32_t b = 0; b < meta.block_count; b++) {
        // offsets from the start of meta (rec.off+block_)
        uint64_t block_start;   // inclusive
        uint64_t block_end;     // exclusive
        count_read = fread(&block_start, 8, 1, fp_pak);
        if (count_read == 0) { fclose(fo); return PAK_EOF; }
        count_read = fread(&block_end, 8, 1, fp_pak);
        if (count_read == 0) { fclose(fo); return PAK_EOF; }
        if (block_end-block_start < 1 || block_end-block_start > meta.size_compr) {
            fclose(fo);
            return PAK_WTF;
        }

        mark = ftello(fp_pak);      // push position for later

        uint8_t *data = malloc(block_end-block_start);
        if (data == NULL) { fclose(fo); return PAK_NO_MEMORY; }

        fseeko(fp_pak, rec.off+block_start, SEEK_SET);
        count_read = fread(data, block_end-block_start, 1, fp_pak);
        if (count_read == 0) {
            free(data);
            fclose(fo);
            return PAK_EOF;
        }

        uint8_t *out = malloc(meta.block_unp_size);
        if (out == NULL) {
            free(data);
            fclose(fo);
            return PAK_NO_MEMORY;
        }

        size_t uncomp_size = decompressPWC(out, data, block_end-block_start);
        free(data);
        if (uncomp_size != meta.block_unp_size && b != meta.block_count-1) {
            // only last block should have uncompressed size < block_unp_size, but we continue anyway
            printf("PWC: Warning: Block %d is not of the same length as block_unp_size %08X.\n", b, meta.block_unp_size);
        }

        fwrite(out, uncomp_size, 1, fo);
        free(out);

        fseeko(fp_pak, mark, SEEK_SET);     // *pop*
    }
    fclose(fo);

    return PAK_SUCCESS;
}








