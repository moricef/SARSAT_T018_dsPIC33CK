/* system_comms.h
 * T018 2nd Generation Communication Systems
 * Consolidates: gps_manager.h + oqpsk_modulator_2g.h + prn_generator_2g.h
 */

#ifndef SYSTEM_COMMS_H
#define SYSTEM_COMMS_H

#include "includes.h"
#include "system_definitions.h"

// =============================================================================
// GPS MANAGER (Trimble 63530-00)
// =============================================================================

// NMEA sentence buffer size
#define NMEA_BUFFER_SIZE    128
#define NMEA_MAX_FIELDS     20

// GPS functions
void gps_init(void);
uint8_t gps_update(void);
gps_data_t* get_current_gps_data(void);
gps_data_t* get_test_position(void);

// NMEA parsing functions
uint8_t parse_nmea_sentence(const char* sentence);
uint8_t parse_gga(const char* sentence);
uint8_t parse_rmc(const char* sentence);

// Utility functions
float nmea_to_degrees(const char* coord, char direction);
uint8_t nmea_get_checksum(const char* sentence);

// =============================================================================
// OQPSK MODULATOR (T018 2nd Generation)
// =============================================================================

// OQPSK parameters for T018
#define OQPSK_CHIP_RATE     38400       // 38.4 kchips/sec
#define OQPSK_BIT_RATE      300         // 300 bps
#define OQPSK_SYMBOLS_PER_BIT 128       // Spreading factor

// OQPSK state
typedef struct {
    uint8_t transmitting;
    uint16_t current_bit;
    uint16_t current_symbol;
    uint8_t frame_bits[252];
    uint32_t start_time;
} oqpsk_state_t;

// OQPSK functions
void oqpsk_init(void);
void build_2g_frame(uint8_t* info_data, uint8_t* output_frame);
void oqpsk_transmit_frame(uint8_t* info_bits);
void oqpsk_test_iq_output(void);

// OQPSK status functions
uint8_t oqpsk_is_transmitting(void);
uint16_t oqpsk_get_bit_position(void);
void oqpsk_stop_transmission(void);

// DMA interrupt handler (declared for reference)
void __attribute__((interrupt, auto_psv)) _DMA0Interrupt(void);

// =============================================================================
// PRN GENERATOR (T018 DSSS)
// =============================================================================

// PRN sequence parameters (T.018 Official)
#define PRN_SEQUENCE_LENGTH PRN_LFSR_PERIOD   // T.018 PRN period (2^23-1)
#define PRN_CHIPS_PER_BIT   SPREADING_FACTOR  // T.018 spreading factor (256)

// PRN modes
typedef enum {
    PRN_MODE_NORMAL = 0,
    PRN_MODE_TEST
} prn_mode_t;

// PRN state structure
typedef struct {
    uint32_t lfsr_i;        // I-channel LFSR state
    uint32_t lfsr_q;        // Q-channel LFSR state
    uint32_t init_i;        // I-channel initial state
    uint32_t init_q;        // Q-channel initial state
    uint8_t initialized;
} prn_state_t;

// PRN generation functions
void generate_prn_sequence_i(int8_t* sequence, uint8_t mode);
void generate_prn_sequence_q(int8_t* sequence, uint8_t mode);
void generate_full_prn_sequence(int8_t* sequence_i, int8_t* sequence_q, uint8_t mode);

// PRN verification and testing
uint8_t verify_prn_sequence(uint8_t mode);
int16_t calculate_prn_autocorrelation(int8_t* sequence, uint16_t length, uint16_t shift);
void reset_prn_generator(void);

// T.018 Hardware timing functions
void start_chip_timer(void);
void stop_chip_timer(void);
extern volatile uint8_t chip_timer_active;

// =============================================================================
// TRANSMISSION CONTROL
// =============================================================================

// Transmission phases defined in system_definitions.h

// Transmission state
typedef struct {
    tx_phase_t phase;
    uint32_t start_time;
    uint32_t last_tx_time;
    uint16_t bit_position;
    uint8_t active;
} tx_state_t;

// Transmission control functions
void start_beacon_transmission_2g(void);
void stop_beacon_transmission_2g(void);
uint8_t is_transmission_active_2g(void);
void transmission_task_2g(void);

// Beacon task functions
void beacon_task_2g(void);
uint8_t should_transmit_beacon_2g(void);
void transmit_beacon_2g(void);
void set_tx_interval(uint32_t interval);

// =============================================================================
// MODULATION HELPERS
// =============================================================================

// I/Q symbol generation
void generate_oqpsk_symbols(uint8_t data_bit, int8_t* i_chips, int8_t* q_chips);
void spread_data_with_prn(uint8_t* data_bits, uint16_t num_bits, 
                         int8_t* i_sequence, int8_t* q_sequence,
                         int8_t* output_i, int8_t* output_q);

// Frame formatting
void format_2g_transmission_frame(uint8_t* info_bits, uint8_t* tx_frame);
void add_preamble_and_sync(uint8_t* frame_bits, uint8_t* tx_frame);

// =============================================================================
// COMMUNICATION STATE
// =============================================================================

// Global communication state
extern tx_state_t tx_state_2g;
extern oqpsk_state_t oqpsk_state_2g;
extern prn_state_t prn_state_2g;

// GPS test position (Grenoble area)
extern gps_data_t test_position_2g;

// Timing constants
extern uint32_t tx_interval_ms;
extern volatile uint32_t millis_counter;

#endif /* SYSTEM_COMMS_H */