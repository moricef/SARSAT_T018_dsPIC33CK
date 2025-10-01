/* system_comms.c
 * T018 2nd Generation Communication Systems
 * Consolidates: gps_manager.c + oqpsk_modulator_2g.c + prn_generator_2g.c
 */

#include "system_comms.h"
#include "system_debug.h"
#include "protocol_data.h"
#include "error_correction.h"
#include "rf_interface.h"

// =============================================================================
// GLOBAL COMMUNICATION STATE
// =============================================================================

// Communication states
tx_state_t tx_state_2g = {IDLE_STATE, 0, 0, 0, 0};
oqpsk_state_t oqpsk_state_2g = {0, 0, 0, {0}, 0};
prn_state_t prn_state_2g = {0, 0, 0x12345, 0x54321, 0};

// GPS data storage
static gps_data_t current_gps_data = {0};
gps_data_t test_position_2g = {
    .latitude = 43.2,         // Marseille offshore latitude
    .longitude = 5.4,         // Mediterranean longitude
    .altitude = 0.0,          // Sea level (m)
    .satellites = 8,          // Good fix
    .fix_quality = 1,         // GPS fix
    .valid = 1,               // Valid data
    .hour = 12, .minute = 30, .second = 45,
    .day = 15, .month = 11, .year = 2024
};

// NMEA buffer
static char nmea_buffer[NMEA_BUFFER_SIZE];
static uint8_t nmea_index = 0;

// Timing
uint32_t tx_interval_ms = 10000;  // Default 10 seconds
extern volatile uint32_t millis_counter;

// =============================================================================
// GPS MANAGER (Trimble 63530-00)
// =============================================================================

void gps_init(void) {
    memset(&current_gps_data, 0, sizeof(gps_data_t));
    memset(nmea_buffer, 0, NMEA_BUFFER_SIZE);
    nmea_index = 0;
    
    DEBUG_LOG_FLUSH("GPS Manager initialized for Trimble 63530-00\r\n");
}

uint8_t gps_update(void) {
    // Check if UART2 has received data
    while(!U2STAHbits.URXBE) {
        char c = U2RXREG;
        
        // Handle NMEA sentence building
        if(c == '$') {
            nmea_index = 0;
            nmea_buffer[nmea_index++] = c;
        }
        else if(c == '\r' || c == '\n') {
            if(nmea_index > 0) {
                nmea_buffer[nmea_index] = '\0';
                
                if(parse_nmea_sentence(nmea_buffer)) {
                    return 1;  // New data available
                }
                nmea_index = 0;
            }
        }
        else if(nmea_index < (NMEA_BUFFER_SIZE - 1)) {
            nmea_buffer[nmea_index++] = c;
        }
        else {
            nmea_index = 0;  // Buffer overflow
        }
    }
    
    return 0;
}

gps_data_t* get_current_gps_data(void) {
    if(get_beacon_mode_2g() == MODE_TEST) {
        return &test_position_2g;
    } else {
        return &current_gps_data;
    }
}

gps_data_t* get_test_position(void) {
    return &test_position_2g;
}

uint8_t parse_nmea_sentence(const char* sentence) {
    if(sentence == NULL || strlen(sentence) < 6) {
        return 0;
    }
    
    if(strncmp(sentence, "$GPGGA", 6) == 0) {
        return parse_gga(sentence);
    }
    
    if(strncmp(sentence, "$GPRMC", 6) == 0) {
        return parse_rmc(sentence);
    }
    
    return 0;
}

uint8_t parse_gga(const char* sentence) {
    // Simplified parsing - use test data until real GPS connected
    if(get_beacon_mode_2g() == MODE_EXERCISE) {
        memcpy(&current_gps_data, &test_position_2g, sizeof(gps_data_t));
        current_gps_data.valid = 1;
        return 1;
    }
    
    return 0;
}

uint8_t parse_rmc(const char* sentence) {
    // Simplified implementation
    return 0;
}

float nmea_to_degrees(const char* coord, char direction) {
    // Simplified implementation
    return 0.0;
}

uint8_t nmea_get_checksum(const char* sentence) {
    if(sentence == NULL || sentence[0] != '$') {
        return 0;
    }
    
    uint8_t checksum = 0;
    for(int i = 1; sentence[i] != '*' && sentence[i] != '\0'; i++) {
        checksum ^= sentence[i];
    }
    
    return checksum;
}

// =============================================================================
// PRN GENERATOR (T018 DSSS)
// =============================================================================

