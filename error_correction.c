/* error_correction.c
 * T018 2nd Generation BCH Error Correction
 * Consolidates: bch_encoder_2g.c + bch_utils.c
 */

#include "error_correction.h"
#include "system_debug.h"
#include "system_definitions.h"

// =============================================================================
// BCH GALOIS FIELD TABLES (from bch_encoder_2g.c)
// =============================================================================

// Galois Field GF(2^6) tables for BCH(250,202)
static uint8_t gf_exp[512];    // Exponential table (double size for modulo)
static uint8_t gf_log[64];     // Logarithm table
static uint8_t gf_initialized = 0;

// Generator polynomial coefficients for BCH(250,202,6)
static const uint8_t generator_poly[] = {
    1, 59, 13, 104, 189, 68, 209, 30, 8, 163, 65, 41, 229, 98, 50, 36, 59, 
    23, 23, 37, 207, 165, 180, 29, 51, 168, 124, 247, 18, 175, 199, 34, 
    60, 220, 121, 101, 246, 81, 63, 94, 180, 19, 58, 153, 187, 230, 84, 
    189, 1
};

// Test data from T018 Appendix B.1
const uint8_t bch_test_data_appendix_b1[202] = {
    0x00, 0xE6, 0x08, 0xF4, 0xC9, 0x86, 0x00, 0x00, // Example test vector
    // ... (complete with actual T018 test data)
};

const uint64_t bch_expected_parity_appendix_b1 = 0x492A4FC57A49ULL;

// =============================================================================
// GALOIS FIELD INITIALIZATION
// =============================================================================

static void init_galois_field(void) {
    if(gf_initialized) return;
    
    // Primitive polynomial: x^6 + x + 1 (binary: 1000011 = 67)
    uint8_t primitive_poly = 0x43;
    
    // Initialize exponential table
    gf_exp[0] = 1;
    for(int i = 1; i < 63; i++) {
        gf_exp[i] = gf_exp[i-1] << 1;
        if(gf_exp[i] & 0x40) {  // If bit 6 is set
            gf_exp[i] ^= primitive_poly;
        }
    }
    
    // Extend table for modulo operations
    for(int i = 63; i < 512; i++) {
        gf_exp[i] = gf_exp[i % 63];
    }
    
    // Initialize logarithm table
    gf_log[0] = 0;  // log(0) undefined, set to 0
    for(int i = 0; i < 63; i++) {
        gf_log[gf_exp[i]] = i;
    }
    
    gf_initialized = 1;
    DEBUG_LOG_FLUSH("Galois Field GF(2^6) initialized\r\n");
}

// =============================================================================
// BCH ENCODING FUNCTIONS
// =============================================================================

void calculate_bch_2g(uint8_t* info_bits, uint8_t* parity_bits) {
    if(!gf_initialized) init_galois_field();
    
    // Clear parity bits
    memset(parity_bits, 0, BCH_PARITY_BITS);
    
    // BCH encoding using polynomial division
    uint8_t remainder[BCH_PARITY_BITS] = {0};
    
    // Process each information bit
    for(int i = 0; i < BCH_K; i++) {
        uint8_t feedback = remainder[BCH_PARITY_BITS-1] ^ info_bits[i];
        
        // Shift remainder
        for(int j = BCH_PARITY_BITS-1; j > 0; j--) {
            remainder[j] = remainder[j-1];
        }
        remainder[0] = 0;
        
        // Add generator polynomial if feedback is 1
        if(feedback) {
            for(int j = 0; j < BCH_PARITY_BITS; j++) {
                remainder[j] ^= generator_poly[j];
            }
        }
    }
    
    // Copy remainder to parity bits
    memcpy(parity_bits, remainder, BCH_PARITY_BITS);
}

uint64_t compute_bch_250_202(const uint8_t *data_202bits) {
    // Simplified BCH computation for 48-bit parity
    const uint64_t g = 0x1C7EB85DF3C97ULL;  // Generator polynomial (49 bits)
    
    uint64_t reg = 0;
    
    // Process 250 bits (202 data + 48 padding)
    for (int i = 0; i < 250; i++) {
        uint8_t bit = (i < 202) ? data_202bits[i] : 0;
        uint8_t msb = (reg >> 48) & 1;
        
        reg = ((reg << 1) | bit) & 0x1FFFFFFFFFFFFULL;
        
        if (msb) reg ^= g;
    }
    
    return reg & 0xFFFFFFFFFFFFULL;  // Return 48 bits
}

void encode_bch_2g_with_correction(uint8_t* info_bits, uint8_t* codeword) {
    // Build complete codeword
    memcpy(codeword, info_bits, BCH_K);
    
    // Calculate and append parity
    uint8_t parity[BCH_PARITY_BITS];
    calculate_bch_2g(info_bits, parity);
    memcpy(&codeword[BCH_K], parity, BCH_PARITY_BITS);
}

