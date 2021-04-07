#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pwc.h>

int main(int argc, char *argv[]) {
    // Usage
    if (argc != 3) {
        printf("Syntax: %s <input> <output>\n", argv[0]);
        return 1;
    }

    // Open input
    FILE *fp_input;
    fp_input = fopen(argv[1], "rb");

    // On failure
    if (fp_input == NULL) {
        printf("Input file read error!\n");
        return 1;
    }

    // Serialize PakFile
    struct pak_file *mypak = parsePakFile(fp_input);
    fclose(fp_input);

    // Not valid PAK
    if (mypak == NULL) {
        printf("File is not a valid PAK!\n");
        return 1;
    }

    // Generate ouput buffer
    uint8_t *outbuf = malloc(mypak->size_decompressed);
    if (outbuf == NULL) {
        printf("Error creating output buffer!\n");
        cleanPakFile(mypak);
        return 1;
    }

    // Decompress the PAK data
    size_t size_read = decompressPWC(outbuf, mypak->data, mypak->size_compressed);

    // Verbose
    if (PRINT_VERBOSE) {
        for (int i=0;i<mypak->size_decompressed;i++) {
            printf("%02X ", outbuf[i]);
            if (!((i+1)%8)) printf(" ");
            if (!((i+1)%16)) printf("\n");
        }
        printf("\n%u\n", size_read);
    }

    // Decompression size differs from specified in header
    if (size_read != mypak->size_decompressed) {
        printf("PWC Decompression failure!\n");
        free(outbuf);
        cleanPakFile(mypak);
        return 1;
    }

    // Create file stream
    FILE *fp_output;
    fp_output = fopen(argv[2], "wb");
    if (fp_output == NULL) {
        printf("Error creating output file!\n");
        free(outbuf);
        cleanPakFile(mypak);
        return 1;
    }

    // Write to file
    fwrite(outbuf, 1, size_read, fp_output);
    fclose(fp_output);
    free(outbuf);

    cleanPakFile(mypak);
    return 0;
}

struct pak_file *parsePakFile(FILE *fp) {
    // go to EOF and get file size
    fseek(fp, 0, SEEK_END);
    uint64_t size = ftell(fp);
    rewind(fp);

    // Invalid size (no data block)
    if (size < PAK_HEADER_SIZE+1) {
        return NULL;
    }

    // check header
    uint64_t header = UINT64_MAX;
    fread(&header, 8, 1, fp);
    if (header) {
        return NULL;
    }

    // Pak meta into structure
    struct pak_file *newfile;
    newfile = malloc(sizeof(struct pak_file));
    if (newfile == NULL) {
        return NULL;
    }

    // Serialize header
    fread(&newfile->size_compressed, 8, 1, fp);
    fread(&newfile->size_decompressed, 8, 1, fp);
    fseek(fp, 4, SEEK_CUR);
    fread(newfile->sha1, 1, 20, fp);

    // Create buffer for the file data
    newfile->data = malloc(newfile->size_compressed);
    if (newfile->data == NULL) {
        free(newfile);
        return NULL;
    }

    // Put raw data block into the data buffer
    fseek(fp, 25, SEEK_CUR);
    size_t count_read = fread(newfile->data, 1, newfile->size_compressed, fp);

    // Something went wrong reading. Stop
    if (count_read != newfile->size_compressed) {
        free(newfile->data);
        free(newfile);
        return NULL;
    }

    // Verbose
    if (PRINT_VERBOSE) {
        printf("Packed: %llu\n", newfile->size_compressed);
        printf("Unpacked: %llu\n", newfile->size_decompressed);
        printf("SHA-1: ");
        for (int i = 0; i < 20; i++) {
            printf("%02X ", newfile->sha1[i]);
        }
        printf("\n");
        for (int i = 0; i < newfile->size_compressed; i++) {
            printf("%02X ", newfile->data[i]);
            if (!((i+1) % 8)) printf(" ");
            if (!((i+1) % 16)) printf("\n");
        }
        printf("\n");
    }

    return newfile;
}

// Free up pak_file structure
void cleanPakFile(struct pak_file *pf) {
    if (pf == NULL) return;

    if (pf->data != NULL) {
        free(pf->data);
    }

    free(pf);
}

size_t decompressPWC(uint8_t *out_buffer, uint8_t *in_buffer, uint64_t size_c) {
    uint8_t *data = in_buffer;
    uint8_t *out = out_buffer;

    // Check if file magic exists
    if (memcmp(data+1, &pwc_magic1, sizeof(pwc_magic1))) {
        if (memcmp(data+1, &pwc_magic2, sizeof(pwc_magic2))) {
            printf("No magic found\n");
            if (!ALLOW_NOMAGIC) return 0;
        }
    }

    // XOR the first 16 bytes of the stream
    for (uint64_t i = 0; i < 16; i++) {
        data[i] ^= PWC_XOR_KEY;
    }

    uint64_t index = 0;
    uint64_t oindex = 0;
    uint16_t skip;
    uint16_t psize;
    uint8_t pext = 0;
    uint16_t backtrack;

    while (index < size_c) {
        skip = (data[index]>>4) & 0x0F; // HI NIB
        psize = data[index] & 0x0F;     // LO NIB

        // Special case, skip has additional byte
        if (skip == 0x0F) {
            index++;
            skip += data[index];
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
                if (PRINT_VERBOSE) printf("Reached end of data block!\n");
                break;
            }
            else {
                if (PRINT_VERBOSE) printf("Error: Offset exceeded input buffer!\n");
                return oindex;
            }
        }

        // Next 2 bytes are our backtrack number
        memcpy(&backtrack, data+index, 2);
        index += 2;

        // Backtrack underrun
        if (backtrack > oindex) {
            if (PRINT_VERBOSE) printf("Error: Backtrack underran current output buffer lenght!\n");
            return oindex;
        }

        // Extended pointer flag was set, increment indexes
        if (pext) {
            psize += data[index];
            index++;
            pext = 0;
        }

        // Repeat backtracked data block while moving
        if (psize > backtrack) {
            for (uint8_t i = 0; i < psize; i++) {
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