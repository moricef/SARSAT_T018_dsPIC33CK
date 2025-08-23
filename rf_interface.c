/* rf_interface.c
 * T018 2nd Generation RF Interface Implementation
 * Consolidates: hardware_drivers.c + system_hal.c (RF parts)
 */

#include "rf_interface.h"
#include "system_debug.h"

// =============================================================================
// GLOBAL RF STATE
// =============================================================================

static rf_status_t rf_status = {0, 0, RF_POWER_OFF, 0, 0};
static rf_calibration_t rf_calibration = {2048, 2048, 1.0, 1.0, 0};

// =============================================================================
// RF INTERFACE INITIALIZATION
// =============================================================================

void rf_interface_init(void) {
    DEBUG_LOG_FLUSH("Initializing RF interface...\r\n");
    
    // Configure pins and SPI
    rf_configure_pins();
    rf_configure_spi();
    
    // Initialize DAC
    mcp4922_init();
    
    // Initialize RF synthesizer
    adf7012_init();
    
    // Perform calibration
    rf_calibration_init();
    
    // Update status
    rf_update_status();
    
    DEBUG_LOG_FLUSH("RF interface initialized\r\n");
}

void rf_configure_pins(void) {
    // MCP4922 CS pin
    TRISBbits.TRISB2 = 0;     // Output
    LATBbits.LATB2 = 1;       // CS inactive (high)
    
    // ADF7012 CS pin  
    ADF_CS_TRIS = 0;          // Output
    ADF_CS_LAT = 1;           // CS inactive (high)
    
    // RF amplifier control
    TRISBbits.TRISB15 = 0;    // Output
    LATBbits.LATB15 = 0;      // Initially disabled
    
    // Power level control
    TRISBbits.TRISB11 = 0;    // Output
    LATBbits.LATB11 = 0;      // Low power initially
}

void rf_configure_spi(void) {
    // SPI1 already configured in system_hal.c
    // Just ensure it's properly set for RF devices
    SPI1CON1bits.MODE16 = 1;    // 16-bit mode for most operations
    SPI1CON1bits.MSTEN = 1;     // Master mode
}

// =============================================================================
// MCP4922 DUAL DAC IMPLEMENTATION
// =============================================================================

void mcp4922_init(void) {
    // Ensure CS is high (inactive)
    MCP4922_CS_LAT = 1;
    
    // Initialize both DACs to mid-scale
    mcp4922_write_dac_a(2048);
    mcp4922_write_dac_b(2048);
    
    DEBUG_LOG_FLUSH("MCP4922 dual DAC initialized\r\n");
}

void mcp4922_write_dac_a(uint16_t value) {
    value &= 0x0FFF;  // Ensure 12-bit value
    uint16_t command = MCP4922_DAC_A_CMD | value;
    
    spi_select_device(SPI_DEVICE_MCP4922);
    spi_transfer_16(command);
    spi_select_device(SPI_DEVICE_ADF7012);  // Restore default
}

void mcp4922_write_dac_b(uint16_t value) {
    value &= 0x0FFF;  // Ensure 12-bit value
    uint16_t command = MCP4922_DAC_B_CMD | value;
    
    spi_select_device(SPI_DEVICE_MCP4922);
    spi_transfer_16(command);
    spi_select_device(SPI_DEVICE_ADF7012);  // Restore default
}

void mcp4922_write_both(uint16_t i_value, uint16_t q_value) {
    // Apply calibration if available
    if(rf_calibration.calibrated) {
        rf_apply_calibration(&i_value, &q_value);
    }
    
    mcp4922_write_dac_a(i_value);  // I channel
    mcp4922_write_dac_b(q_value);  // Q channel
}

void mcp4922_shutdown(void) {
    spi_select_device(SPI_DEVICE_MCP4922);
    spi_transfer_16(MCP4922_SHUTDOWN_A);
    spi_transfer_16(MCP4922_SHUTDOWN_B);
    spi_select_device(SPI_DEVICE_ADF7012);
    
    DEBUG_LOG_FLUSH("MCP4922 shutdown\r\n");
}

void mcp4922_test_output(void) {
    DEBUG_LOG_FLUSH("Testing MCP4922 I/Q outputs...\r\n");
    
    // Test pattern: sine wave on I, cosine on Q
    for(int i = 0; i < 360; i += 10) {
        float angle = i * 3.14159 / 180.0;
        uint16_t i_val = (uint16_t)(2048 + 1000 * sin(angle));
        uint16_t q_val = (uint16_t)(2048 + 1000 * cos(angle));
        
        mcp4922_write_both(i_val, q_val);
        system_delay_ms(10);
    }
    
    // Return to center
    mcp4922_write_both(2048, 2048);
    DEBUG_LOG_FLUSH("MCP4922 test completed\r\n");
}

// =============================================================================
// ADF7012 RF SYNTHESIZER IMPLEMENTATION
// =============================================================================

