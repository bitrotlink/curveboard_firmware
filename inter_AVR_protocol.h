typedef struct { //Keyboard report.
        uint8_t mod_keys;
        uint8_t reserved;
        uint8_t nonmod_keys[6];
        uint8_t report_sequence_number;
        uint8_t try_sequence_number;
        uint8_t fletcher16_checksum[2];
} kr_t;