void generate_prn_sequence_i(int8_t* sequence, uint8_t mode) {
    if(!prn_state_2g.initialized) {
        // T.018 Official initial states
        prn_state_2g.lfsr_i = 0x000001;    // Initial state I
        prn_state_2g.lfsr_q = 0x000041;    // Initial state Q (64 offset)
        prn_state_2g.initialized = 1;
    }
    
    // Generate I-channel PRN sequence using T.018 LFSR (x^23 + x^18 + 1)
    uint32_t lfsr = prn_state_2g.lfsr_i;
    
    for(int i = 0; i < PRN_CHIPS_PER_BIT; i++) {
        // Extract output bit (LSB)
        sequence[i] = (lfsr & 1) ? 1 : -1;
        
        // T.018 LFSR feedback: x^23 + x^18 + 1 (taps at positions 23 and 18)
        uint8_t feedback = ((lfsr >> 22) ^ (lfsr >> 17)) & 1;
        lfsr = (lfsr >> 1) | ((uint32_t)feedback << 22);
        
        // Ensure 23-bit register (mask upper bits)
        lfsr &= 0x7FFFFF;
    }
    
    prn_state_2g.lfsr_i = lfsr;
}

void generate_prn_sequence_q(int8_t* sequence, uint8_t mode) {
    // Generate Q-channel PRN sequence using T.018 LFSR (same polynomial, offset state)
    uint32_t lfsr = prn_state_2g.lfsr_q;
    
    for(int i = 0; i < PRN_CHIPS_PER_BIT; i++) {
        sequence[i] = (lfsr & 1) ? 1 : -1;
        
        // T.018 LFSR feedback: x^23 + x^18 + 1 (same as I channel)
        uint8_t feedback = ((lfsr >> 22) ^ (lfsr >> 17)) & 1;
        lfsr = (lfsr >> 1) | ((uint32_t)feedback << 22);
        
        // Ensure 23-bit register (mask upper bits)
        lfsr &= 0x7FFFFF;
    }
    
    prn_state_2g.lfsr_q = lfsr;
}

void generate_full_prn_sequence(int8_t* sequence_i, int8_t* sequence_q, uint8_t mode) {
    generate_prn_sequence_i(sequence_i, mode);
    generate_prn_sequence_q(sequence_q, mode);
}

uint8_t verify_prn_sequence(uint8_t mode) {
    static int8_t test_seq_i[PRN_CHIPS_PER_BIT];
    static int8_t test_seq_q[PRN_CHIPS_PER_BIT];
    
    generate_full_prn_sequence(test_seq_i, test_seq_q, mode);
    
    // T.018 verification - check first few chips against known values
    // Expected first chips for T.018 LFSR x^23+x^18+1, init=1: 1,0,0,0,0,0,0...
    if(test_seq_i[0] == 1 && test_seq_i[1] == -1 && test_seq_i[2] == -1) {
        DEBUG_LOG_FLUSH("T.018 PRN sequence verification passed\r\n");
        return 1;
    } else {
        DEBUG_LOG_FLUSH("PRN sequence verification failed\r\n");
        return 0;
    }
}

int16_t calculate_prn_autocorrelation(int8_t* sequence, uint16_t length, uint16_t shift) {
    int16_t correlation = 0;
    
    for(int i = 0; i < length; i++) {
        int j = (i + shift) % length;
        correlation += sequence[i] * sequence[j];
    }
    
    return correlation;
}

void reset_prn_generator(void) {
    prn_state_2g.lfsr_i = prn_state_2g.init_i;
    prn_state_2g.lfsr_q = prn_state_2g.init_q;
    prn_state_2g.initialized = 0;
    
    DEBUG_LOG_FLUSH("PRN generator reset\r\n");
}

// =============================================================================
// T.018 HARDWARE TIMING
// =============================================================================

volatile uint8_t chip_timer_active = 0;

void start_chip_timer(void) {
    // Enable chip callback in Timer1 ISR (already running at 38.4 kHz)
    chip_timer_active = 1;

    DEBUG_LOG_FLUSH("T.018 chip timer started (38.400 kHz via Timer1)\r\n");
}

void stop_chip_timer(void) {
    // Disable chip callback in Timer1 ISR
    chip_timer_active = 0;

    DEBUG_LOG_FLUSH("T.018 chip timer stopped\r\n");
}

