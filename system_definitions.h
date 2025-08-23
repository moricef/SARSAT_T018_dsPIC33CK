/* system_definitions.h
 * System definitions required by beacon_2g_main.c and oqpsk_modulator_2g.c
 */

#ifndef SYSTEM_DEFINITIONS_H
#define SYSTEM_DEFINITIONS_H

#include <xc.h>
#include <stdint.h>

// Global system timer counter (expected by beacon_2g_main.c:293)
extern volatile uint32_t millis_counter;

// Hardware abstraction functions (expected by beacon_2g_main.c)
void toggle_status_led(void);
void system_init(void);
uint32_t get_system_time_ms(void);
void system_delay_ms(uint16_t ms);
extern volatile unsigned int __attribute__((__sfr__)) _RP20R;

// Bit field manipulation functions
void set_bit_field(uint8_t* buffer, uint16_t start_bit, uint8_t num_bits, uint32_t value);
void set_bit_field_64(uint8_t* buffer, uint16_t start_bit, uint8_t num_bits, uint64_t value);
uint32_t get_bit_field(const uint8_t* buffer, uint16_t start_bit, uint8_t num_bits);
uint64_t get_bit_field_64(const uint8_t* buffer, uint16_t start_bit, uint8_t num_bits);

// =============================
// Hardware Pin Definitions
// =============================
#define AMP_ENABLE_PIN       LATBbits.LATB15  // RF amplifier enable
#define POWER_CTRL_PIN       LATBbits.LATB11  // Power level control
#define LED_TX_PIN           LATDbits.LATD10  // Transmission indicator LED

// Pin control macros
#define STATUS_LED_TRIS      TRISDbits.TRISD10
#define STATUS_LED_LAT       LATDbits.LATD10
#define ADF_CS_TRIS          TRISBbits.TRISB1   // SPI CS for ADF7012
#define ADF_CS_LAT           LATBbits.LATB1
#define LED_TOGGLE()         (LED_TX_PIN = !LED_TX_PIN)

// MCP4922 Dual DAC pins
#define MCP4922_CS_TRIS      TRISBbits.TRISB2
#define MCP4922_CS_LAT       LATBbits.LATB2

// UART Configuration
#define BAUDRATE             115200

// Power Control Definitions
#define POWER_LOW            0
#define POWER_HIGH           1

// =============================
// Mode Switch Definitions (Test/Exercise)
// =============================
#define MODE_SWITCH_TRIS     TRISCbits.TRISC0
#define MODE_SWITCH_PORT     PORTCbits.RC0

// Mode switch states
#define MODE_TEST            0   // Default (pull-up active)
#define MODE_EXERCISE        1   // Switch pressed to ground

// =============================
// ELT Timing Constants (T018)
// =============================
#define ELT_PHASE1_INTERVAL     5000    // 5 seconds
#define ELT_PHASE2_INTERVAL     10000   // 10 seconds  
#define ELT_PHASE3_INTERVAL     28500   // 28.5 seconds
#define ELT_PHASE3_RANDOM       1500    // ±1.5 seconds randomization

#define ELT_PHASE1_COUNT        24      // 24 transmissions in phase 1
#define ELT_PHASE2_COUNT        18      // 18 transmissions in phase 2

#define TEST_INTERVAL           10000   // 10 seconds for test mode

// =============================
// T.018 DSSS Parameters (COSPAS-SARSAT Official)
// =============================
#define SPREADING_FACTOR        256         // Official T.018 spreading factor
#define CHIP_RATE_HZ            38400       // 38.4 kchips/s
#define SYMBOL_RATE_HZ          300         // 300 symbols/s  
#define FRAME_TOTAL_BITS        300         // Total frame length
#define PREAMBLE_BITS           50          // Preamble length (symbols)
#define INFO_BITS               202         // Information bits
#define BCH_PARITY_BITS         48          // BCH parity bits

// PRN LFSR Polynomial: x^23 + x^18 + 1
#define PRN_LFSR_TAPS           0x040040001UL
#define PRN_LFSR_PERIOD         8388607     // 2^23 - 1

// =============================
// Beacon Generations and RF Types
// =============================
#define BEACON_GEN_1            1
#define BEACON_GEN_2            2

#define RF_TYPE_G008            0
#define RF_TYPE_ELTDT           1  
#define RF_TYPE_RLS             2
#define RF_TYPE_CANCEL          3

// =============================
// GPS Configuration
// =============================
#define GPS_BAUDRATE            9600    // Trimble 63530-00 baud rate

// =============================
// GPS Interface Definitions (Trimble 63530-00)
// =============================

// GPS data structure
typedef struct {
    float latitude;         // Decimal degrees
    float longitude;        // Decimal degrees  
    float altitude;         // Meters above sea level
    uint8_t satellites;     // Number of satellites
    uint8_t fix_quality;    // GPS fix quality (0=none, 1=GPS, 2=DGPS)
    uint8_t valid;          // Data validity flag
    uint8_t hour, minute, second;  // Time
    uint8_t day, month;     // Date
    uint16_t year;          // Year
} gps_data_t;

// =============================================================================
// TRANSMISSION CONTROL TYPES
// =============================================================================

// Transmission phases (used by main.c)
typedef enum {
    IDLE_STATE = 0,
    TX_PREAMBLE,
    TX_SYNC, 
    TX_DATA,
    TX_COMPLETE
} tx_phase_t;

// ELT transmission phases
typedef enum {
    ELT_PHASE_1,    // Initial 24 transmissions @ 5s
    ELT_PHASE_2,    // Next 18 transmissions @ 10s
    ELT_PHASE_3     // Continuous @ 28.5s ±1.5s
} elt_phase_t;

#endif /* SYSTEM_DEFINITIONS_H */