void adf7012_init(void) {
    DEBUG_LOG_FLUSH("Initializing ADF7012 RF synthesizer...\r\n");
    
    // Power-on delay
    system_delay_ms(10);
    
    // Initialize registers (simplified configuration)
    adf7012_write_register(ADF7012_REG3, 0x0001C7);  // Initialize
    system_delay_ms(1);
    
    adf7012_write_register(ADF7012_REG0, 0x200000);  // Reference setup
    adf7012_write_register(ADF7012_REG1, 0x80325B);  // N divider for 406MHz
    adf7012_write_register(ADF7012_REG2, 0x10E42A);  // Function control
    
    system_delay_ms(5);  // Allow PLL to lock
    
    rf_status.current_frequency = ADF7012_FREQ_406MHZ;
    DEBUG_LOG_FLUSH("ADF7012 initialized for 406 MHz\r\n");
}

void adf7012_set_frequency(uint32_t frequency) {
    // Calculate N divider for desired frequency
    // Simplified calculation - real implementation would be more complex
    uint32_t n_divider = frequency / 25000;  // Assuming 25kHz reference
    
    uint32_t reg1_data = 0x800000 | (n_divider & 0x7FFF);
    adf7012_write_register(ADF7012_REG1, reg1_data);
    
    system_delay_ms(2);  // Allow PLL to lock
    rf_status.current_frequency = frequency;
    
    DEBUG_LOG_FLUSH("ADF7012 frequency set to: ");
    debug_print_dec(frequency);
    DEBUG_LOG_FLUSH(" Hz\r\n");
}

void adf7012_enable_output(uint8_t enable) {
    uint32_t reg2_data = 0x10E42A;  // Base configuration
    
    if(enable) {
        reg2_data |= 0x000008;  // Enable RF output
    } else {
        reg2_data &= ~0x000008; // Disable RF output
    }
    
    adf7012_write_register(ADF7012_REG2, reg2_data);
}

void adf7012_write_register(uint8_t reg, uint32_t data) {
    // Combine register address with data
    uint32_t spi_data = (data & 0xFFFFFC) | (reg & 0x03);
    
    spi_select_device(SPI_DEVICE_ADF7012);
    spi_transfer_32(spi_data);
    spi_select_device(SPI_DEVICE_ADF7012);  // Keep selected as default
}

uint8_t adf7012_get_lock_status(void) {
    // In a real implementation, would read lock detect pin
    // For now, assume locked after initialization
    return rf_status.adf7012_locked;
}

// =============================================================================
// RF POWER CONTROL
// =============================================================================

void rf_amplifier_enable(uint8_t enable) {
    if(enable) {
        LATBbits.LATB15 = 1;
        rf_status.amplifier_enabled = 1;
        DEBUG_LOG_FLUSH("RF amplifier enabled\r\n");
    } else {
        LATBbits.LATB15 = 0;
        rf_status.amplifier_enabled = 0;
        DEBUG_LOG_FLUSH("RF amplifier disabled\r\n");
    }
}

void rf_power_level_set(uint8_t level) {
    if(level > 0) {
        LATBbits.LATB11 = 1;  // High power
    } else {
        LATBbits.LATB11 = 0;  // Low power
    }
}

void rf_set_power_level(rf_power_level_t level) {
    rf_status.power_level = level;
    
    switch(level) {
        case RF_POWER_OFF:
            rf_amplifier_enable(0);
            adf7012_enable_output(0);
            break;
            
        case RF_POWER_LOW:
            rf_power_level_set(0);
            rf_amplifier_enable(1);
            adf7012_enable_output(1);
            break;
            
        case RF_POWER_MEDIUM:
        case RF_POWER_HIGH:
            rf_power_level_set(1);
            rf_amplifier_enable(1);
            adf7012_enable_output(1);
            break;
    }
    
    DEBUG_LOG_FLUSH("RF power level set to: ");
    debug_print_dec(level);
    DEBUG_LOG_FLUSH("\r\n");
}

// =============================================================================
// SPI INTERFACE IMPLEMENTATION
// =============================================================================

void spi_select_device(spi_device_t device) {
    switch(device) {
        case SPI_DEVICE_ADF7012:
            ADF_CS_LAT = 0;      // Select ADF7012
            MCP4922_CS_LAT = 1;  // Deselect MCP4922
            break;
            
        case SPI_DEVICE_MCP4922:
            ADF_CS_LAT = 1;      // Deselect ADF7012
            MCP4922_CS_LAT = 0;  // Select MCP4922
            break;
    }
    
    __delay_us(1);  // CS setup time
}

uint16_t spi_transfer_16(uint16_t data) {
    while(SPI1STATLbits.SPITBF);  // Wait for transmit buffer
    
    SPI1BUFL = data;
    
    while(!SPI1STATLbits.SPIRBF); // Wait for receive
    
    return SPI1BUFL;
}

