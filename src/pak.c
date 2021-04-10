//
// Created by Lyr on 4/9/2021.
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pak.h>
#include <ctype.h>

int test() {
    // line format
    // LogPakFile: Display: "Century/Content/Characters/Customization/Flags/Flag001/Materials/Axe/MI_Flag001_Axe_F.uasset" offset: 27609088, size: 1578 bytes, sha1: C90BAA3D00A39B72BA583214790D4A6676F9CCCF, compression: PWC.

    /*
     * This thing just reads stdin from UnrealPak, line-by-line,
     * stores the data we need from it into variables, and prints them out
     */

    // Get from stdin (piped from UnrealPak)
    size_t charsize = sizeof(char);
    char buf[4000];
    while (fgets(buf, sizeof(buf) / charsize, stdin) != NULL) {
        // Full array zero initialize
        char path[3000] = {0};
        char sha1[41] = {0};
        char compr[100] = {0};
        size_t off = 0;
        size_t size = 0;

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
        if (path_len > (sizeof(path) / charsize) - charsize) {
            printf("WARNING: file name is too long for input: %s\n", buf);
            continue;
        }

        // Copy substring (skip first quote)
        memcpy(path, path_start, path_len);

        char *match_offset;

        match_offset = strstr(buf, "offset:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 8;
        if (!sscanf(match_offset, "%llu", &off)) continue;

        match_offset = strstr(buf, "size:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 6;
        if(!sscanf(match_offset, "%llu", &size)) continue;

        match_offset = strstr(buf, "sha1:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 6;
        memcpy(sha1, match_offset, charsize * 40);
        
        match_offset = strstr(buf, "compression:");
        if (match_offset == NULL) continue;
        match_offset += charsize * 13;

        size_t compsize = strlen(match_offset);
        match_offset[compsize - (2 * charsize)] = '\0';
        if(!sscanf(match_offset, "%s", compr)) continue;

        printf("Got [%s] with offset %08llX, size %08llX, sha1 [%s] compression [%s]\n", path, off, size, sha1, compr);
    }
    return 0;
}
