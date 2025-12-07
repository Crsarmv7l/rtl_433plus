/** @file
    Decoder for Kia V0
    
    By L0rdDiakon
    The Pirates Plunder
    https://discord.gg/UYUYGUk8

*/

#include "decoder.h"

#define NUM_BITS_PREAMBLE_v0 32
#define NUM_BYTES_DATA_v0    25

uint8_t const preamble_v0[] = {0xaa, 0xaa, 0xcc, 0xcc};

static int kia_v0_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    //TODO iterate through each row and attempt
    int available_bits = bitbuffer->bits_per_row[0];

    //length abort is only the preamble bits.
    //Actual signal should be at least +100 bits
    if (available_bits < 670) {
        return DECODE_ABORT_EARLY; 
    }

    // sync on preamble
    unsigned start_pos = bitbuffer_search(bitbuffer, 0, 0, preamble_v0, NUM_BITS_PREAMBLE_v0);
    available_bits -= start_pos;

    //Abort if preamble not found
    if (available_bits < NUM_BITS_PREAMBLE_v0) {
        return DECODE_FAIL_SANITY; 
    }

    //find close enough end position
    unsigned end_pos = bitbuffer_search(bitbuffer, 0, start_pos + NUM_BITS_PREAMBLE_v0, preamble_v0, NUM_BITS_PREAMBLE_v0);
    int total_bits = (end_pos - (start_pos + NUM_BITS_PREAMBLE_v0));
    
    //Abort if not enough to extract
    if (available_bits < total_bits) {
        return DECODE_FAIL_SANITY; 
    }
    
    uint8_t b[NUM_BYTES_DATA_v0];
    //Extract payload bytes, size varies so take more than we need
    bitbuffer_extract_bytes(bitbuffer, 0, start_pos + NUM_BITS_PREAMBLE_v0, b, total_bits);

    int bit_count = 56;
    uint64_t final = 0;
    size_t j = 0;
    while (bit_count > 0) {
        for (int i = 3; i >= 0; i--) {
            uint8_t two_bits = (b[j] >> (i * 2)) & 0x03;
            if (two_bits == 0x3) {
                bit_count-=1;
                final |= 1ULL << (bit_count);
            }
            else if (two_bits == 0x2) {
                bit_count-=1;
            }
            else if (two_bits != 0x0) {
                //bail if illegal value
                return DECODE_FAIL_SANITY;
            }
            if (bit_count <= 0) {
                break;
            }
        }
        j++;
    }
                                           
    uint32_t count = (uint32_t)((final & 0xFFFF0000000000)>>40);
    uint32_t serial = (uint32_t)((final & 0x0000FFFFFFF000)>>12);
    uint8_t btn = (uint8_t)((final & 0x00000000000F00) >>8);
    uint8_t crc = (uint8_t)(final & 0x000000000000FF);
    
    char key_str[17];
    snprintf(key_str, sizeof(key_str), "0F%08lX", final);
    char count_str[5];
    snprintf(count_str, sizeof(count_str), "%04X", count);
    char serial_str[8];
    snprintf(serial_str, sizeof(serial_str), "%07X", serial);
    char btn_str[2];
    snprintf(btn_str, sizeof(btn_str), "%01X", btn);
    char crc_str[3];
    snprintf(crc_str, sizeof(crc_str), "%02X", crc);
    
    /* clang-format off */
    data_t *data = data_make(
            "model",            "",             DATA_STRING,    "Kia V0",
            "id",              "serial",        DATA_STRING,    serial_str,
            "flipper key",      "",             DATA_STRING,    key_str,
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
        "flipper key",
        "btn",
        "count",
        "crc",
        NULL,
};

r_device const kia_v0 = {
        .name        = "Kia V0",
        .modulation  = FSK_PULSE_PCM,
        .short_width = 260, 
        .long_width  = 260, 
        .reset_limit = 4096,
        .decode_fn   = &kia_v0_decode,
        .fields      = output_fields,
};
