/* rf_interface.h
 * T018 2nd Generation RF Interface
 * Consolidates: hardware_drivers.h + system_hal.h (RF parts)
 */

#ifndef RF_INTERFACE_H
#define RF_INTERFACE_H

#include "includes.h"
#include "system_definitions.h"

// =============================================================================
// MCP4922 DUAL DAC INTERFACE
// =============================================================================

// MCP4922 command bits
#define MCP4922_DAC_A_CMD       0x7000  // DAC A, buffered, gain=1, active
#define MCP4922_DAC_B_CMD       0xF000  // DAC B, buffered, gain=1, active
#define MCP4922_SHUTDOWN_A      0x6000  // Shutdown DAC A
#define MCP4922_SHUTDOWN_B      0xE000  // Shutdown DAC B

// MCP4922 functions
void mcp4922_init(void);
void mcp4922_write_dac_a(uint16_t value);
void mcp4922_write_dac_b(uint16_t value);
void mcp4922_write_both(uint16_t i_value, uint16_t q_value);
void mcp4922_shutdown(void);
void mcp4922_test_output(void);

// =============================================================================
// ADF7012 RF SYNTHESIZER INTERFACE
// =============================================================================

// ADF7012 register addresses
#define ADF7012_REG0    0x00    // Reference divider
#define ADF7012_REG1    0x01    // N divider
#define ADF7012_REG2    0x02    // Function control
#define ADF7012_REG3    0x03    // Initialization

// ADF7012 configuration for 406 MHz
#define ADF7012_FREQ_406MHZ     406025000UL

// ADF7012 functions
void adf7012_init(void);
void adf7012_set_frequency(uint32_t frequency);
void adf7012_enable_output(uint8_t enable);
void adf7012_write_register(uint8_t reg, uint32_t data);
uint8_t adf7012_get_lock_status(void);

// =============================================================================
// RF POWER CONTROL
// =============================================================================

// RF power levels
typedef enum {
    RF_POWER_OFF = 0,
    RF_POWER_LOW,
    RF_POWER_MEDIUM,
    RF_POWER_HIGH
} rf_power_level_t;

// RF control functions
void rf_amplifier_enable(uint8_t enable);
void rf_power_level_set(uint8_t level);
void rf_set_power_level(rf_power_level_t level);
void rf_enable_carrier(uint8_t enable);

// =============================================================================
// SPI INTERFACE (shared between MCP4922 and ADF7012)
// =============================================================================

// SPI device selection
typedef enum {
    SPI_DEVICE_ADF7012 = 0,
    SPI_DEVICE_MCP4922
} spi_device_t;

// SPI functions
void spi_select_device(spi_device_t device);
uint16_t spi_transfer_16(uint16_t data);
uint32_t spi_transfer_32(uint32_t data);
void spi_write_register(spi_device_t device, uint16_t reg_data);

// =============================================================================
// I/Q MODULATION HELPERS
// =============================================================================

// I/Q output functions
void set_iq_outputs(float i_amplitude, float q_amplitude);
void generate_oqpsk_symbol(uint8_t symbol_data, uint16_t* i_out, uint16_t* q_out);
void output_iq_chip(int8_t i_chip, int8_t q_chip);

// Modulation test functions
void test_iq_constellation(void);
void test_carrier_output(void);
void calibrate_iq_offset(void);

// =============================================================================
// RF CALIBRATION AND TESTING
// =============================================================================

// Calibration structure
typedef struct {
    uint16_t i_offset;      // I channel DC offset
    uint16_t q_offset;      // Q channel DC offset
    float i_gain;           // I channel gain correction
    float q_gain;           // Q channel gain correction
    uint8_t calibrated;     // Calibration status
} rf_calibration_t;

// Calibration functions
void rf_calibration_init(void);
void rf_perform_calibration(void);
rf_calibration_t* rf_get_calibration(void);
void rf_apply_calibration(uint16_t* i_value, uint16_t* q_value);

// Test functions
void rf_test_frequency_sweep(void);
void rf_test_power_levels(void);
void rf_test_modulation_quality(void);

// =============================================================================
// RF STATUS AND MONITORING
// =============================================================================

// RF status structure
typedef struct {
    uint8_t adf7012_locked;     // PLL lock status
    uint8_t amplifier_enabled;  // Amplifier state
    rf_power_level_t power_level;
    uint32_t current_frequency;
    uint8_t transmission_active;
} rf_status_t;

// Status functions
rf_status_t* rf_get_status(void);
void rf_update_status(void);
uint8_t rf_is_ready(void);

// =============================================================================
// HARDWARE INITIALIZATION
// =============================================================================

// Complete RF subsystem initialization
void rf_interface_init(void);
void rf_subsystem_enable(uint8_t enable);
void rf_emergency_shutdown(void);

// Low-level hardware setup
void rf_configure_pins(void);
void rf_configure_spi(void);
void rf_configure_timers(void);

#endif /* RF_INTERFACE_H */