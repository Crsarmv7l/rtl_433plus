/** @file
    Decoder for Subaru
    
    By L0rdDiakon & Wootini
    Based on analysis by Wootini

*/

#include "decoder.h"

inline void subaru_count(const uint8_t *KB, uint32_t *count)
{
    uint8_t lo = 0;
    if ((KB[4] & 0x40) == 0) lo |= 0x01;
    if ((KB[4] & 0x80) == 0) lo |= 0x02;
    if ((KB[5] & 0x01) == 0) lo |= 0x04;
    if ((KB[5] & 0x02) == 0) lo |= 0x08;
    if ((KB[6] & 0x01) == 0) lo |= 0x10;
    if ((KB[6] & 0x02) == 0) lo |= 0x20;
    if ((KB[5] & 0x40) == 0) lo |= 0x40;
    if ((KB[5] & 0x80) == 0) lo |= 0x80;

    uint8_t REG_SH0 = (KB[4] << 2) & 0xFF;
    if (KB[5] & 0x10) REG_SH0 |= 0x01;
    if (KB[5] & 0x20) REG_SH0 |= 0x02;

    uint8_t REG_SH1 = (KB[7] << 4) & 0xF0;
    if (KB[5] & 0x04) REG_SH1 |= 0x04;
    if (KB[5] & 0x08) REG_SH1 |= 0x08;
    if (KB[6] & 0x80) REG_SH1 |= 0x02;
    if (KB[6] & 0x40) REG_SH1 |= 0x01;

    uint8_t REG_SH2 = ((KB[6] << 2) & 0xF0) | ((KB[7] >> 4) & 0x0F);

    uint8_t SER0 = KB[3];
    uint8_t SER1 = KB[1];
    uint8_t SER2 = KB[2];

    uint8_t total_rot = 4 + lo;
    for (uint8_t i = 0; i < total_rot; ++i) {
        uint8_t t_bit = (SER0 >> 7) & 1;
        SER0 = ((SER0 << 1) & 0xFE) | ((SER1 >> 7) & 1);
        SER1 = ((SER1 << 1) & 0xFE) | ((SER2 >> 7) & 1);
        SER2 = ((SER2 << 1) & 0xFE) | t_bit;
    }

    uint8_t T1 = SER1 ^ REG_SH1;
    uint8_t T2 = SER2 ^ REG_SH2;

    uint8_t hi = 0;
    if ((T1 & 0x10) == 0) hi |= 0x04;
    if ((T1 & 0x20) == 0) hi |= 0x08;
    if ((T2 & 0x80) == 0) hi |= 0x02;
    if ((T2 & 0x40) == 0) hi |= 0x01;
    if ((T1 & 0x01) == 0) hi |= 0x40;
    if ((T1 & 0x02) == 0) hi |= 0x80;
    if ((T2 & 0x08) == 0) hi |= 0x20;
    if ((T2 & 0x04) == 0) hi |= 0x10;

    *count = ((hi << 8) | lo) & 0xFFFF;
}


static int subaru_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{   
    uint8_t *b = bitbuffer->bb[0];

    //timing params and PPM do the heavy lifting

    //check partial preamble in bitbuffer 0
    if (b[1] != 0xff && b[2] != 0xff && b[3] != 0xff && b[4] != 0xff && b[5] != 0xff && b[6] != 0xff) {
         return DECODE_FAIL_SANITY;
    }

    //check data length is good in bitbuffer 1, handle common rx length
    if ((bitbuffer->bits_per_row[1] != 64) && (bitbuffer->bits_per_row[1] != 65)) {
        return DECODE_FAIL_SANITY;
    }

    uint8_t b_data[8];
    
    //Extract payload bytes 
    bitbuffer_extract_bytes(bitbuffer, 1, 0, b_data, 64);

    uint32_t count;
    subaru_count(b_data, &count);
    
    uint64_t key     = ((uint64_t)b_data[0] << 56) | ((uint64_t)b_data[1] << 48) | ((uint64_t)b_data[2] << 40) |
                       ((uint64_t)b_data[3] << 32) | ((uint64_t)b_data[4] << 24) | ((uint64_t)b_data[5] << 16) |
                       ((uint64_t)b_data[6] << 8)  | ((uint64_t)b_data[7]);
    uint32_t serial  = (b_data[1] << 16) | (b_data[2] << 8) | b_data[3];
    uint8_t btn      = (b_data[0] & 0x0F);    
    
    char key_str[17];
    snprintf(key_str, sizeof(key_str), "%08lX", key);
    char count_str[5];
    snprintf(count_str, sizeof(count_str), "%03X", count);
    char serial_str[9];
    snprintf(serial_str, sizeof(serial_str), "%06X", serial);
    
         /* clang-format off */
    data_t *data = data_make(
            "model",            "",             DATA_STRING,    "Subaru",
            "id",              "serial",        DATA_STRING,    serial_str,
            "key",              "",             DATA_STRING,    key_str,
            "btn",              "",   DATA_FORMAT, "%01X", DATA_INT, (unsigned)(btn),
            "count",            "",             DATA_STRING,    count_str,
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
        NULL,
};

r_device const subaru = {
        .name        = "Subaru",
        .modulation  = OOK_PULSE_PPM,
        .short_width = 600,  
        .long_width  = 1200, 
        .gap_limit   = 2000, 
        .reset_limit = 6000,
        .decode_fn   = &subaru_decode,
        .fields      = output_fields,
};
