/* system_debug.h
 * Debug macros and basic functions for COSPAS-SARSAT beacon
 */

#ifndef SYSTEM_DEBUG_H
#define SYSTEM_DEBUG_H

#include <stdint.h>

// Debug buffer size
#define DEBUG_BUFFER_SIZE 256

// Debug macros (used throughout the code)
#define DEBUG_LOG_FLUSH(msg)    // Empty for now
#define DEBUG_LOG_INFO(msg)     // Empty for now  
#define DEBUG_LOG_ERROR(msg)    // Empty for now
#define DEBUG_LOG_WARN(msg)     // Empty for now
#define DEBUG_LOG_ISR(msg)      // Empty for now

// Debug initialization
void debug_init(void);

// Debug print functions  
void debug_print_dec(uint32_t value);
void debug_print_hex(uint8_t value);
void debug_print_float(float value, uint8_t decimals);
void debug_print_string(const char* str);

#endif /* SYSTEM_DEBUG_H */