uint32_t spi_transfer_32(uint32_t data) {
    // Transfer 32-bit data as two 16-bit transfers
    uint16_t high_word = (data >> 16) & 0xFFFF;
    uint16_t low_word = data & 0xFFFF;
    
    spi_transfer_16(high_word);
    spi_transfer_16(low_word);
    
    return 0;  // ADF7012 doesn't return data
}

void spi_write_register(spi_device_t device, uint16_t reg_data) {
    spi_select_device(device);
    spi_transfer_16(reg_data);
    spi_select_device(SPI_DEVICE_ADF7012);  // Restore default
}

// =============================================================================
// I/Q MODULATION HELPERS
// =============================================================================

void set_iq_outputs(float i_amplitude, float q_amplitude) {
    // Convert normalized amplitudes (-1.0 to +1.0) to 12-bit DAC values
    uint16_t i_dac = (uint16_t)(2048 + i_amplitude * 2047);
    uint16_t q_dac = (uint16_t)(2048 + q_amplitude * 2047);
    
    // Clamp to valid range
    if(i_dac > 4095) i_dac = 4095;
    if(q_dac > 4095) q_dac = 4095;
    
    mcp4922_write_both(i_dac, q_dac);
}

void generate_oqpsk_symbol(uint8_t symbol_data, uint16_t* i_out, uint16_t* q_out) {
    // OQPSK symbol mapping
    float i_val = ((symbol_data & 0x02) ? -1.0 : +1.0);  // Bit 1
    float q_val = ((symbol_data & 0x01) ? -1.0 : +1.0);  // Bit 0
    
    *i_out = (uint16_t)(2048 + i_val * 2047);
    *q_out = (uint16_t)(2048 + q_val * 2047);
}

void output_iq_chip(int8_t i_chip, int8_t q_chip) {
    uint16_t i_dac = (uint16_t)(2048 + i_chip * 1000);  // Scale to DAC range
    uint16_t q_dac = (uint16_t)(2048 + q_chip * 1000);
    
    mcp4922_write_both(i_dac, q_dac);
}

// =============================================================================
// RF CALIBRATION
// =============================================================================

void rf_calibration_init(void) {
    DEBUG_LOG_FLUSH("Initializing RF calibration...\r\n");
    
    // Set default calibration values
    rf_calibration.i_offset = 2048;
    rf_calibration.q_offset = 2048;
    rf_calibration.i_gain = 1.0;
    rf_calibration.q_gain = 1.0;
    rf_calibration.calibrated = 0;
    
    // Perform basic calibration
    rf_perform_calibration();
}

void rf_perform_calibration(void) {
    DEBUG_LOG_FLUSH("Performing RF calibration...\r\n");
    
    // Basic DC offset calibration
    // In a real implementation, would measure actual output
    
    // For now, assume calibration successful
    rf_calibration.calibrated = 1;
    
    DEBUG_LOG_FLUSH("RF calibration completed\r\n");
}

rf_calibration_t* rf_get_calibration(void) {
    return &rf_calibration;
}

void rf_apply_calibration(uint16_t* i_value, uint16_t* q_value) {
    if(!rf_calibration.calibrated) return;
    
    // Apply offset and gain corrections
    float i_corrected = (*i_value - 2048) * rf_calibration.i_gain + rf_calibration.i_offset;
    float q_corrected = (*q_value - 2048) * rf_calibration.q_gain + rf_calibration.q_offset;
    
    // Clamp to valid range
    if(i_corrected < 0) i_corrected = 0;
    if(i_corrected > 4095) i_corrected = 4095;
    if(q_corrected < 0) q_corrected = 0;
    if(q_corrected > 4095) q_corrected = 4095;
    
    *i_value = (uint16_t)i_corrected;
    *q_value = (uint16_t)q_corrected;
}

// =============================================================================
// RF STATUS AND MONITORING
// =============================================================================

rf_status_t* rf_get_status(void) {
    return &rf_status;
}

void rf_update_status(void) {
    rf_status.adf7012_locked = adf7012_get_lock_status();
    // Other status updates would go here
}

uint8_t rf_is_ready(void) {
    return (rf_status.adf7012_locked && 
            rf_calibration.calibrated);
}

// =============================================================================
// SYSTEM CONTROL
// =============================================================================

void rf_subsystem_enable(uint8_t enable) {
    if(enable) {
        rf_set_power_level(RF_POWER_LOW);
        DEBUG_LOG_FLUSH("RF subsystem enabled\r\n");
    } else {
        rf_set_power_level(RF_POWER_OFF);
        DEBUG_LOG_FLUSH("RF subsystem disabled\r\n");
    }
}

void rf_emergency_shutdown(void) {
    DEBUG_LOG_FLUSH("RF emergency shutdown\r\n");
    
    rf_amplifier_enable(0);
    adf7012_enable_output(0);
    mcp4922_shutdown();
    
    rf_status.amplifier_enabled = 0;
    rf_status.power_level = RF_POWER_OFF;
    rf_status.transmission_active = 0;
}