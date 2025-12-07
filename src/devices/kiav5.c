/** @file
    Decoder for Kia V5
    
    By L0rdDiakon 
    The Pirates Plunder
    https://discord.gg/UYUYGUk8

    Based on initial bit decoding by myself,
    and complete reimplement of bit mixing
    and all other functions by Yougz. Fanastic work YougZ!
    
*/

#include "decoder.h"

#define NUM_BITS_PREAMBLE_v5 24
#define NUM_BYTES_DATA_v5    17

uint8_t const keystore_bytes[] = {0x53, 0x54, 0x46, 0x52, 0x4b, 0x45, 0x30, 0x30};                              
uint8_t const preamble_v5[] = {0xaa, 0xaa, 0xa6};

inline uint16_t mixer_decode(uint32_t encrypted) {
    uint8_t s0 = (encrypted & 0xFF);
    uint8_t s1 = (encrypted >> 8) & 0xFF;
    uint8_t s2 = (encrypted >> 16) & 0xFF;
    uint8_t s3 = (encrypted >> 24) & 0xFF;

    int round_index = 1;
    for (size_t i = 0; i < 18; i++) {
        uint8_t r = keystore_bytes[round_index] & 0xFF;
        int steps = 8;
        while (steps > 0) {
            uint8_t base;
            if ((s3 & 0x40) == 0) {
                base = (s3 & 0x02) == 0 ? 0x74 : 0x2E;
            } else {
                base = (s3 & 0x02) == 0 ? 0x3A : 0x5C;
            }

            if (s2 & 0x08) {
                base = (((base >> 4) & 0x0F) | ((base & 0x0F) << 4)) & 0xFF;
            }
            if (s1 & 0x01) {
                base = ((base & 0x3F) << 2) & 0xFF;
            }
            if (s0 & 0x01) {
                base = (base << 1) & 0xFF;
            }

            uint8_t temp = (s3 ^ s1) & 0xFF;
            s3 = ((s3 & 0x7F) << 1) & 0xFF;
            if (s2 & 0x80) {
                s3 |= 0x01;
            }
            s2 = ((s2 & 0x7F) << 1) & 0xFF;
            if (s1 & 0x80) {
                s2 |= 0x01;
            }
            s1 = ((s1 & 0x7F) << 1) & 0xFF;
            if (s0 & 0x80) {
                s1 |= 0x01;
            }
            s0 = ((s0 & 0x7F) << 1) & 0xFF;

            uint8_t chk = (base ^ (r ^ temp)) & 0xFF;
            if (chk & 0x80) {
                s0 |= 0x01;
            }
            r = ((r & 0x7F) << 1) & 0xFF;
            steps--;
        }
        round_index = (round_index - 1) & 0x7;
    }
    return (s0 + (s1 << 8)) & 0xFFFF;
}

