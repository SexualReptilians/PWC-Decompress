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
    char buf[4000];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        char path[3000] = {0};
        char sha1[41] = {0};
        char compr[9] = {0};
        uint64_t off = 0;
        uint64_t size = 0;

        char *pbuf = buf;

        char *path_start = strchr(pbuf, '"');
        if (path_start == NULL) continue;

        char *path_end = strchr(path_start+1, '"');
        if (path_end == NULL) continue;

        size_t path_len = ((path_end-path_start)/sizeof(char))-1;
        // always leave space for zero terminator when dealing with strings
        if (path_len > (sizeof(path)/sizeof(char))-1) {
            printf("WARNING: file name is too long for input: %s\n", buf);
            continue;
        }

        memcpy(path, path_start+1, path_len);
        // start on the second quote for the next search
        pbuf = path_end+1;

        char *poff = strstr(buf, "offset:");
        if (poff == NULL) continue;
        poff += sizeof(char)*8;
        sscanf(poff, "%llu", &off);

        char *soff = strstr(buf, "size:");
        if (soff == NULL) continue;
        soff += sizeof(char)*6;
        sscanf(soff, "%llu", &size);

        char *shaoff = strstr(buf, "sha1:");
        if (shaoff == NULL) continue;
        shaoff += sizeof(char)*6;

        memcpy(sha1, shaoff, sizeof(char)*40);
        
        char *compoff = strstr(buf, "compression:");
        if (compoff == NULL) continue;
        compoff += sizeof(char)*13;

        size_t compsize = strlen(compoff);
        compoff[compsize-2] = '\0';
        sscanf(compoff, "%s", compr);

        printf("Got [%s] with offset %08llX, size %08llX, sha1 [%s] compression [%s]\n", path, off, size, sha1, compr);
    }
    return 0;
}

