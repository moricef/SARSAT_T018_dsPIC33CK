void validate_bch_250_202(void) {
    // Données de test Appendix B.1
    uint8_t test_data[202] = {...};  // 00E608F4C986...
    uint64_t expected_bch = 0x492A4FC57A49;
    
    if (compute_bch_250_202(test_data) != expected_bch) {
        DEBUG_LOG_FLUSH("BCH VALIDATION FAILED!\r\n");
    }
}