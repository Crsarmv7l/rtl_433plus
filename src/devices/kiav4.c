/** @file
    Decoder for Kia V3/4
    
    By L0rdDiakon
    The Pirates Plunder
    https://discord.gg/UYUYGUk8

    v3/4 uses keeloq keys which I am not going to distribute
    decoding will require the user to put in keys
*/

#include "decoder.h"
                            //V4     V3
uint64_t kia_3_4_mf_keys[] = {0x00, 0x00};

static int kia_v4_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{   
    uint8_t *b;
    //Handle variations
    if (bitbuffer->bits_per_row[0] == 0) {
        //Abort if too short
        if (bitbuffer->bits_per_row[1] != 10 || bitbuffer->bits_per_row[2] < 64 || bitbuffer->bits_per_row[2] > 80) {
            return DECODE_ABORT_LENGTH;
        }

        uint8_t *a = bitbuffer->bb[1];
        // Reject codes with an incorrect preamble (expected 0xffc0)
        if (a[0] != 0xff || a[1] != 0xc0) {
            decoder_log(decoder, 2, __func__, "Preamble not found");
            return DECODE_ABORT_EARLY;
        }
        bitbuffer_invert(bitbuffer);
        b = bitbuffer->bb[2];
    }
    //Second common rx has partial preamble in bb[0] eg codes : {11}7fe, {79}9563e7f99f48fc7df7fe
    else if (bitbuffer->bits_per_row[0] == 11) {
            if (bitbuffer->bits_per_row[1] < 64 || bitbuffer->bits_per_row[1] > 80) {
                return DECODE_ABORT_LENGTH;
            }
        
            uint8_t *a = bitbuffer->bb[0];
            // Reject codes with an incorrect partial preamble (expected 0x7fe0)
            if (a[0] != 0x7f || a[1] != 0xe0) {
                decoder_log(decoder, 2, __func__, "Preamble not found2");
                return DECODE_ABORT_EARLY;
            }
            bitbuffer_invert(bitbuffer);
            b = bitbuffer->bb[1];
    }
    else {
        return DECODE_ABORT_EARLY;
    }
    
    // The transmission is LSB first, big endian.
    uint32_t encrypted = ((unsigned)reverse8(b[3]) << 24) | (reverse8(b[2]) << 16) | (reverse8(b[1]) << 8) | (reverse8(b[0]));
    uint64_t key = ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) | ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
                   ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) | ((uint64_t)b[6] << 8)  | (uint64_t)b[7];
    uint64_t yek = ((uint64_t)reverse8(b[7]) << 56) | ((uint64_t)reverse8(b[6]) << 48) | ((uint64_t)reverse8(b[5]) << 40) | ((uint64_t)reverse8(b[4]) << 32) |
                   ((uint64_t)reverse8(b[3]) << 24) | ((uint64_t)reverse8(b[2]) << 16) | ((uint64_t)reverse8(b[1]) << 8)  | (uint64_t)reverse8(b[0]);
    int serial         = (reverse8(b[7] & 0xf0) << 24) | (reverse8(b[6]) << 16) | (reverse8(b[5]) << 8) | (reverse8(b[4]));
    int btn            = ((reverse8(b[7]) & 0xF0) >> 4);

    char key_str[18];
    snprintf(key_str, sizeof(key_str), "%08lX", key);
    char yek_str[18];
    snprintf(yek_str, sizeof(yek_str), "%08lX", yek);
    char encrypted_str[9];
    snprintf(encrypted_str, sizeof(encrypted_str), "%08X", encrypted);
    char serial_str[9];
    snprintf(serial_str, sizeof(serial_str), "%07X", serial);
    char model_str[] = "Kia 3/4";
    char decrypted_str[12];
    snprintf(decrypted_str, sizeof(decrypted_str), "%s", "Unknown");
    char mfkey_str[18];
    snprintf(mfkey_str, sizeof(mfkey_str), "%s", "Unknown");

    if (kia_3_4_mf_keys[0] != 0x00) {
        for (size_t j = 0; j < 2; j++){ 
            uint32_t block = keeloq_common_decrypt(encrypted, kia_3_4_mf_keys[j]);
            if (((uint8_t)(btn) == (uint8_t)(block >> 28)) && ((uint8_t)(serial & 0x00000FF)) == ((uint8_t)((block & 0x00FF0000) >> 16))) {
                snprintf(decrypted_str, sizeof(decrypted_str), "%08X", block);
                snprintf(mfkey_str, sizeof(mfkey_str), "%08lX", kia_3_4_mf_keys[j]);
            
                if (kia_3_4_mf_keys[j] == kia_3_4_mf_keys[0]) {
                    snprintf(model_str, sizeof(model_str), "%s", "Kia V4");
                }
                else {
                    snprintf(model_str, sizeof(model_str), "%s", "Kia V3");
                }
                break;
            }
        }
    }
    
     /* clang-format off */
    data_t *data = data_make(
            "model",            "",             DATA_STRING,    model_str,
            "key",              "",             DATA_STRING,    key_str,
            "id",        "encrypted",           DATA_STRING,    encrypted_str,
            "button",           "",             DATA_INT,       btn,
            "yek",              "",             DATA_STRING,    yek_str,
            "serial",           "",             DATA_STRING,    serial_str,
            "decrypted",        "",             DATA_STRING,    decrypted_str,
            "mfkey",            "",             DATA_STRING,    mfkey_str,
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
        "mfkey",
        NULL,
};

//Don't think v4 is ever OOK
//but might as well check
r_device const kia_v4 = {
        .name        = "Kia V4",
        .modulation  = OOK_PULSE_PWM,
        .short_width = 370,
        .long_width  = 772,
        .gap_limit   = 1500,
        .reset_limit = 9000,
        .tolerance   = 152, // us
        .decode_fn   = &kia_v4_decode,
        .fields      = output_fields,
};

r_device const kia_v4_fsk = {
        .name        = "Kia V4 (FSK)",
        .modulation  = FSK_PULSE_PWM,
        .short_width = 370,
        .long_width  = 772,
        .gap_limit   = 1500,
        .reset_limit = 9000,
        .tolerance   = 152, // us
        .decode_fn   = &kia_v4_decode,
        .fields      = output_fields,
};