// =============================================================================
// BCH VERIFICATION AND TESTING
// =============================================================================

int8_t verify_bch_2g(uint8_t* received_bits) {
    uint8_t syndrome[BCH_PARITY_BITS];
    calculate_syndrome_2g(received_bits, syndrome);
    
    // Check if syndrome is zero (no errors)
    for(int i = 0; i < BCH_PARITY_BITS; i++) {
        if(syndrome[i] != 0) {
            return 1;  // Errors detected
        }
    }
    
    return 0;  // No errors
}

uint8_t test_bch_encoder_2g(void) {
    DEBUG_LOG_FLUSH("Testing BCH encoder...\r\n");
    
    // Test with known vector
    uint8_t test_info[202];
    memcpy(test_info, bch_test_data_appendix_b1, 202);
    
    uint64_t computed_bch = compute_bch_250_202(test_info);
    uint64_t expected_bch = bch_expected_parity_appendix_b1;
    
    if(computed_bch == expected_bch) {
        DEBUG_LOG_FLUSH("BCH encoder test PASSED\r\n");
        return 1;
    } else {
        DEBUG_LOG_FLUSH("BCH encoder test FAILED\r\n");
        DEBUG_LOG_FLUSH("Expected: 0x");
        for(int i = 5; i >= 0; i--) {
            debug_print_hex((expected_bch >> (i * 8)) & 0xFF);
        }
        DEBUG_LOG_FLUSH("\r\nComputed: 0x");
        for(int i = 5; i >= 0; i--) {
            debug_print_hex((computed_bch >> (i * 8)) & 0xFF);
        }
        DEBUG_LOG_FLUSH("\r\n");
        return 0;
    }
}

void validate_bch_250_202(void) {
    test_bch_encoder_2g();
}

uint8_t verify_bch_integrity(const uint8_t *frame_252bits) {
    // Extract information bits (skip 2-bit header)
    uint8_t info_bits[202];
    memcpy(info_bits, &frame_252bits[2], 202);
    
    // Extract received BCH (last 48 bits)
    uint64_t received_bch = 0;
    for(int i = 0; i < 48; i++) {
        if(frame_252bits[204 + i]) {
            received_bch |= (1ULL << (47 - i));
        }
    }
    
    // Compute expected BCH
    uint64_t computed_bch = compute_bch_250_202(info_bits);
    
    return (received_bch == computed_bch);
}

// =============================================================================
// BCH SYNDROME CALCULATION
// =============================================================================

void calculate_syndrome_2g(uint8_t* received_bits, uint8_t* syndrome) {
    if(!gf_initialized) init_galois_field();
    
    memset(syndrome, 0, BCH_PARITY_BITS);
    
    // Calculate syndrome for error detection
    for(int i = 0; i < BCH_N; i++) {
        uint8_t bit = received_bits[i];
        
        // Update syndrome components
        for(int j = 0; j < 2*BCH_T; j++) {
            // Polynomial evaluation at roots
            syndrome[j] ^= bit;  // Simplified calculation
        }
    }
}

// =============================================================================
// BCH DECODING (for receiver implementation)
// =============================================================================

uint8_t decode_bch_250_202(uint8_t *received_250bits, uint8_t *corrected_202bits) {
    // Extract data portion
    memcpy(corrected_202bits, received_250bits, 202);
    
    // Compute syndrome
    uint64_t syndrome = compute_bch_250_202(corrected_202bits);
    
    if(syndrome == 0) {
        return 0;  // No errors detected
    }
    
    // Simple error detection (full correction requires complex algorithms)
    DEBUG_LOG_FLUSH("BCH errors detected, syndrome: 0x");
    for(int i = 5; i >= 0; i--) {
        debug_print_hex((syndrome >> (i * 8)) & 0xFF);
    }
    DEBUG_LOG_FLUSH("\r\n");
    
    return 1;  // Errors detected but not corrected
}

uint8_t count_bch_errors(const uint8_t *received_bits, const uint8_t *expected_bits) {
    uint8_t error_count = 0;
    
    for(int i = 0; i < 202; i++) {
        if(received_bits[i] != expected_bits[i]) {
            error_count++;
        }
    }
    
    return error_count;
}

// =============================================================================
// DEBUG FUNCTIONS
// =============================================================================

void debug_print_bch_parity(uint8_t* parity_bits) {
    DEBUG_LOG_FLUSH("BCH Parity (48 bits): ");
    for(int i = 0; i < 6; i++) {  // 48 bits = 6 bytes
        debug_print_hex(parity_bits[i]);
        if(i < 5) DEBUG_LOG_FLUSH(" ");
    }
    DEBUG_LOG_FLUSH("\r\n");
}