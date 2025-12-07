/** @file
    Decoder for Kia V2
    
    By L0rdDiakon

*/

#include "decoder.h"

#define NUM_BITS_PREAMBLE_v2 24
#define NUM_BYTES_DATA_v2    13

uint8_t const preamble_v2[] = {0xcc, 0xcc, 0xcd};

static int kia_v2_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{   
    uint8_t *b = bitbuffer->bb[0];
    
    //check partial preamble
    if ((b[0] != 0xf9 && b[0] != 0x99 && b[0] != 0xcc) || (b[1] != 0x99 && b[1] != 0xcc) || (b[2] != 0x99 && b[2] != 0xcc)) {
         return DECODE_FAIL_SANITY;
    }

    int available_bits = bitbuffer->bits_per_row[0];

    // sync on preamble
    unsigned start_pos = bitbuffer_search(bitbuffer, 0, 0, preamble_v2, NUM_BITS_PREAMBLE_v2);
    available_bits -= start_pos;

    
    int total_bits = (NUM_BYTES_DATA_v2 * 8);
    
    //Abort if preamble not found and/or not enough to extract
    if (available_bits < total_bits) {
        return DECODE_FAIL_SANITY; 
    }

    uint8_t b_data[NUM_BYTES_DATA_v2];
    
    //Extract payload bytes start at preamble -1 bit to align manchester
    bitbuffer_extract_bytes(bitbuffer, 0, start_pos + NUM_BITS_PREAMBLE_v2 -1, b_data, total_bits);

    int bit_count = 52;
    uint64_t final = 0;
    size_t j = 0;
    while (bit_count > 0) {
        for (int i = 3; i >= 0; i--) {
            uint8_t two_bits = (b_data[j] >> (i * 2)) & 0x03;
            if (two_bits == 0x2) {
                bit_count-=1;
                final |= 1ULL << (bit_count);
            }
            else if (two_bits == 0x1) {
                bit_count-=1;
            }
            else {
                //bail if illegal value
                return DECODE_FAIL_SANITY;
            }
            if (bit_count <= 0) {
                break;
            }
        }
        j++;
    }

    uint32_t serial  = (uint32_t)(final >> 20) & 0xFFFFFFFF;
    uint16_t count = (uint16_t)(((final >> 4) & 0xFFF) >> 4 | ((final >> 4) & 0xFFF) << 8) & 0xFFF;
    uint8_t btn = (uint8_t)(final >> 16) & 0xF;  
    uint8_t crc = (uint8_t)(final & 0x0F);
    
    char key_str[17];
    snprintf(key_str, sizeof(key_str), "%08lX", final);
    char count_str[5];
    snprintf(count_str, sizeof(count_str), "%03X", count);
    char serial_str[9];
    snprintf(serial_str, sizeof(serial_str), "%08X", serial);
    
         /* clang-format off */
    data_t *data = data_make(
            "model",            "",             DATA_STRING,    "Kia V2",
            "id",              "serial",        DATA_STRING,    serial_str,
            "key",              "",             DATA_STRING,    key_str,
            "btn",              "",   DATA_FORMAT, "%01X", DATA_INT, (unsigned)(btn),
            "count",            "",             DATA_STRING,    count_str,
            "crc",              "",   DATA_FORMAT, "%01X", DATA_INT, (unsigned)(crc),
            NULL);
    /* clang-format on */
    
    decoder_output_data(decoder, data);
    return 1;
}


static char const *const output_fields[] = {
        "model",
        "id",
        "key",
        "btn",
        "count",
        "crc",
        NULL,
};


r_device const kia_v2 = {
        .name        = "Kia V2",
        .modulation  = FSK_PULSE_PCM,
        .short_width = 500,
        .long_width  = 500,
        .reset_limit = 2000,
        .decode_fn   = &kia_v2_decode,
        .fields      = output_fields,
};

