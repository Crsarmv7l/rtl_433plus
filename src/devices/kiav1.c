/** @file
    Decoder for Kia V1
    
    By L0rdDiakon
    The Pirates Plunder
    https://discord.gg/UYUYGUk8

*/

#include "decoder.h"

#define NUM_BITS_PREAMBLE_v1 32
#define NUM_BYTES_DATA_v1    16

uint8_t const preamble_v1[] = {0xcc, 0xcc, 0xcc, 0xcd};

static int kia_v1_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{   
    
    int available_bits = bitbuffer->bits_per_row[0];

    //length abort is only the preamble bits.
    //Actual signal should be longer
    if (available_bits < 354) {
        return DECODE_ABORT_EARLY; 
    }

    // sync on preamble
    unsigned start_pos = bitbuffer_search(bitbuffer, 0, 0, preamble_v1, NUM_BITS_PREAMBLE_v1);
    available_bits -= start_pos;

    //Signal shouldn't need longer than the num bytes data
    //as 8 bytes decoded = 16 bytes before decode
    int total_bits = (NUM_BYTES_DATA_v1 * 8);
    
    //Abort if preamble not found and/or not enough to extract
    if ((available_bits < NUM_BITS_PREAMBLE_v1) || (available_bits < total_bits)) {
        return DECODE_FAIL_SANITY; 
    }
    
    uint8_t b[NUM_BYTES_DATA_v1];
    
    //Extract payload bytes start at preamble -1 bit to align manchester
    bitbuffer_extract_bytes(bitbuffer, 0, start_pos + NUM_BITS_PREAMBLE_v1 -1, b, total_bits);

    int bit_count = 56;
    uint64_t final = 0;
    size_t j = 0;
    while (bit_count > 0) {
        for (int i = 3; i >= 0; i--) {
            uint8_t two_bits = (b[j] >> (i * 2)) & 0x03;
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

    uint32_t serial = (uint32_t)((final & 0xFFFFFFFF000000) >> 24);
    uint8_t btn = (uint8_t)((final & 0x00000000FF0000) >> 16);                            
    uint8_t count = (uint8_t)((final & 0x0000000000FF00) >> 8);
    uint8_t crc = (uint8_t)(final & 0x000000000000FF);
    
    char key_str[17];
    snprintf(key_str, sizeof(key_str), "%08lX", final);
    char count_str[3];
    snprintf(count_str, sizeof(count_str), "%02X", count);
    char serial_str[9];
    snprintf(serial_str, sizeof(serial_str), "%08X", serial);
    char btn_str[3];
    snprintf(btn_str, sizeof(btn_str), "%X", btn);
    char crc_str[3];
    snprintf(crc_str, sizeof(crc_str), "%02X", crc);

     /* clang-format off */
    data_t *data = data_make(
            "model",            "",             DATA_STRING,    "Kia V1",
            "id",              "serial",        DATA_STRING,    serial_str,
            "key",              "",             DATA_STRING,    key_str,
            "btn",              "",             DATA_STRING,    btn_str,
            "count",            "",             DATA_STRING,    count_str,
            "crc",              "",             DATA_STRING,    crc_str,
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

r_device const kia_v1 = {
        .name        = "Kia V1",
        .modulation  = OOK_PULSE_PCM,
        .short_width = 800,
        .long_width  = 800,
        .reset_limit = 3000,
        .decode_fn   = &kia_v1_decode,
        .fields      = output_fields,
};
