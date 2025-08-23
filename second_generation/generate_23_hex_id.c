// =============================
// 23 HEX ID Generation (Appendix B.2 Compliant)
// =============================

void generate_23hex_id(const uint8_t *frame_202bits, char *hex_id) {
    uint8_t id_bits[92] = {0};
    int pos = 0;

    // Bits fixes (1,12,13,14)
    id_bits[pos++] = 1;  // Bit 1: fixed '1'
    
    // Country Code (bits 31-40)
    for (int i = 30; i < 40; i++) {  // Bits 31-40 (index 30-39)
        id_bits[pos++] = frame_202bits[i];
    }
    
    // Bits fixes (suite)
    id_bits[pos++] = 1;  // Bit 12: fixed '1'
    id_bits[pos++] = 0;  // Bit 13: fixed '0'
    id_bits[pos++] = 1;  // Bit 14: fixed '1'
    
    // TAC Number (bits 1-16)
    for (int i = 0; i < 16; i++) {  // Bits 1-16 (index 0-15)
        id_bits[pos++] = frame_202bits[i];
    }
    
    // Serial Number (bits 17-30)
    for (int i = 16; i < 30; i++) {  // Bits 17-30 (index 16-29)
        id_bits[pos++] = frame_202bits[i];
    }
    
    // Test Protocol (bit 43)
    id_bits[pos++] = frame_202bits[42];  // Bit 43 (index 42)
    
    // Beacon Type (bits 138-140)
    for (int i = 137; i < 140; i++) {  // Bits 138-140 (index 137-139)
        id_bits[pos++] = frame_202bits[i];
    }
    
    // Vessel ID (bits 91-134 - 44 premiers bits)
    for (int i = 90; i < 134; i++) {  // Bits 91-134 (index 90-133)
        id_bits[pos++] = frame_202bits[i];
    }

    // Conversion binaire -> hexadécimal
    for (int i = 0; i < 23; i++) {
        uint8_t nibble = 0;
        for (int j = 0; j < 4; j++) {
            nibble = (nibble << 1) | id_bits[i*4 + j];
        }
        hex_id[i] = "0123456789ABCDEF"[nibble];
    }
    hex_id[23] = '\0';  // Terminaison de chaîne
}