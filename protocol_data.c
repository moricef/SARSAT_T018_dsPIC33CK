/* protocol_data.c
 * T018 2nd Generation Protocol Data Implementation
 * Consolidates: beacon_2g_main.c + beacon_utils.c + rotating_field_config.c
 */

#include "protocol_data.h"
#include "system_debug.h"
#include "error_correction.h"
#include "system_comms.h"
#include "rf_interface.h"

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Frame buffers
uint8_t beacon_frame_2g[252] = {0};
uint8_t frame_2g_info[202] = {0};

// Beacon configuration - EPIRB maritime au large de Marseille
beacon_config_2g_t beacon_config_2g = {
    .generation = 2,
    .test_mode = 1,              // Test mode
    .rotating_type = RF_TYPE_G008_2G,
    .beacon_id = 13398,          // Serial number
    .country_code = 228,         // France MID 228
    .protocol_code = 1,          // EPIRB
    .vessel_id = 227006600       // MMSI example (France)
};

// ELT state
elt_state_2g_t elt_state_2g = {
    .current_phase = ELT_PHASE_1,
    .transmission_count = 0,
    .last_tx_time = 0,
    .phase_start_time = 0,
    .active = 0
};

// System state
uint32_t system_time_2g = 0;
uint32_t last_update_2g = 0;
uint32_t activation_time_2g = 0;  // Time of beacon activation
float current_latitude_2g = 0.0;     // TEST: Equator
float current_longitude_2g = 0.0;    // TEST: Prime meridian
float current_altitude_2g = 0.0;

// =============================================================================
// FRAME BUILDING FUNCTIONS
// =============================================================================

// Helper function to write multi-bit values into bit array
static void write_bits(uint8_t* bit_array, int start_pos, int num_bits, uint64_t value) {
    for(int i = num_bits - 1; i >= 0; i--) {
        bit_array[start_pos++] = (value >> i) & 1;
    }
}

void build_2g_information_field(uint8_t* info_bits) {
    memset(info_bits, 0, 202);

    DEBUG_LOG_FLUSH("Building 2G information field...\r\n");

    // Standard T.018 format (202 bits)
    int bit_pos = 0;

    // Bits 1-16: TAC (16 bits)
    uint16_t tac = (beacon_config_2g.test_mode) ? 9999 : 10001;
    write_bits(info_bits, bit_pos, 16, tac);
    bit_pos += 16;

    // Bits 17-30: Serial number (14 bits)
    uint16_t serial = beacon_config_2g.beacon_id & 0x3FFF;
    write_bits(info_bits, bit_pos, 14, serial);
    bit_pos += 14;

    // Bits 31-40: Country code (10 bits)
    uint16_t country = beacon_config_2g.country_code & 0x3FF;
    write_bits(info_bits, bit_pos, 10, country);
    bit_pos += 10;

    // Bit 41: Homing device status (0 = not equipped/disabled)
    info_bits[bit_pos++] = 0;

    // Bit 42: RLS capability (1 = enabled)
    info_bits[bit_pos++] = 1;

    // Bit 43: Test protocol flag
    info_bits[bit_pos++] = beacon_config_2g.test_mode ? 1 : 0;

    // Bits 44-66: Latitude (23 bits) per T.018 Appendix C
    gps_data_t* gps = get_current_gps_data();
    float lat = (gps && gps->valid) ? gps->latitude : current_latitude_2g;
    float lon = (gps && gps->valid) ? gps->longitude : current_longitude_2g;

    // Bit 44: N/S flag (N=0, S=1)
    info_bits[bit_pos++] = (lat < 0) ? 1 : 0;

    // Bits 45-51: Degrees (7 bits, 0-90)
    uint8_t lat_degrees = (uint8_t)(lat < 0 ? -lat : lat);
    write_bits(info_bits, bit_pos, 7, lat_degrees);
    bit_pos += 7;

    // Bits 52-66: Decimal parts (15 bits)
    float lat_decimal = (lat < 0 ? -lat : lat) - lat_degrees;
    uint16_t lat_decimal_encoded = (uint16_t)(lat_decimal * (1 << 15) + 0.5);  // Round
    write_bits(info_bits, bit_pos, 15, lat_decimal_encoded);
    bit_pos += 15;

    // Bits 67-90: Longitude (24 bits) per T.018 Appendix C
    // Bit 67: E/W flag (E=0, W=1)
    info_bits[bit_pos++] = (lon < 0) ? 1 : 0;

    // Bits 68-75: Degrees (8 bits, 0-180)
    uint8_t lon_degrees = (uint8_t)(lon < 0 ? -lon : lon);
    write_bits(info_bits, bit_pos, 8, lon_degrees);
    bit_pos += 8;

    // Bits 76-90: Decimal parts (15 bits)
    float lon_decimal = (lon < 0 ? -lon : lon) - lon_degrees;
    uint16_t lon_decimal_encoded = (uint16_t)(lon_decimal * (1 << 15) + 0.5);  // Round
    write_bits(info_bits, bit_pos, 15, lon_decimal_encoded);
    bit_pos += 15;

    // Bits 91-93: Vessel ID type (3 bits) - 1=Maritime MMSI (001)
    write_bits(info_bits, bit_pos, 3, 1);
    bit_pos += 3;

    // Bits 94-123: MMSI (30 bits) for maritime
    uint32_t mmsi = beacon_config_2g.vessel_id & 0x3FFFFFFF;  // 30 bits max
    write_bits(info_bits, bit_pos, 30, mmsi);
    bit_pos += 30;

    // Bits 124-137: EPIRB-AIS System Identity (14 bits) - spare for basic implementation
    write_bits(info_bits, bit_pos, 14, 0);
    bit_pos += 14;

    // Bits 138-140: Beacon type (3 bits) - 1=EPIRB
    write_bits(info_bits, bit_pos, 3, beacon_config_2g.protocol_code);
    bit_pos += 3;

    // Bits 141-154: Spare bits (14 bits) - all 1s unless cancellation
    uint16_t spare_bits = (beacon_config_2g.rotating_type == RF_TYPE_CANCEL_2G) ? 0x3FFF : 0x3FFF;
    write_bits(info_bits, bit_pos, 14, spare_bits);
    bit_pos += 14;

    // Bits 155-202: Rotating Field (48 bits)
    set_rotating_field_2g(info_bits, beacon_config_2g.rotating_type);

    DEBUG_LOG_FLUSH("2G information field built\r\n");
}