// Hardware chip timer callback - called by CCP1 ISR at 38.4 kHz
void chip_timer_callback(void) {
    // Safety: Check if CCP1 should be active
    if(!chip_timer_active || !oqpsk_state_2g.transmitting) {
        return;
    }

    oqpsk_state_2g.isr_call_count++;

    // Select chips from active buffer
    int8_t i_chip, q_chip;
    if(oqpsk_state_2g.active_buffer == 0) {
        i_chip = oqpsk_state_2g.chip_buffer_a_i[oqpsk_state_2g.current_chip];
        q_chip = oqpsk_state_2g.chip_buffer_a_q[oqpsk_state_2g.current_chip];
    } else {
        i_chip = oqpsk_state_2g.chip_buffer_b_i[oqpsk_state_2g.current_chip];
        q_chip = oqpsk_state_2g.chip_buffer_b_q[oqpsk_state_2g.current_chip];
    }

    // T.018 OQPSK: Apply half-chip Q delay
    int8_t delayed_q = oqpsk_state_2g.prev_q_chip;
    oqpsk_state_2g.prev_q_chip = q_chip;

    // Convert to 12-bit DAC values (MCP4922)
    uint16_t i_dac = (uint16_t)(2048 + i_chip * 1000);
    uint16_t q_dac = (uint16_t)(2048 + delayed_q * 1000);

    mcp4922_write_both(i_dac, q_dac);

    // Move to next chip
    oqpsk_state_2g.current_chip++;

    // Check if current bit complete (256 chips)
    if(oqpsk_state_2g.current_chip >= PRN_CHIPS_PER_BIT) {
        oqpsk_state_2g.current_chip = 0;
        oqpsk_state_2g.current_bit++;
        oqpsk_state_2g.bits_transmitted++;

        // Switch to next buffer if ready
        if(oqpsk_state_2g.next_chips_ready) {
            oqpsk_state_2g.active_buffer = !oqpsk_state_2g.active_buffer;
            oqpsk_state_2g.next_chips_ready = 0;
        } else {
            oqpsk_state_2g.buffer_misses++;
        }

        // Check if frame complete
        if(oqpsk_state_2g.current_bit >= FRAME_TOTAL_BITS) {
            oqpsk_stop_transmission();
        }
    }
}

// =============================================================================
// OQPSK MODULATOR (T018 2nd Generation)
// =============================================================================

void oqpsk_init(void) {
    memset(&oqpsk_state_2g, 0, sizeof(oqpsk_state_t));
    oqpsk_state_2g.prev_q_chip = 0;
    oqpsk_state_2g.active_buffer = 0;
    oqpsk_state_2g.next_bit_to_gen = 0;
    oqpsk_state_2g.next_chips_ready = 0;
    oqpsk_state_2g.isr_call_count = 0;
    oqpsk_state_2g.bits_transmitted = 0;
    oqpsk_state_2g.buffer_misses = 0;

    // Initialize MCP4922 DAC for I/Q outputs
    mcp4922_init();

    DEBUG_LOG_FLUSH("OQPSK modulator initialized\r\n");
}

void build_2g_frame(uint8_t* info_data, uint8_t* output_frame) {
    memset(output_frame, 0, FRAME_TOTAL_BITS);
    
    // T.018 Preamble (50 bits): Alternating 0,1 pattern for synchronization
    for(int i = 0; i < PREAMBLE_BITS; i++) {
        output_frame[i] = (i % 2);
    }
    
    // Information field (202 bits) - complete encoded frame from protocol_data
    for(int i = 0; i < INFO_BITS; i++) {
        output_frame[PREAMBLE_BITS + i] = get_bit_field(info_data, i, 1);
    }
    
    // BCH parity (48 bits) - extract from encoded frame
    for(int i = 0; i < BCH_PARITY_BITS; i++) {
        output_frame[PREAMBLE_BITS + INFO_BITS + i] = 
            get_bit_field(info_data, INFO_BITS + i, 1);
    }
}

