#ifndef __PAK_H
#define __PAK_H

#define PAK_HEADER_SIZE 0x49

// From UnrealPak output
struct pak_record_info {
    char path[512];
    char sha1[41];
    char compression[64];
    size_t offset;
    size_t size;
};


// (LE)
//  8 | 00~07 - Header start, all zeroes.
//  8 | 08~0F - Data length
//  8 | 10~17 - Unpacked length
//  4 | 18~1B - ?? Only value is a 1 probably flag bitfield?
// 20 | 1C~2F - SHA-1 of data block
//  4 | 30~33 - Block count N
// 16*| 34~+(N*10h)-1 - >>> Block offsets [8b start offset][8b end offset]
//  1 | 34+(N*10h)  - Zero
//  4 | 34+(N*10h)+1~34+(N*10h)+4 - >>> Unpacked block size
//  X | 34+(N*10h)+5~ - Raw data

// Blocks of data within the pak
struct pak_metadata {
    uint64_t size_compressed;
    uint64_t size_decompressed;
    uint32_t block_count;
    uint32_t block_unpacked;
};

// Pak processing err codes
enum PAK_ERROR {
    PAK_SUCCESS = 0,
    PAK_OUTFILE_ERR,
    PAK_UNSUPPORTED_COMPRESSION,
    PAK_EOF,
    PAK_NO_MEMORY,
    PAK_INVALID,
    PAK_WTF,
};

enum PAK_ERROR processRecord(FILE *fp_pak, struct pak_record_info rec);

#endif // __PAK_H
