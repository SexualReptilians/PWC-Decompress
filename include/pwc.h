#ifndef __PWC_H
#define __PWC_H

static const uint8_t print_verbose = 0;

#define PWC_XOR_KEY 0xCC

size_t decompressPWC(uint8_t *out_buffer, uint8_t *in_buffer, uint64_t size_c);

#endif // __PWC_H
