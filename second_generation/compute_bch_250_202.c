// =============================
// BCH (250,202) Implementation
// =============================
#include <stdint.h>

uint64_t compute_bch_250_202(const uint8_t *data_202bits) {
    // Polynôme générateur CORRECT (49 bits)
    const uint64_t g = 0x1C7EB85DF3C97ULL;  // 0b1110001111110101110000101110111110011110010010111
    
    // Registre de division (49 bits)
    uint64_t reg = 0;
    
    // Traitement des 250 bits (202 données + 48 padding)
    for (int i = 0; i < 250; i++) {
        // Bit courant (donnée ou 0 pour padding)
        uint8_t bit = (i < 202) ? data_202bits[i] : 0;
        
        // Extrait le MSB actuel (bit 48)
        uint8_t msb = (reg >> 48) & 1;
        
        // Décalage à gauche + injection du nouveau bit en LSB
        reg = ((reg << 1) | bit) & 0x1FFFFFFFFFFFFULL;
        
        // Division polynomiale (XOR si MSB = 1)
        if (msb) reg ^= g;
    }
    
    // Renvoie les 48 bits de poids faible (supprime le MSB)
    return reg & 0xFFFFFFFFFFFFULL;
}
