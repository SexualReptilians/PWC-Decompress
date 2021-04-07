#ifndef __PWC_H
#define __PWC_H

#include <stdint.h>

// (LE)
// 00-07 - Header start, all zeroes.
// 08-15 - Data length
// 16-23 - Unpacked length
// 24-27 - ?? Only value is a 1 probably flag bitfield?
// 28-47 - SHA-1 of data block
// 48-55 - ???
// 56-63 - ???
// 64-71 - ???
// 72 - Zero
// 73-73+datalength - Raw data


#define PRINT_VERBOSE 1
#define ALLOW_NOMAGIC 1


#define PAK_HEADER_SIZE 0x49
#define PWC_XOR_KEY 0xCC

const uint8_t pwc_magic1[3] = {0xCC, 0xCD, 0xCC};
const uint8_t pwc_magic2[4] = {0x0D, 0x4F, 0xE6, 0x52};

struct pak_file {
    uint64_t size_compressed;
    uint64_t size_decompressed;
    uint8_t sha1[20];
    uint8_t *data;
};

struct pak_file *parsePakFile(FILE *fp);
void cleanPakFile(struct pak_file *pf);
size_t decompressPWC(uint8_t *out_buffer, uint8_t *in_buffer, uint64_t size_c);

#endif // __PWC_H