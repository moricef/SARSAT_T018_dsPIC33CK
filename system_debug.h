/* system_debug.h
 * Debug macros and basic functions for COSPAS-SARSAT beacon
 */

#ifndef SYSTEM_DEBUG_H
#define SYSTEM_DEBUG_H

#include <stdint.h>

// Debug buffer size
#define DEBUG_BUFFER_SIZE 256

// Debug macros (used throughout the code)
#ifdef DEBUG_ENABLED
    #define DEBUG_LOG_FLUSH(msg)    debug_print_string(msg)
    #define DEBUG_LOG_INFO(msg)     debug_print_string(msg)
    #define DEBUG_LOG_ERROR(msg)    debug_print_string(msg)
    #define DEBUG_LOG_WARN(msg)     debug_print_string(msg)
    #define DEBUG_LOG_ISR(msg)      debug_print_string(msg)
#else
    #define DEBUG_LOG_FLUSH(msg)
    #define DEBUG_LOG_INFO(msg)
    #define DEBUG_LOG_ERROR(msg)
    #define DEBUG_LOG_WARN(msg)
    #define DEBUG_LOG_ISR(msg)
#endif

// Debug initialization
void debug_init(void);

// Debug print functions
void debug_print_char(char c);
void debug_print_dec(uint32_t value);
void debug_print_hex(uint8_t value);
void debug_print_float(float value, uint8_t decimals);
void debug_print_string(const char* str);

#endif /* SYSTEM_DEBUG_H */