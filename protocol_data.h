/* protocol_data.h
 * T018 2nd Generation Protocol Data Structures
 * Replaces fragmented beacon utilities
 */

#ifndef PROTOCOL_DATA_H
#define PROTOCOL_DATA_H

#include "includes.h"
#include "system_definitions.h"

// =============================================================================
// T018 FRAME STRUCTURE DEFINITIONS
// =============================================================================

// T018 frame types
typedef enum {
    BEACON_TEST_FRAME_2G,      // Test frame for decoder validation
    BEACON_EXERCISE_FRAME_2G   // Exercise frame for ELT simulation
} beacon_frame_type_2g_t;

// ELT transmission phases defined in system_definitions.h as elt_phase_t

// T018 rotating field types
typedef enum {
    RF_TYPE_G008_2G = 0,     // G008 ELTDT (Time/Altitude)
    RF_TYPE_ELTDT_2G,        // ELT-DT (Time/Altitude)
    RF_TYPE_RLS_2G,          // Return Link Service
    RF_TYPE_CANCEL_2G        // Cancellation message
} rotating_field_type_2g_t;

// =============================================================================
// T018 FRAME BUILDING FUNCTIONS
// =============================================================================

// Main frame building
void build_2g_information_field(uint8_t* info_bits);
void build_2g_complete_frame(uint8_t* info_bits, uint8_t* complete_frame);
void build_compliant_frame_2g(void);

// Frame components
void set_23_hex_id_2g(uint8_t* info_bits);
void encode_location_2g(uint8_t* info_bits, float latitude, float longitude);
void set_vessel_id_2g(uint8_t* info_bits);
void set_rotating_field_2g(uint8_t* info_bits, rotating_field_type_2g_t rf_type);

// GPS position encoding (T018 specific)
uint64_t encode_gps_position_2g(double lat, double lon);
void encode_location_appendix_c(uint8_t* info_bits, float latitude, float longitude);

// 23 HEX ID generation (T018 Appendix B.2)
void generate_23hex_id_2g(const uint8_t *frame_202bits, char *hex_id);
void build_23hex_from_components(uint16_t tac, uint16_t serial, uint16_t country, 
                                uint8_t protocol, char *hex_id);

// Dynamic field functions
uint16_t get_last_location_time_2g(void);
uint16_t altitude_to_code_2g(double altitude);
uint8_t encode_altitude_2g(float altitude_m);
uint32_t encode_time_value_2g(uint8_t day, uint8_t hour, uint8_t minute, uint8_t type);

// Vessel ID functions
uint64_t encode_vessel_id_2g(uint64_t mmsi_or_reg);
void set_vessel_id_field_2g(uint8_t* info_bits, uint64_t vessel_id);

// =============================================================================
// T018 BEACON CONFIGURATION
// =============================================================================

// Beacon configuration structure
typedef struct {
    uint8_t generation;         // 1 = 1st Gen, 2 = 2nd Gen
    uint8_t test_mode;          // 0 = Normal, 1 = Self-test
    rotating_field_type_2g_t rotating_type;
    uint32_t beacon_id;         // 23 HEX ID
    uint16_t country_code;      // Country code
    uint8_t protocol_code;      // Protocol type
    uint64_t vessel_id;         // MMSI or aircraft registration
} beacon_config_2g_t;

// ELT transmission state
typedef struct {
    elt_phase_t current_phase;
    uint16_t transmission_count;
    uint32_t last_tx_time;
    uint32_t phase_start_time;
    uint8_t active;
} elt_state_2g_t;

// =============================================================================
// T018 ROTATING FIELD DATA
// =============================================================================

// Rotating field data structure
typedef struct {
    rotating_field_type_2g_t field_type;
    union {
        struct {    // G008/ELTDT
            uint32_t time_value;
            uint16_t altitude_code;
        };
        struct {    // RLS
            uint8_t rls_provider;
            uint64_t rls_data;
        };
        struct {    // Cancel
            uint8_t deactivation_method;
        };
    };
} rotating_field_data_2g_t;

// =============================================================================
// T018 CONFIGURATION FUNCTIONS
// =============================================================================

// Configuration management
void load_beacon_configuration_2g(void);
void save_beacon_configuration_2g(void);
beacon_config_2g_t* get_beacon_config_2g(void);

// Frame type selection
beacon_frame_type_2g_t get_frame_type_from_switch_2g(void);
uint8_t get_beacon_mode_2g(void);

// ELT sequence management
void start_elt_sequence_2g(void);
void stop_elt_sequence_2g(void);
uint32_t get_current_interval_2g(void);
void check_phase_transition_2g(void);

// Rotating field configuration
void prepare_rotating_field_data_2g(rotating_field_data_2g_t* rf_data);
const char* get_rotating_field_name_2g(rotating_field_type_2g_t rf_type);

// Stub functions for external data
uint8_t get_rls_provider_id_2g(void);
uint64_t get_rls_data_2g(void);
uint8_t get_deactivation_method_2g(void);
uint64_t get_configured_vessel_id_2g(void);

// =============================================================================
// GLOBAL FRAME BUFFER
// =============================================================================

// Global frame buffers
extern uint8_t beacon_frame_2g[252];        // Complete frame with header+BCH
extern uint8_t frame_2g_info[202];          // Information field only
extern beacon_config_2g_t beacon_config_2g;
extern elt_state_2g_t elt_state_2g;

// System state variables
extern uint32_t system_time_2g;
extern uint32_t last_update_2g;
extern float current_latitude_2g;
extern float current_longitude_2g;
extern float current_altitude_2g;

#endif /* PROTOCOL_DATA_H */