#ifndef __PWC_H
#define __PWC_H

#include <stdint.h>

#define PAK_HEADER_SIZE 0x49
#define PWC_XOR_KEY 0xCC

static const uint8_t print_verbose = 0;

struct pak_record {
    uint64_t size_compressed;
    uint64_t size_decompressed;
    uint64_t data_offset;
    uint8_t sha1[20];
    uint8_t *data;
};

size_t decompressPWC(uint8_t *out_buffer, uint8_t *in_buffer, uint64_t size_c);
struct pak_record *parsePakRecord(FILE *fp);
void cleanPakFile(struct pak_record *pf);

#endif // __PWC_H
