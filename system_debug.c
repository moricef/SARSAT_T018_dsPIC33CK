/* system_debug.c
 * Debug logging implementation for COSPAS-SARSAT beacon
 */

#include "system_debug.h"
#include "system_definitions.h"
#include <string.h>
#include <stdio.h>

// Debug buffer for formatting
static char debug_buffer[DEBUG_BUFFER_SIZE];

// Initialize debug system
void debug_init(void) {
    // UART is initialized in system_hal.c
    debug_print_string("\r\n=== COSPAS-SARSAT 2G BEACON DEBUG ===\r\n");
    debug_print_string("System: dsPIC33CK64MC105\r\n");
    debug_print_string("Version: Autonomous 1.0\r\n");
    debug_print_string("=====================================\r\n");
}

// Print string via UART
void debug_print_string(const char* str) {
    #if DEBUG_ENABLED
    while(*str) {
        debug_print_char(*str++);
    }
    #endif
}

// Print single character
void debug_print_char(char c) {
    #if DEBUG_ENABLED
    // Wait for transmit buffer to be empty
    while(U1STAbits.UTXBF);
    U1TXREG = c;
    #endif
}

// Print 8-bit hex value
void debug_print_hex(uint8_t value) {
    const char hex_chars[] = "0123456789ABCDEF";
    debug_print_char(hex_chars[(value >> 4) & 0x0F]);
    debug_print_char(hex_chars[value & 0x0F]);
}

// Print 16-bit hex value
void debug_print_hex16(uint16_t value) {
    debug_print_hex((uint8_t)(value >> 8));
    debug_print_hex((uint8_t)(value & 0xFF));
}

// Print 32-bit hex value
void debug_print_hex32(uint32_t value) {
    debug_print_hex16((uint16_t)(value >> 16));
    debug_print_hex16((uint16_t)(value & 0xFFFF));
}

// Print decimal value
void debug_print_dec(uint32_t value) {
    if(value == 0) {
        debug_print_char('0');
        return;
    }
    
    char buffer[12];  // Max for 32-bit: 4294967295
    int i = 0;
    
    while(value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    // Print in reverse order
    while(i > 0) {
        debug_print_char(buffer[--i]);
    }
}

// Print floating point value
void debug_print_float(float value, uint8_t decimals) {
    if(value < 0) {
        debug_print_char('-');
        value = -value;
    }
    
    // Print integer part
    uint32_t int_part = (uint32_t)value;
    debug_print_dec(int_part);
    
    if(decimals > 0) {
        debug_print_char('.');
        
        // Print decimal part
        value -= int_part;
        for(uint8_t i = 0; i < decimals; i++) {
            value *= 10;
            uint8_t digit = (uint8_t)value;
            debug_print_char('0' + digit);
            value -= digit;
        }
    }
}

// Print binary value
void debug_print_binary(uint32_t value, uint8_t bits) {
    for(int8_t i = bits - 1; i >= 0; i--) {
        debug_print_char((value & (1UL << i)) ? '1' : '0');
        if(i > 0 && (i % 8) == 0) {
            debug_print_char(' ');  // Space every 8 bits
        }
    }
}

// Print newline
void debug_newline(void) {
    debug_print_string("\r\n");
}

// Print frame in hex format
void debug_print_frame_hex(const uint8_t* frame, uint16_t length) {
    debug_print_string("Frame hex: ");
    for(uint16_t i = 0; i < length; i++) {
        debug_print_hex(frame[i]);
        if((i + 1) % 16 == 0) {
            debug_newline();
            debug_print_string("           ");
        } else {
            debug_print_char(' ');
        }
    }
    debug_newline();
}

// Print frame in binary format
void debug_print_frame_bits(const uint8_t* frame, uint16_t num_bits) {
    debug_print_string("Frame bits: ");
    for(uint16_t i = 0; i < num_bits; i++) {
        uint16_t byte_index = i / 8;
        uint8_t bit_index = 7 - (i % 8);
        debug_print_char((frame[byte_index] & (1 << bit_index)) ? '1' : '0');
        
        if((i + 1) % 64 == 0) {
            debug_newline();
            debug_print_string("            ");
        } else if((i + 1) % 8 == 0) {
            debug_print_char(' ');
        }
    }
    debug_newline();
}

// Print rotating field analysis
void debug_print_rotating_field(const uint8_t* frame) {
    debug_print_string("Rotating Field Analysis:\r\n");
    
    // Extract rotating field ID (bits 155-158)
    uint8_t rf_id = (frame[19] >> 1) & 0x0F;
    debug_print_string("  Type ID: ");
    debug_print_dec(rf_id);
    
    switch(rf_id) {
        case 0:
            debug_print_string(" (G.008/ELTDT)\r\n");
            break;
        case 14:
            debug_print_string(" (RLS)\r\n");
            break;
        case 15:
            debug_print_string(" (Cancel)\r\n");
            break;
        default:
            debug_print_string(" (Unknown)\r\n");
            break;
    }
    
    // Extract time and altitude for G.008
    if(rf_id == 0) {
        uint16_t time_val = ((frame[19] & 0x01) << 10) | (frame[20] << 2) | ((frame[21] >> 6) & 0x03);
        uint16_t altitude = ((frame[21] & 0x3F) << 4) | ((frame[22] >> 4) & 0x0F);
        
        debug_print_string("  Time value: ");
        debug_print_dec(time_val);
        debug_print_string("\r\n  Altitude code: ");
        debug_print_dec(altitude);
        debug_print_string("\r\n");
    }
}

// debug_print_bch_parity moved to error_correction.c

// Print GPS data
void debug_print_gps_data(float lat, float lon, int16_t alt) {
    debug_print_string("GPS: ");
    debug_print_float(lat, 5);
    debug_print_string(", ");
    debug_print_float(lon, 5);
    debug_print_string(", alt=");
    debug_print_dec(alt);
    debug_print_string("m\r\n");
}

// Print time
void debug_print_time(uint8_t day, uint8_t hour, uint8_t minute) {
    debug_print_string("Time: Day ");
    debug_print_dec(day);
    debug_print_string(", ");
    if(hour < 10) debug_print_char('0');
    debug_print_dec(hour);
    debug_print_char(':');
    if(minute < 10) debug_print_char('0');
    debug_print_dec(minute);
    debug_print_string(" UTC\r\n");
}

// Print system status
void debug_print_system_status(void) {
    debug_print_string("System Status:\r\n");
    debug_print_string("  Uptime: ");
    debug_print_dec(millis_counter / 1000);
    debug_print_string(" seconds\r\n");
    debug_print_string("  Free RAM: ");
    // TODO: Implement stack pointer check for free RAM
    debug_print_string("Unknown\r\n");
}

// Print memory usage
void debug_print_memory_usage(void) {
    debug_print_string("Memory Usage:\r\n");
    debug_print_string("  Stack pointer: 0x");
    debug_print_hex16((uint16_t)&debug_buffer);  // Approximate
    debug_print_string("\r\n");
}