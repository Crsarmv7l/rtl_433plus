/** @file
    Decoder for Ford v0
    
    By L0rdDiakon and YoungZ
    
    Once again YougZ to the rescue 
    with extractions for Serial, Btn,
    and Counter python functions!

*/

#include "decoder.h"

static inline void decode_ford_v0(uint64_t key1, uint16_t key2, uint32_t *serial, uint8_t  *button, uint32_t *count) {
    uint8_t buf[13] = {0};
    
    // Load key1 (big-endian uint64_t) into buf[0..7]
    for (int i = 0; i < 8; ++i) {
        buf[i] = (uint8_t)(key1 >> (56 - i * 8));
    }
    
    // Load key2 (big-endian uint16_t) into buf[8..9]
    buf[8] = (uint8_t)(key2 >> 8);
    buf[9] = (uint8_t)(key2 & 0xFF);
    
    
    // --- Parity calculation on buf[8] (BS) ---
    uint8_t tmp = buf[8];
    uint8_t parity = 0;
    uint8_t parity_any = (tmp != 0);
    while (tmp) {
        parity ^= (tmp & 1);
        tmp >>= 1;
    }
    buf[11] = parity_any ? parity : 0;
    
    // Choose XOR byte based on parity
    uint8_t xor_byte;
    uint8_t limit;
    if (buf[11]) {
        xor_byte = buf[7];
        limit = 7;
    } else {
        xor_byte = buf[6];
        limit = 6;
    }
    
    // XOR bytes 1 to (limit-1) with xor_byte
    for (int idx = 1; idx < limit; ++idx) {
        buf[idx] ^= xor_byte;
    }
    
    // Extra XOR on byte7 when parity == 0
    if (buf[11] == 0) {
        buf[7] ^= xor_byte;
    }
    
    // Save original buf[7]
    uint8_t orig_b7 = buf[7];
    
    // Mix byte6 and byte7: split even/odd bits
    buf[7] = (orig_b7 & 0xAA) | (buf[6] & 0x55);
    uint8_t mixed = (buf[6] & 0xAA) | (orig_b7 & 0x55);
    buf[12] = mixed;
    buf[6] = mixed;
    
    uint32_t serial_le = ((uint32_t)buf[1])       |
                         ((uint32_t)buf[2] << 8)  |
                         ((uint32_t)buf[3] << 16) |
                         ((uint32_t)buf[4] << 24);
    
    *serial = ((serial_le & 0xFF) << 24) |
              (((serial_le >> 8) & 0xFF) << 16) |
              (((serial_le >> 16) & 0xFF) << 8) |
              ((serial_le >> 24) & 0xFF);

    *button  = (buf[5] >> 4) & 0x0F;

    *count = ((buf[5] & 0x0F) << 16) |
               (buf[6] << 8) |
               buf[7];
    
}

static int fordremote_callback(r_device *decoder, bitbuffer_t *bitbuffer)
{   
    //preamble is split from data so we can check the preamble length
    //for a full or repeat preamble length
    if (bitbuffer->bits_per_row[0] != 35 && bitbuffer->bits_per_row[0] != 28) {
            return DECODE_ABORT_LENGTH;
    }
    
    //Dont need to extract for a simple test
    uint8_t *b = bitbuffer->bb[0];
    
    //check partial preamble values
    if ((b[0] != 0xff && b[0] != 0x99) || (b[1] != 0x33 && b[1] != 0x99)) {
         return DECODE_FAIL_SANITY;
    }
    
    //check that the full data length is there
    //overall length changes by 1 bit based on parity
    if ((bitbuffer->bits_per_row[1] != 167) && (bitbuffer->bits_per_row[1] != 168)) {
            return DECODE_ABORT_LENGTH;
    }
    
    //don't need to search for data start because I
    //actually split preamble/data well
    uint8_t *raw = bitbuffer->bb[1];

    int bit_count = 64;
    uint64_t final = 0;
    size_t j = 0;
    while (bit_count > 0) {
        for (int i = 3; i >= 0; i--) {
            uint8_t two_bits = (raw[j] >> (i * 2)) & 0x03;
            if (two_bits == 0x2) {
                bit_count-=1;
            }
            else if (two_bits == 0x1) {
                bit_count-=1;
                final |= 1ULL << (bit_count);
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

    //Should be fine as I length checked data before processing
    uint8_t raw_crc_bs[] = {raw[16], raw[17], raw[18], raw[19]};
    uint16_t bs_crc = 0;
    size_t p = 0;
    int crc_count = 16;
    while (crc_count > 0) {
        for (int i = 3; i >= 0; i--) {
            uint8_t two_bits = (raw_crc_bs[p] >> (i * 2)) & 0x03;
            if (two_bits == 0x2) {
                crc_count-=1;
            }
            else if (two_bits == 0x1) {
                crc_count-=1;
                bs_crc |= 1 << (crc_count);
            }
            else {
                //bail if illegal value
                return DECODE_FAIL_SANITY;
            }
            if (crc_count <= 0) {
                break;
            }
        }
        p++;
    }

    uint32_t serial, count;
    uint8_t  button;

    decode_ford_v0(final, bs_crc, &serial, &button, &count);
    
    char key_str[17];
    snprintf(key_str, sizeof(key_str), "%08lX", final);
    char serial_str[9];
    snprintf(serial_str, sizeof(serial_str), "%08X", serial);
    char count_str[9];
    snprintf(count_str, sizeof(count_str), "%08X", count);
    

     /* clang-format off */
    data_t *data = data_make(
            "model",            "",             DATA_STRING,    "Ford V0",
            "id",              "serial",        DATA_STRING,    serial_str,
            "key",              "",             DATA_STRING,    key_str,
            "btn",              "",   DATA_FORMAT, "%02X", DATA_INT, (unsigned)(button),
            "count",            "",             DATA_STRING,    count_str,
            "bs",               "",   DATA_FORMAT, "%02X", DATA_INT, (unsigned)(bs_crc >> 8),
            "crc",              "",   DATA_FORMAT, "%02X", DATA_INT, (unsigned)(bs_crc & 0xFF),
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
        "bs",
        "crc",
        NULL,
};

r_device const fordremote = {
        .name        = "Ford Car Key",
        .modulation  = OOK_PULSE_PCM,
        .short_width = 250, 
        .long_width  = 250,
        .gap_limit   = 2000,
        .reset_limit = 52000,
        .tolerance   = 50,
        .decode_fn   = &fordremote_callback,
        .fields      = output_fields,
};
