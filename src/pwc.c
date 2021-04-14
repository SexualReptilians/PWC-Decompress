#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <pwc.h>

size_t decompressPWC(uint8_t *out_buffer, uint8_t *in_buffer, uint64_t size_c) {
    uint8_t *data = in_buffer;
    uint8_t *out = out_buffer;

    if (size_c < 17) return 0;

    // XOR the first 16 bytes of the stream
    for (uint64_t i = 0; i < 16; i++) {
        data[i] ^= PWC_XOR_KEY;
    }

    size_t index = 0;
    size_t oindex = 0;
    uint64_t skip;
    uint64_t psize;
    uint8_t pext = 0;
    uint64_t backtrack = 0;

    while (index < size_c) {
        skip = (data[index]>>4) & 0x0F;
        psize = data[index] & 0x0F;

        // Special case, skip has additional byte(s)
        if (skip == 0x0F) {
            while (1) {
                index++;
                skip += data[index];
                if (data[index] != 0xFF) break;
            }
        }

        // Special case, next pointer will have additional byte
        if (psize == 0x0F) {
            pext = 1;
        }

        // Size base always 4
        psize += 4;
        index++;

        // Copy skip bytes to output buffer
        memcpy(out+oindex, data+index, skip);
        index += skip;
        oindex += skip;

        // End condition
        if (index >= size_c) {
            if (index == size_c) {
                if (print_verbose) printf("Reached end of data block!\n");
            }
            else printf("Error: Offset exceeded input buffer!\n");
            break;  // main while
        }

        // Next 2 bytes are our backtrack number
        memcpy(&backtrack, data+index, 2);
        index += 2;

        // Backtrack underrun
        if (backtrack > oindex) {
            printf("Error: Backtrack underran current output buffer length!\n");
            break;  // main while
        }

        // Extended pointer flag was set, accumulate psize
        if (pext) {
            while(1) {
                psize += data[index];
                uint8_t odata = data[index];
                index++;
                if (odata != 0xFF) break;
            }
            pext = 0;
        }

        // Repeat backtracked data block while moving
        if (psize > backtrack) {
            for (uint64_t i = 0; i < psize; i++) {
                out[oindex] = out[oindex-backtrack];
                oindex++;
            }
        }
            // Repeat backtracked data block statically
        else {
            memcpy(out+oindex, out+oindex-backtrack, psize);
            oindex += psize;
        }
    }

    return oindex;
}