static int kia_v5_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{   
    int available_bits = bitbuffer->bits_per_row[0];

    //length abort is only the preamble bits.
    //Actual signal should be longer
    if (available_bits < 404) {
        return DECODE_ABORT_EARLY; 
    }

    // sync on preamble
    unsigned start_pos = bitbuffer_search(bitbuffer, 0, 0, preamble_v5, NUM_BITS_PREAMBLE_v5);
    available_bits -= start_pos;
    
    //Signal shouldn't need longer than the num bytes data
    //as 8 bytes decoded = 16 bytes before decode
    int total_bits = (NUM_BYTES_DATA_v5 * 8);
    
    //Abort if preamble not found and/or not enough to extract
    if ((available_bits < NUM_BITS_PREAMBLE_v5) || (available_bits < total_bits)) {
        return DECODE_FAIL_SANITY; 
    }

    uint8_t b[NUM_BYTES_DATA_v5];
    //Extract payload bytes start at preamble +1 bit to align manchester and handle guard bits
    bitbuffer_extract_bytes(bitbuffer, 0, start_pos + NUM_BITS_PREAMBLE_v5 + 1, b, total_bits);
    
    int bit_count = 64;
    uint64_t final = 0;
    size_t j = 0;
    while (bit_count > 0) {
        for (int i = 3; i >= 0; i--) {
            uint8_t two_bits = (b[j] >> (i * 2)) & 0x03;
            if (two_bits == 0x01) {
                bit_count-=1;
                final |= 1ULL << (bit_count);
            }
            else if (two_bits == 0x02) {
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

    //This should be fine as the
    //byte for max crc 0x7 should be 0x95, first two bits
    //are 10 -> 0 leaving us with the 3 bit CRC 
    //rather than only take the 3 bits we will take all 4
    // since first two should always be 0
    int crc_count = 4;
    uint8_t crc = 0;
    for (int i = 3; i >= 0; i--) {
        uint8_t two_bits = (b[16] >> (i * 2)) & 0x03;
        if (two_bits == 0x01) {
            crc_count-=1;
            crc |= 1 << (crc_count);
        }
        else if (two_bits == 0x02) {
            crc_count-=1;
        }
        else {
                //bail if illegal value
                return DECODE_FAIL_SANITY;
        }
    }
    
    uint64_t yek = ((uint64_t)reverse8(final & 0xFF) << 56) |
               ((uint64_t)reverse8((final >> 8) & 0xFF) << 48) |
               ((uint64_t)reverse8((final >> 16) & 0xFF) << 40) |
               ((uint64_t)reverse8((final >> 24) & 0xFF) << 32) |
               ((uint64_t)reverse8((final >> 32) & 0xFF) << 24) |
               ((uint64_t)reverse8((final >> 40) & 0xFF) << 16) |
               ((uint64_t)reverse8((final >> 48) & 0xFF) << 8) |
               (uint64_t)reverse8((final >> 56) & 0xFF);
    uint32_t encrypted = (yek & 0xFFFFFFFF);
    
    int btn    = ((yek >> 60) & 0xF);
    int serial = ((yek >> 32) & 0x0FFFFFFF);
    uint16_t decrypted = mixer_decode(encrypted);
    
    char key_str[18];
    snprintf(key_str, sizeof(key_str), "%08lX", final);
    char yek_str[18];
    snprintf(yek_str, sizeof(yek_str), "%08lX", yek);
    char serial_str[9];
    snprintf(serial_str, sizeof(serial_str), "%07X", serial);
    char encrypted_str[9];
    snprintf(encrypted_str, sizeof(encrypted_str), "%08X", encrypted);
    char decrypted_str[9];
    snprintf(decrypted_str, sizeof(decrypted_str), "%X", decrypted);
    char crc_str[3];
    snprintf(crc_str, sizeof(crc_str), "%X", crc);

    /* clang-format off */
    data_t *data = data_make(
            "model",            "",             DATA_STRING,    "Kia V5",
            "key",              "",             DATA_STRING,    key_str,
            "id",        "encrypted",           DATA_STRING,    encrypted_str,
            "button",           "",             DATA_INT,       btn,
            "yek",              "",             DATA_STRING,    yek_str,
            "serial",           "",             DATA_STRING,    serial_str,
            "decrypted",        "",             DATA_STRING,    decrypted_str,
            "crc",              "",             DATA_STRING,    crc_str,
            NULL);
    /* clang-format on */
    
    decoder_output_data(decoder, data);
    return 1;
}


static char const *const output_fields[] = {
        "model",
        "key",
        "id",
        "button",
        "yek",  
        "serial",      
        "decrypted",
        "crc",
        NULL,
};


r_device const kia_v5_fsk = {
        .name        = "Kia V5 (FSK)",
        .modulation  = FSK_PULSE_PCM,
        .short_width = 400,
        .long_width  = 400,
        .reset_limit = 4096,
        .decode_fn   = &kia_v5_decode,
        .fields      = output_fields,
};