void oqpsk_transmit_frame(uint8_t* info_bits) {
    DEBUG_LOG_FLUSH("Starting OQPSK transmission...\r\n");

    // Build complete transmission frame
    build_2g_frame(info_bits, oqpsk_state_2g.frame_bits);

    // Initialize transmission state
    oqpsk_state_2g.transmitting = 1;
    oqpsk_state_2g.current_bit = 0;
    oqpsk_state_2g.current_chip = 0;
    oqpsk_state_2g.start_time = millis_counter;
    oqpsk_state_2g.prev_q_chip = 0;
    oqpsk_state_2g.active_buffer = 0;  // Start with buffer A
    oqpsk_state_2g.next_bit_to_gen = 0;
    oqpsk_state_2g.next_chips_ready = 0;
    oqpsk_state_2g.isr_call_count = 0;
    oqpsk_state_2g.bits_transmitted = 0;
    oqpsk_state_2g.buffer_misses = 0;

    // Pre-generate bit 0 chips into buffer A (active buffer)
    uint8_t data_bit = oqpsk_state_2g.frame_bits[0];

    generate_prn_sequence_i(oqpsk_state_2g.chip_buffer_a_i, PRN_MODE_NORMAL);
    generate_prn_sequence_q(oqpsk_state_2g.chip_buffer_a_q, PRN_MODE_NORMAL);

    // DSSS spreading for bit 0
    for(int i = 0; i < PRN_CHIPS_PER_BIT; i++) {
        if(!data_bit) {
            oqpsk_state_2g.chip_buffer_a_i[i] = -oqpsk_state_2g.chip_buffer_a_i[i];
            oqpsk_state_2g.chip_buffer_a_q[i] = -oqpsk_state_2g.chip_buffer_a_q[i];
        }
    }

    // Set pipeline: bit 0 ready in buffer A, next generate bit 1
    oqpsk_state_2g.next_bit_to_gen = 1;

    // Enable RF amplifier
    rf_amplifier_enable(1);

    // Start T.018 hardware chip timer (ISR will output chips from buffer A)
    start_chip_timer();

    // Main loop will pre-generate bit 1 into buffer B while ISR transmits bit 0
}

void oqpsk_test_iq_output(void) {
    DEBUG_LOG_FLUSH("Testing OQPSK I/Q outputs...\r\n");
    
    // Generate test I/Q pattern
    for(int i = 0; i < 100; i++) {
        float phase = i * 2.0 * 3.14159 / 50.0;  // 50 samples per cycle
        uint16_t i_val = (uint16_t)(2048 + 1000 * cos(phase));
        uint16_t q_val = (uint16_t)(2048 + 1000 * sin(phase));
        
        mcp4922_write_both(i_val, q_val);
        system_delay_ms(10);
    }
    
    // Return to center
    mcp4922_write_both(2048, 2048);
    DEBUG_LOG_FLUSH("I/Q test completed\r\n");
}

uint8_t oqpsk_is_transmitting(void) {
    return oqpsk_state_2g.transmitting;
}

uint16_t oqpsk_get_bit_position(void) {
    return oqpsk_state_2g.current_bit;
}

void oqpsk_stop_transmission(void) {
    oqpsk_state_2g.transmitting = 0;
    
    // Stop T.018 chip timer
    stop_chip_timer();
    
    rf_amplifier_enable(0);
    mcp4922_write_both(2048, 2048);  // Center DACs
    
    DEBUG_LOG_FLUSH("T.018 transmission stopped\r\n");
}

// =============================================================================
// TRANSMISSION CONTROL
// =============================================================================

void start_beacon_transmission_2g(void) {
    if(!tx_state_2g.active) {
        tx_state_2g.phase = IDLE_STATE;
        tx_state_2g.start_time = millis_counter;
        tx_state_2g.bit_position = 0;
        tx_state_2g.active = 1;
        
        DEBUG_LOG_FLUSH("Beacon transmission started\r\n");
    }
}

void stop_beacon_transmission_2g(void) {
    tx_state_2g.active = 0;
    tx_state_2g.phase = IDLE_STATE;
    oqpsk_stop_transmission();
    
    DEBUG_LOG_FLUSH("Beacon transmission stopped\r\n");
}

uint8_t is_transmission_active_2g(void) {
    return tx_state_2g.active;
}

