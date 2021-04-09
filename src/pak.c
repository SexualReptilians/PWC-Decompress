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
    char buf[512];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        char path[255] = "\0";  // strncat expects an initialized string
        char sha1[41] = "\0";
        char compr[9] = "\0";
        uint64_t off = 0;
        uint64_t size = 0;

        char *pbuf = buf;

        char *qs = strchr(pbuf, '"');
        if (qs == NULL) continue;       // invalid entry
        qs++;

        char *qe = strchr(qs,'"');
        if (qe == NULL) continue;       // invalid entry what

        if (qe-qs > sizeof(path)/sizeof(char)) {
            printf("WARNING: file name is too long for input: %s\n", buf);
            continue;
        }

        strncat(path, qs, qe-qs);
        pbuf = qe;                      // start on the second quote for the next search

        // Next number: offset
        while(*pbuf) {
            if (isdigit(*pbuf)) {
                off = strtoull(pbuf, &pbuf, 10);
                break;
            }
            else pbuf++;
        }
        if (off == 0) continue;     // waht

        // Next number: size
        while(*pbuf) {
            if (isdigit(*pbuf)) {
                size = strtoull(pbuf, &pbuf, 10);
                break;
            }
            else pbuf++;
        }
        if (size == 0) continue;     // uhh

        pbuf = strchr(pbuf, ':');
        pbuf += 2;
        strncat(sha1, pbuf, 40);

        pbuf = strchr(pbuf, ':');
        pbuf += 2;
        qe = strchr(pbuf, '.');     // lol variable reuse
        if (qe-pbuf > 8) {
            printf("WARNING: compression format name is too long: %s\n", buf);
            continue;
        }
        strncat(compr, pbuf, qe-pbuf);

        printf("Got [%s] with offset %08llX, size %08llX, sha1 [%s] compression [%s]\n", path, off, size, sha1, compr);
    }
    return 0;
}