void build_2g_complete_frame(uint8_t* info_bits, uint8_t* complete_frame) {
    memset(complete_frame, 0, 252);
    
    // PRN header (2 bits)
    complete_frame[0] = (beacon_config_2g.test_mode) ? 1 : 0;
    complete_frame[1] = 0;  // Padding bit
    
    // Copy information bits (202 bits)
    memcpy(&complete_frame[2], info_bits, 202);
    
    // Calculate and append BCH parity (48 bits)
    uint64_t bch_parity = compute_bch_250_202(info_bits);
    for(int i = 0; i < 48; i++) {
        complete_frame[204 + i] = (bch_parity >> (47 - i)) & 1;
    }
}

void build_compliant_frame_2g(void) {
    // Build information field
    build_2g_information_field(frame_2g_info);

    // Build complete frame with BCH
    build_2g_complete_frame(frame_2g_info, beacon_frame_2g);

    // Extract Short 15 HEX ID from frame (bits 2-43 = 42 bits TAC+Serial+Country)
    DEBUG_LOG_FLUSH("Frame built - 15 HEX ID: ");
    for(int i = 0; i < 11; i++) {  // 42 bits = 10.5 hex chars, round to 11
        // Extract 4 bits starting at bit position 2 + i*4
        uint8_t nibble = 0;
        for(int j = 0; j < 4; j++) {
            if((2 + i*4 + j) < 252 && beacon_frame_2g[2 + i*4 + j]) {
                nibble |= (1 << (3 - j));
            }
        }
        char hex_char = "0123456789ABCDEF"[nibble];
        debug_print_char(hex_char);
    }
    DEBUG_LOG_FLUSH("\r\n");

    // Display complete 252-bit frame in hexadecimal (63 hex chars)
    DEBUG_LOG_FLUSH("Complete frame (252 bits): ");
    for(int i = 0; i < 63; i++) {
        // Extract 4 bits starting at bit position i*4
        uint8_t nibble = 0;
        for(int j = 0; j < 4; j++) {
            if(beacon_frame_2g[i*4 + j]) {
                nibble |= (1 << (3 - j));
            }
        }
        char hex_char = "0123456789ABCDEF"[nibble];
        debug_print_char(hex_char);
    }
    DEBUG_LOG_FLUSH("\r\n");
}

// =============================================================================
// FRAME COMPONENT FUNCTIONS
// =============================================================================