void transmission_task_2g(void) {
    if(!oqpsk_state_2g.transmitting) return;

    // Atomically check if we need to generate next bit's chips
    uint8_t need_chips = 0;
    uint16_t bit_to_generate = 0;
    uint8_t target_buffer = 0;

    __builtin_disable_interrupts();
    if(!oqpsk_state_2g.next_chips_ready &&
       oqpsk_state_2g.next_bit_to_gen < FRAME_TOTAL_BITS) {
        need_chips = 1;
        bit_to_generate = oqpsk_state_2g.next_bit_to_gen;
        target_buffer = !oqpsk_state_2g.active_buffer;  // Generate in inactive buffer
    }
    __builtin_enable_interrupts();

    if(!need_chips) return;

    // Generate chips outside critical section (takes time)
    uint8_t data_bit = oqpsk_state_2g.frame_bits[bit_to_generate];

    // Select target buffer pointers (ISR won't touch inactive buffer)
    int8_t* target_i = (target_buffer == 0) ?
        oqpsk_state_2g.chip_buffer_a_i : oqpsk_state_2g.chip_buffer_b_i;
    int8_t* target_q = (target_buffer == 0) ?
        oqpsk_state_2g.chip_buffer_a_q : oqpsk_state_2g.chip_buffer_b_q;

    // Generate T.018 PRN chips directly into target buffer (no intermediate copy)
    generate_prn_sequence_i(target_i, PRN_MODE_NORMAL);
    generate_prn_sequence_q(target_q, PRN_MODE_NORMAL);

    // T.018 DSSS spreading: XOR data bit with PRN chips
    if(!data_bit) {
        for(int i = 0; i < PRN_CHIPS_PER_BIT; i++) {
            target_i[i] = -target_i[i];
            target_q[i] = -target_q[i];
        }
    }

    // Mark ready atomically (short critical section)
    __builtin_disable_interrupts();
    oqpsk_state_2g.next_bit_to_gen++;
    oqpsk_state_2g.next_chips_ready = 1;
    __builtin_enable_interrupts();
}

// =============================================================================
// BEACON TASK FUNCTIONS
// =============================================================================

void beacon_task_2g(void) {
    uint32_t current_time = millis_counter;
    uint8_t mode = get_beacon_mode_2g();
    
    // Update GPS data
    gps_update();
    
    if(mode == MODE_TEST) {
        // Test mode: transmit every 10 seconds
        static uint32_t last_test_tx = 0;
        
        if(current_time - last_test_tx >= TEST_INTERVAL) {
            DEBUG_LOG_FLUSH("TEST transmission\r\n");
            transmit_beacon_2g();
            last_test_tx = current_time;
        }
    }
    else {
        // Exercise mode: ELT sequence
        if(!elt_state_2g.active) {
            start_elt_sequence_2g();
        }
        
        uint32_t interval = get_current_interval_2g();
        
        if(current_time - elt_state_2g.last_tx_time >= interval) {
            transmit_beacon_2g();
            elt_state_2g.last_tx_time = current_time;
            elt_state_2g.transmission_count++;
            
            check_phase_transition_2g();
        }
    }
}

void transmit_beacon_2g(void) {
    DEBUG_LOG_FLUSH("\r\n=== TRANSMITTING 2G BEACON ===\r\n");

    // Build compliant frame
    build_compliant_frame_2g();

    // Turn ON transmission LED (RD10)
    LED_TX_PIN = 1;

    // Start OQPSK transmission
    oqpsk_transmit_frame(frame_2g_info);

    // Wait for completion with timeout
    // Expected: 300 bits × 256 chips ÷ 38400 Hz = 2.0s + margin
    uint32_t timeout = millis_counter + 3000;  // 3 second timeout
    while(oqpsk_is_transmitting()) {
        // Call chip generation continuously without delay
        // ISR needs chips generated faster than consumption rate
        transmission_task_2g();

        // Check timeout every 10 calls (reduce overhead)
        static uint16_t loop_count = 0;
        if(++loop_count >= 10) {
            loop_count = 0;
            if(millis_counter > timeout) {
                DEBUG_LOG_FLUSH("WARNING: Transmission timeout - forcing stop\r\n");
                oqpsk_stop_transmission();
                break;
            }
        }
    }

    // Turn OFF transmission LED (RD10)
    LED_TX_PIN = 0;

    // Debug statistics
    DEBUG_LOG_FLUSH("2G transmission complete\r\n");
    char debug_buf[100];
    sprintf(debug_buf, "ISR calls: %lu, Bits TX: %u, Buffer misses: %u\r\n",
            oqpsk_state_2g.isr_call_count,
            oqpsk_state_2g.bits_transmitted,
            oqpsk_state_2g.buffer_misses);
    DEBUG_LOG_FLUSH(debug_buf);
}

uint8_t should_transmit_beacon_2g(void) {
    return (!is_transmission_active_2g() && 
            ((millis_counter - tx_state_2g.last_tx_time) >= tx_interval_ms));
}

// Set transmission interval
void set_tx_interval(uint32_t interval) {
    tx_interval_ms = interval;
}