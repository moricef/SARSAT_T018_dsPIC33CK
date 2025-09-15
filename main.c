/* main.c - T018 2nd Generation Beacon Main Application
 * Based on T001 structure and organization
 */

#include "includes.h"
#include "system_definitions.h"
#include "system_debug.h"
#include "system_comms.h"
#include "protocol_data.h"
#include "rf_interface.h"
#include "error_correction.h"

// External declarations
extern volatile uint32_t millis_counter;
extern volatile tx_phase_t tx_phase;

// Global variables
static uint32_t last_tx_time = 0;

// Function declarations
void start_beacon_frame_2g(beacon_frame_type_2g_t frame_type);

// Read beacon mode from switch
beacon_frame_type_2g_t get_frame_type_from_switch(void) {
    // MODE_SWITCH_PORT: 0=TEST, 1=EXERCISE
    return MODE_SWITCH_PORT ? BEACON_EXERCISE_FRAME_2G : BEACON_TEST_FRAME_2G;
}

uint8_t should_transmit_beacon(void) {
    uint32_t current_millis;
    uint32_t last_tx;
    
    // Atomic read of shared variables
    __builtin_disable_interrupts();
    current_millis = millis_counter;
    last_tx = last_tx_time;
    __builtin_enable_interrupts();

    return (!is_transmission_active_2g()) && 
           ((current_millis - last_tx) >= get_current_interval_2g());
}

int main(void) {
    __builtin_disable_interrupts();
    
    // Initialize system
    system_init();
    
    // Set initial transmission interval
    tx_interval_ms = TEST_INTERVAL;  // 10 seconds default
    
    __builtin_enable_interrupts();
    
    DEBUG_LOG_FLUSH("=== T018 2ND GENERATION BEACON ===\r\n");
    DEBUG_LOG_FLUSH("System initialized\r\n");
    
    // Initialize RF subsystem
    rf_interface_init();
    rf_set_power_level(RF_POWER_LOW);
    
    // Initialize communication systems
    gps_init();
    oqpsk_init();
    
    // Verify PRN sequences
    if(!verify_prn_sequence(PRN_MODE_NORMAL)) {
        DEBUG_LOG_FLUSH("WARNING: PRN sequence verification failed\r\n");
    }
    
    // Test BCH encoder
    if(!test_bch_encoder_2g()) {
        DEBUG_LOG_FLUSH("WARNING: BCH encoder test failed\r\n");
    }
    
    // Load beacon configuration
    load_beacon_configuration_2g();
    
    // Determine frame type from switch
    beacon_frame_type_2g_t frame_type = get_frame_type_from_switch();
    DEBUG_LOG_FLUSH("Starting transmission - Mode: ");
    
    if(frame_type == BEACON_TEST_FRAME_2G) {
        DEBUG_LOG_FLUSH("TEST (decoder validation)\r\n");
        tx_interval_ms = TEST_INTERVAL;
        beacon_config_2g.test_mode = 1;
    } else {
        DEBUG_LOG_FLUSH("EXERCISE (ELT simulation)\r\n");
        beacon_config_2g.test_mode = 0;
        start_elt_sequence_2g();
    }
    
    DEBUG_LOG_FLUSH("Beacon ready - entering main loop\r\n");
    
    // Main loop
    while(1) {
        uint32_t current_time = millis_counter;
        
        // Update GPS data
        gps_update();
        
        // Check if transmission should occur
        if(should_transmit_beacon()) {
            start_beacon_frame_2g(frame_type);
        }
        
        // Run beacon task
        beacon_task_2g();
        
        // Update RF status
        rf_update_status();
        
        // Status indication
        if((current_time % 1000) == 0) {  // Every second
            toggle_status_led();
        }
        
        // Small delay to prevent watchdog timeout
        system_delay_ms(100);
    }
    
    return 0;
}

void start_beacon_frame_2g(beacon_frame_type_2g_t frame_type) {
    DEBUG_LOG_FLUSH("\r\n=== STARTING BEACON TRANSMISSION ===\r\n");
    
    // Update system time for rotating fields
    system_time_2g = millis_counter;
    
    // Set beacon configuration based on frame type
    if(frame_type == BEACON_TEST_FRAME_2G) {
        beacon_config_2g.test_mode = 1;
        beacon_config_2g.rotating_type = RF_TYPE_G008_2G;
        DEBUG_LOG_FLUSH("Mode: TEST - Fixed position (Grenoble)\r\n");
    } else {
        beacon_config_2g.test_mode = 0;
        beacon_config_2g.rotating_type = RF_TYPE_ELTDT_2G;
        DEBUG_LOG_FLUSH("Mode: EXERCISE - ELT Phase ");
        debug_print_dec(elt_state_2g.current_phase + 1);
        DEBUG_LOG_FLUSH("\r\n");
    }
    
    // Build and transmit frame
    transmit_beacon_2g();
    
    // Update transmission time
    last_tx_time = millis_counter;
    
    // For exercise mode, update ELT state
    if(frame_type == BEACON_EXERCISE_FRAME_2G) {
        elt_state_2g.transmission_count++;
        check_phase_transition_2g();
        
        DEBUG_LOG_FLUSH("ELT transmission #");
        debug_print_dec(elt_state_2g.transmission_count);
        DEBUG_LOG_FLUSH(" in phase ");
        debug_print_dec(elt_state_2g.current_phase + 1);
        DEBUG_LOG_FLUSH("\r\n");
    }
    
    DEBUG_LOG_FLUSH("=== TRANSMISSION COMPLETE ===\r\n");
}