void set_rotating_field_2g(uint8_t* info_bits, rotating_field_type_2g_t rf_type) {
    rotating_field_data_2g_t rf_data = {0};
    prepare_rotating_field_data_2g(&rf_data);

    // Set rotating field type identifier (4 bits) at position 154
    write_bits(info_bits, 154, 4, rf_type);

    switch(rf_type) {
        case RF_TYPE_G008_2G:
            // T.018 G.008 Objective Requirements rotating field
            {
                uint8_t elapsed_hours = get_elapsed_activation_hours_2g();
                uint16_t last_pos_minutes = get_time_since_last_location_minutes_2g();

                write_bits(info_bits, 158, 6, elapsed_hours);           // T.018 bits 159-164
                write_bits(info_bits, 164, 11, last_pos_minutes);       // T.018 bits 165-175
                write_bits(info_bits, 175, 10, rf_data.altitude_code);  // T.018 bits 176-185
                write_bits(info_bits, 185, 17, 0);                      // T.018 bits 186-202
            }
            break;

        case RF_TYPE_ELTDT_2G:
            // ELT-DT Time/Altitude data (44 bits)
            write_bits(info_bits, 158, 16, rf_data.time_value);
            write_bits(info_bits, 174, 10, rf_data.altitude_code);
            write_bits(info_bits, 184, 18, 0);  // Spare bits
            break;

        case RF_TYPE_RLS_2G:
            // RLS provider and data
            write_bits(info_bits, 158, 8, rf_data.rls_provider);
            write_bits(info_bits, 166, 36, rf_data.rls_data);
            break;

        case RF_TYPE_CANCEL_2G:
            // Cancellation method
            write_bits(info_bits, 158, 2, rf_data.deactivation_method);
            // Fixed bits - all 42 bits set to 1 (T.018 spec)
            write_bits(info_bits, 160, 42, 0x3FFFFFFFFFFULL);
            break;
    }
}

// =============================================================================
// GPS POSITION ENCODING
// =============================================================================

uint64_t encode_gps_position_2g(double lat, double lon) {
    int32_t lat_encoded = (int32_t)((lat + 90.0) * (1L << 23) / 180.0);
    int32_t lon_encoded = (int32_t)((lon + 180.0) * (1L << 24) / 360.0);

    lat_encoded &= 0x7FFFFF;  // 23 bits
    lon_encoded &= 0xFFFFFF;  // 24 bits

    return ((uint64_t)lat_encoded << 24) | lon_encoded;
}

// =============================================================================
// DYNAMIC FIELD FUNCTIONS
// =============================================================================

uint16_t get_last_location_time_2g(void) {
    return (uint16_t)((system_time_2g - last_update_2g) / 60);
}

uint16_t altitude_to_code_2g(double altitude) {
    if(altitude <= -400.0) {
        return 0;
    }
    if(altitude > 15952.0) {
        return 1022;
    }
    // T.018 encoding: (altitude + 400) / 16 + 0.0625 (code 25 for 0m)
    int16_t encoded = (int16_t)((altitude + 400.0) / 16.0 + 0.0625 + 0.5);
    return (uint16_t)(encoded & 0x3FF);
}

uint8_t encode_altitude_2g(float altitude_m) {
    return (uint8_t)(altitude_to_code_2g(altitude_m) & 0xFF);
}

uint32_t encode_time_value_2g(uint8_t day, uint8_t hour, uint8_t minute, uint8_t type) {
    switch(type) {
        case RF_TYPE_G008_2G:
        case RF_TYPE_ELTDT_2G:
            return ((day & 0x1F) << 11) | ((hour & 0x1F) << 6) | (minute & 0x3F);
        default:
            return 0;
    }
}

// =============================================================================
// CONFIGURATION FUNCTIONS
// =============================================================================

void load_beacon_configuration_2g(void) {
    // TODO: Read from EEPROM/Flash
    DEBUG_LOG_FLUSH("Loading beacon configuration...\r\n");
    // For now, use defaults already set
}

beacon_config_2g_t* get_beacon_config_2g(void) {
    return &beacon_config_2g;
}

beacon_frame_type_2g_t get_frame_type_from_switch_2g(void) {
    // Read mode switch (0=TEST, 1=EXERCISE)
    return (MODE_SWITCH_PORT) ? BEACON_EXERCISE_FRAME_2G : BEACON_TEST_FRAME_2G;
}

uint8_t get_beacon_mode_2g(void) {
    return MODE_SWITCH_PORT;  // 0=TEST, 1=EXERCISE
}

// =============================================================================
// ELT SEQUENCE MANAGEMENT
// =============================================================================

void start_elt_sequence_2g(void) {
    elt_state_2g.active = 1;
    elt_state_2g.current_phase = ELT_PHASE_1;
    elt_state_2g.transmission_count = 0;
    elt_state_2g.last_tx_time = 0;
    elt_state_2g.phase_start_time = get_system_time_ms();
    
    DEBUG_LOG_FLUSH("ELT sequence started - Phase 1 (5s intervals)\r\n");
}

void stop_elt_sequence_2g(void) {
    elt_state_2g.active = 0;
    DEBUG_LOG_FLUSH("ELT sequence stopped\r\n");
}

