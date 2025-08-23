void build_compliant_frame(void) {
    uint8_t info_bits[202] = {0};  // Bits d'information (1-202)
    char hex_id[24];               // 23 HEX ID + terminateur

    // 1. Section Identification (bits 1-43)
    set_bit_field(info_bits, FRAME_TAC_START, FRAME_TAC_LENGTH, tac_value);
    set_bit_field(info_bits, FRAME_SERIAL_START, FRAME_SERIAL_LENGTH, serial_value);
    set_bit_field(info_bits, FRAME_COUNTRY_START, FRAME_COUNTRY_LENGTH, country_code);
    info_bits[FRAME_HOMING_BIT - 1] = homing_status;
    info_bits[FRAME_RLS_BIT - 1] = rls_function;
    info_bits[FRAME_TEST_BIT - 1] = test_protocol;

    // 2. Position encodée (bits 44-90)
    set_bit_field(info_bits, FRAME_LOCATION_START, FRAME_LOCATION_LENGTH,
                 encode_gps_position(current_latitude, current_longitude));

    // 3. Vessel ID (bits 91-137)
    set_bit_field(info_bits, FRAME_VESSEL_ID_START, FRAME_VESSEL_ID_LENGTH, vessel_id);

    // 4. Beacon Type + Spare (bits 138-154)
    set_bit_field(info_bits, FRAME_BEACON_TYPE_START, FRAME_BEACON_TYPE_LENGTH, beacon_type);
    set_bit_field(info_bits, FRAME_SPARE_START, FRAME_SPARE_LENGTH, 0);  // Spare bits

    // 5. Rotating Field (bits 155-202)
    set_bit_field(info_bits, FRAME_ROTATING_ID_START, FRAME_ROTATING_ID_LENGTH, 0x1);  // "0001"
    set_bit_field(info_bits, FRAME_TIME_START, FRAME_TIME_LENGTH, get_last_location_time());
    set_bit_field(info_bits, FRAME_ALTITUDE_START, FRAME_ALTITUDE_LENGTH, 
                 altitude_to_code(current_altitude));
    set_bit_field(info_bits, FRAME_TRIGGERING_START, FRAME_TRIGGERING_LENGTH, triggering_events);
    set_bit_field(info_bits, FRAME_GNSS_STATUS_START, FRAME_GNSS_STATUS_LENGTH, gnss_status);
    set_bit_field(info_bits, FRAME_BATTERY_START, FRAME_BATTERY_LENGTH, battery_level);
    set_bit_field(info_bits, FRAME_ROTATING_SPARE_START, FRAME_ROTATING_SPARE_LENGTH, 0);

    // 6. Génération 23 HEX ID
    generate_23hex_id(info_bits, hex_id);
    DEBUG_LOG_FLUSH("23 HEX ID: %s\r\n", hex_id);

    // 7. Calcul BCH (250,202)
    uint64_t bch = compute_bch_250_202(info_bits);

    // 8. Construction trame finale (252 bits)
    uint8_t final_frame[252] = {0};
    
    // Entête PRN (2 bits)
    final_frame[0] = (beacon_mode == BEACON_MODE_TEST) ? 1 : 0;
    final_frame[1] = 0;  // Bit de padding
    
    // Copie des informations (202 bits)
    memcpy(&final_frame[2], info_bits, 202);
    
    // Copie du BCH (48 bits)
    for (int i = 0; i < 48; i++) {
        final_frame[204 + i] = (bch >> (47 - i)) & 1;
    }

    // Copie dans le buffer global
    memcpy(beacon_frame, final_frame, 252);
}