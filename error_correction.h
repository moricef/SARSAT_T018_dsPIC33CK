/* error_correction.h
 * T018 2nd Generation BCH Error Correction
 * Consolidates: bch_encoder_2g.h + bch_utils.h
 */

#ifndef ERROR_CORRECTION_H
#define ERROR_CORRECTION_H

#include "includes.h"
#include "system_definitions.h"

// BCH (250,202) Error Correction for T018
#define BCH_N 250           // Codeword length
#define BCH_K 202           // Information length  
#define BCH_T 6             // Error correction capability
#define BCH_PARITY_BITS 48  // Parity bits

// BCH encoder functions
void calculate_bch_2g(uint8_t* info_bits, uint8_t* parity_bits);
uint64_t compute_bch_250_202(const uint8_t *data_202bits);
void encode_bch_2g_with_correction(uint8_t* info_bits, uint8_t* codeword);

// BCH verification and testing
int8_t verify_bch_2g(uint8_t* received_bits);
uint8_t test_bch_encoder_2g(void);
void validate_bch_250_202(void);
uint8_t verify_bch_integrity(const uint8_t *frame_252bits);

// BCH syndrome calculation
void calculate_syndrome_2g(uint8_t* received_bits, uint8_t* syndrome);

// BCH decoding (receiver functions)
uint8_t decode_bch_250_202(uint8_t *received_250bits, uint8_t *corrected_202bits);
uint8_t count_bch_errors(const uint8_t *received_bits, const uint8_t *expected_bits);

// Debug functions
void debug_print_bch_parity(uint8_t* parity_bits);

// Test vectors from T018 Appendix B.1
extern const uint8_t bch_test_data_appendix_b1[202];
extern const uint64_t bch_expected_parity_appendix_b1;

#endif /* ERROR_CORRECTION_H */