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

// Beacon configuration
beacon_config_2g_t beacon_config_2g = {
    .generation = 2,
    .test_mode = 0,
    .rotating_type = RF_TYPE_G008_2G,
    .beacon_id = 0x123456,
    .country_code = 228,        // France
    .protocol_code = 2,         // Standard location
    .vessel_id = 0x123456789ABC
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
float current_latitude_2g = 45.1885;   // Grenoble (test position)
float current_longitude_2g = 5.7245;
float current_altitude_2g = 214.0;

// =============================================================================
// FRAME BUILDING FUNCTIONS
// =============================================================================

void build_2g_information_field(uint8_t* info_bits) {
    memset(info_bits, 0, 202);
    
    DEBUG_LOG_FLUSH("Building 2G information field...\r\n");
    
    // Bit allocation per T.018 Appendix E
    // Bits 1-43: TAC + Serial Number + Country Code (23 HEX ID)
    set_23_hex_id_2g(info_bits);
    
    // Bits 44-90: Encoded Location (47 bits)
    gps_data_t* gps = get_current_gps_data();
    if(gps && gps->valid) {
        encode_location_2g(info_bits, gps->latitude, gps->longitude);
    } else {
        // Use fixed test position
        encode_location_2g(info_bits, current_latitude_2g, current_longitude_2g);
    }
    
    // Bits 91-137: Vessel ID (47 bits)
    set_vessel_id_2g(info_bits);
    
    // Bits 138-140: Beacon Type (3 bits)
    set_bit_field(info_bits, 137, 3, beacon_config_2g.protocol_code);
    
    // Bits 141-154: Spare bits (14 bits)
    if(beacon_config_2g.rotating_type == RF_TYPE_CANCEL_2G) {
        set_bit_field(info_bits, 140, 14, 0x3FFF);  // All 1s for cancel
    } else {
        set_bit_field(info_bits, 140, 14, 0);       // All 0s otherwise
    }
    
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
    
    // Generate 23 HEX ID for logging
    char hex_id[24];
    generate_23hex_id_2g(frame_2g_info, hex_id);
    DEBUG_LOG_FLUSH("Frame built - 23 HEX ID: ");
    DEBUG_LOG_FLUSH(hex_id);
    DEBUG_LOG_FLUSH("\r\n");
}

// =============================================================================
// FRAME COMPONENT FUNCTIONS
// =============================================================================

void set_23_hex_id_2g(uint8_t* info_bits) {
    // Format: TAC (16 bits) + Serial (14 bits) + Country (10 bits) + Protocol (3 bits)
    uint64_t hex_id = 0;
    
    // TAC must be > 10000 for real beacons
    uint16_t tac = (beacon_config_2g.test_mode) ? 9999 : 10001;
    
    hex_id |= ((uint64_t)tac & 0xFFFF) << 27;
    hex_id |= ((uint64_t)beacon_config_2g.beacon_id & 0x3FFF) << 13;
    hex_id |= ((uint64_t)beacon_config_2g.country_code & 0x3FF) << 3;
    hex_id |= (beacon_config_2g.protocol_code & 0x7);
    
    set_bit_field(info_bits, 0, 43, hex_id);
}

void encode_location_2g(uint8_t* info_bits, float latitude, float longitude) {
    // Convert to binary encoding per T.018 Appendix C
    // Latitude: 23 bits (-90 to +90 degrees)
    // Longitude: 24 bits (-180 to +180 degrees)
    
    int32_t lat_encoded = (int32_t)((latitude + 90.0) * (1L << 23) / 180.0);
    int32_t lon_encoded = (int32_t)((longitude + 180.0) * (1L << 24) / 360.0);
    
    // Ensure within bounds
    lat_encoded &= 0x7FFFFF;  // 23 bits
    lon_encoded &= 0xFFFFFF;  // 24 bits
    
    // Set in frame (bits 44-90)
    set_bit_field(info_bits, 43, 23, lat_encoded);
    set_bit_field(info_bits, 66, 24, lon_encoded);
}

void set_vessel_id_2g(uint8_t* info_bits) {
    uint64_t vessel_id = get_configured_vessel_id_2g();
    set_bit_field(info_bits, 90, 47, vessel_id);
}

void set_rotating_field_2g(uint8_t* info_bits, rotating_field_type_2g_t rf_type) {
    rotating_field_data_2g_t rf_data = {0};
    prepare_rotating_field_data_2g(&rf_data);
    
    // Set rotating field type identifier (4 bits)
    set_bit_field(info_bits, 154, 4, rf_type);
    
    switch(rf_type) {
        case RF_TYPE_G008_2G:
        case RF_TYPE_ELTDT_2G:
            // Time/Altitude data (44 bits)
            set_bit_field(info_bits, 158, 16, rf_data.time_value);
            set_bit_field(info_bits, 174, 10, rf_data.altitude_code);
            set_bit_field(info_bits, 184, 18, 0);  // Spare bits
            break;
            
        case RF_TYPE_RLS_2G:
            // RLS provider and data
            set_bit_field(info_bits, 158, 8, rf_data.rls_provider);
            set_bit_field(info_bits, 166, 36, rf_data.rls_data);
            break;
            
        case RF_TYPE_CANCEL_2G:
            // Cancellation method
            set_bit_field(info_bits, 158, 2, rf_data.deactivation_method);
            // Fixed bits - all 42 bits set to 1 (T.018 spec)
            set_bit_field_64(info_bits, 160, 42, 0x3FFFFFFFFFFULL);
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

void encode_location_appendix_c(uint8_t* info_bits, float latitude, float longitude) {
    uint64_t encoded_pos = encode_gps_position_2g(latitude, longitude);
    set_bit_field(info_bits, 43, 47, encoded_pos);
}

// =============================================================================
// 23 HEX ID GENERATION
// =============================================================================

void generate_23hex_id_2g(const uint8_t *frame_202bits, char *hex_id) {
    uint8_t id_bits[92] = {0};
    int pos = 0;

    // Extract components according to T.018 Appendix B.2
    id_bits[pos++] = 1;  // Fixed bit 1
    
    // Country Code (bits 31-40)
    for(int i = 30; i < 40; i++) {
        id_bits[pos++] = frame_202bits[i];
    }
    
    // Fixed bits
    id_bits[pos++] = 1;  // Bit 12
    id_bits[pos++] = 0;  // Bit 13
    id_bits[pos++] = 1;  // Bit 14
    
    // TAC Number (bits 1-16)
    for(int i = 0; i < 16; i++) {
        id_bits[pos++] = frame_202bits[i];
    }
    
    // Serial Number (bits 17-30)
    for(int i = 16; i < 30; i++) {
        id_bits[pos++] = frame_202bits[i];
    }
    
    // Test Protocol (bit 43)
    id_bits[pos++] = frame_202bits[42];
    
    // Beacon Type (bits 138-140)
    for(int i = 137; i < 140; i++) {
        id_bits[pos++] = frame_202bits[i];
    }
    
    // Vessel ID (first 44 bits)
    for(int i = 90; i < 134; i++) {
        id_bits[pos++] = frame_202bits[i];
    }

    // Convert to hexadecimal
    for(int i = 0; i < 23; i++) {
        uint8_t nibble = 0;
        for(int j = 0; j < 4; j++) {
            nibble = (nibble << 1) | id_bits[i*4 + j];
        }
        hex_id[i] = "0123456789ABCDEF"[nibble];
    }
    hex_id[23] = '\0';
}

// =============================================================================
// DYNAMIC FIELD FUNCTIONS
// =============================================================================

uint16_t get_last_location_time_2g(void) {
    return (uint16_t)((system_time_2g - last_update_2g) / 60);
}

uint16_t altitude_to_code_2g(double altitude) {
    if(altitude < -1500) return 0;
    if(altitude > 17000) return 1023;
    
    int16_t encoded = (int16_t)((altitude + 1500) * 1023.0 / 18500.0);
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

void prepare_rotating_field_data_2g(rotating_field_data_2g_t* rf_data) {
    rf_data->field_type = beacon_config_2g.rotating_type;
    
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