uint32_t get_current_interval_2g(void) {
    switch(elt_state_2g.current_phase) {
        case ELT_PHASE_1:
            return ELT_PHASE1_INTERVAL;  // 5 seconds
        case ELT_PHASE_2:
            return ELT_PHASE2_INTERVAL;  // 10 seconds
        case ELT_PHASE_3:
            // 28.5s ±1.5s randomization
            return ELT_PHASE3_INTERVAL + (rand() % (ELT_PHASE3_RANDOM * 2)) - ELT_PHASE3_RANDOM;
        default:
            return TEST_INTERVAL;       // 10 seconds for test
    }
}

void check_phase_transition_2g(void) {
    switch(elt_state_2g.current_phase) {
        case ELT_PHASE_1:
            if(elt_state_2g.transmission_count >= ELT_PHASE1_COUNT) {
                elt_state_2g.current_phase = ELT_PHASE_2;
                elt_state_2g.transmission_count = 0;
                elt_state_2g.phase_start_time = get_system_time_ms();
                DEBUG_LOG_FLUSH("ELT Phase 2 started (10s intervals)\r\n");
            }
            break;
            
        case ELT_PHASE_2:
            if(elt_state_2g.transmission_count >= ELT_PHASE2_COUNT) {
                elt_state_2g.current_phase = ELT_PHASE_3;
                elt_state_2g.transmission_count = 0;
                elt_state_2g.phase_start_time = get_system_time_ms();
                DEBUG_LOG_FLUSH("ELT Phase 3 started (28.5s intervals)\r\n");
            }
            break;
            
        case ELT_PHASE_3:
            // Continue indefinitely
            break;
    }
}

// =============================================================================
// ROTATING FIELD CONFIGURATION
// =============================================================================

uint8_t get_elapsed_activation_hours_2g(void) {
    if(activation_time_2g == 0) {
        activation_time_2g = system_time_2g;
    }
    uint32_t elapsed_seconds = system_time_2g - activation_time_2g;
    uint8_t elapsed_hours = (uint8_t)(elapsed_seconds / 3600);
    if(elapsed_hours > 63) elapsed_hours = 63;
    return elapsed_hours;
}

uint16_t get_time_since_last_location_minutes_2g(void) {
    uint32_t elapsed_seconds = system_time_2g - last_update_2g;
    uint16_t elapsed_minutes = (uint16_t)(elapsed_seconds / 60);
    if(elapsed_minutes > 2046) elapsed_minutes = 2046;
    return elapsed_minutes;
}

void init_realistic_test_times(void) {
    static uint8_t initialized = 0;
    if(!initialized) {
        activation_time_2g = system_time_2g - (3 * 3600);  // 3 hours ago
        last_update_2g = system_time_2g - (5 * 60);        // GPS updated 5 minutes ago
        initialized = 1;
        DEBUG_LOG_FLUSH("Realistic ADRASEC test: activation=-3h, GPS=-5min\r\n");
    }
}

void prepare_rotating_field_data_2g(rotating_field_data_2g_t* rf_data) {
    rf_data->field_type = beacon_config_2g.rotating_type;

    // Initialize realistic test times for ADRASEC training
    init_realistic_test_times();

    switch(beacon_config_2g.rotating_type) {
        case RF_TYPE_G008_2G:
        case RF_TYPE_ELTDT_2G:
            {
                gps_data_t* gps = get_current_gps_data();
                if(gps && gps->valid) {
                    rf_data->time_value = encode_time_value_2g(
                        gps->day, gps->hour, gps->minute,
                        beacon_config_2g.rotating_type);
                    rf_data->altitude_code = encode_altitude_2g(gps->altitude);
                } else {
                    rf_data->time_value = 0;
                    rf_data->altitude_code = encode_altitude_2g(current_altitude_2g);
                }
            }
            break;

        case RF_TYPE_RLS_2G:
            rf_data->rls_provider = get_rls_provider_id_2g();
            rf_data->rls_data = get_rls_data_2g();
            break;

        case RF_TYPE_CANCEL_2G:
            rf_data->deactivation_method = get_deactivation_method_2g();
            break;
    }
}

const char* get_rotating_field_name_2g(rotating_field_type_2g_t rf_type) {
    switch(rf_type) {
        case RF_TYPE_G008_2G:   return "G008";
        case RF_TYPE_ELTDT_2G:  return "ELT-DT";
        case RF_TYPE_RLS_2G:    return "RLS";
        case RF_TYPE_CANCEL_2G: return "CANCEL";
        default:                return "UNKNOWN";
    }
}

// =============================================================================
// STUB FUNCTIONS
// =============================================================================

uint8_t get_rls_provider_id_2g(void) {
    return 0;  // Galileo
}

uint64_t get_rls_data_2g(void) {
    return 0;
}

uint8_t get_deactivation_method_2g(void) {
    return 0;  // Manual deactivation
}

uint64_t get_configured_vessel_id_2g(void) {
    return beacon_config_2g.vessel_id;
}