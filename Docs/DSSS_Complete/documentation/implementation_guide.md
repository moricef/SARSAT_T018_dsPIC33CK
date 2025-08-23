# Guide d'implémentation DSSS T018

Ce guide pratique détaille comment adapter les algorithmes MATLAB DSSS vers l'implémentation T018 en C pour dsPIC33CK.

## 🎯 Objectifs d'adaptation

1. **Convertir** les algorithmes MATLAB flottants vers entiers 16/32 bits
2. **Optimiser** pour contraintes temps réel (38.4 kchips/s)
3. **Minimiser** l'usage RAM (2KB total disponible)
4. **Maintenir** la précision nécessaire pour conformité T.018

## 📋 Plan d'implémentation

### Phase 1: Émetteur DSSS ✅ (Implémenté)
- [x] Génération PRN LFSR
- [x] Construction trame T.018
- [x] Encodage BCH (250,202)
- [x] Modulation OQPSK I/Q
- [x] Interface MCP4922 DAC

### Phase 2: Optimisations ⚠️ (En cours)
- [ ] Réduction usage RAM
- [ ] Timing DMA haute précision
- [ ] Calibration RF automatique
- [ ] Parser GPS complet

### Phase 3: Récepteur 📋 (Optionnel)
- [ ] Détection préambule
- [ ] Synchronisation fréquence/phase
- [ ] Désétalement DSSS
- [ ] Décodage BCH

## 🔧 Adaptations techniques

### 1. Arithmétique entière vs flottante

**Problème MATLAB :**
```matlab
% Calculs flottants
bitsI = 2*spreadI - 1;  % Génère [-1, +1]
symbols = complex(bitsI, delayedQ);
```

**Solution T018 :**
```c
// Conversion optimisée entière
static inline int16_t bit_to_bipolar(uint8_t bit) {
    return bit ? 1000 : -1000;  // Échelle DAC 12-bit
}

void oqpsk_output_chip(uint8_t i_chip, uint8_t q_chip) {
    uint16_t i_dac = 2048 + bit_to_bipolar(i_chip);  // 2048 ± 1000
    uint16_t q_dac = 2048 + bit_to_bipolar(q_chip);
    mcp4922_write_both(i_dac, q_dac);
}
```

### 2. Génération PRN temps réel

**Problème MATLAB :**
```matlab
% Génère toute la séquence en une fois
DSSS.PRN_I = generateLFSR(initialState, [23 18], 256*300);
```

**Solution T018 streaming :**
```c
// Générateur LFSR temps réel optimisé
static uint32_t lfsr_state_i = 0x1;
static uint32_t lfsr_state_q = 0x41;  // Offset pour orthogonalité

static inline uint8_t lfsr_next_bit(uint32_t* state) {
    uint8_t output = *state & 0x1;
    uint32_t feedback = ((*state >> 22) ^ (*state >> 17)) & 0x1;
    *state = ((*state << 1) | feedback) & 0x7FFFFF;  // Masque 23-bit
    return output;
}

// Appelé à 38.4kHz par timer interrupt
void generate_next_prn_chips(uint8_t* i_chip, uint8_t* q_chip) {
    *i_chip = lfsr_next_bit(&lfsr_state_i);
    *q_chip = lfsr_next_bit(&lfsr_state_q);
}
```

### 3. Pipeline émission temps réel

**Problème MATLAB :**
```matlab
% Traitement batch offline
for bitIdx = 1:numBits
    spreadChips = despreadBit(message(bitIdx), PRN, bitIdx);
    oqpskSymbols = modulate(spreadChips);
end
```

**Solution T018 state machine :**
```c
typedef struct {
    tx_state_t state;
    uint16_t bit_index;
    uint8_t chip_index;
    uint8_t current_bit;
    uint8_t frame_buffer[FRAME_SIZE_BYTES];
} tx_context_t;

// Appelé à chaque chip (38.4kHz)
void oqpsk_state_machine(void) {
    static tx_context_t ctx = {0};
    
    switch(ctx.state) {
        case TX_PREAMBLE:
            if(ctx.bit_index < PREAMBLE_LENGTH) {
                output_preamble_chip(&ctx);
            } else {
                ctx.state = TX_DATA;
                ctx.bit_index = 0;
            }
            break;
            
        case TX_DATA:
            if(ctx.bit_index < INFO_BITS_LENGTH) {
                output_data_chip(&ctx);
            } else {
                ctx.state = TX_BCH;
                ctx.bit_index = 0;
            }
            break;
            
        case TX_BCH:
            if(ctx.bit_index < BCH_PARITY_BITS) {
                output_bch_chip(&ctx);
            } else {
                ctx.state = TX_IDLE;
                transmission_complete_callback();
            }
            break;
    }
}
```

### 4. Optimisation mémoire RAM

**Problème :** dsPIC33CK = 2KB RAM total, usage actuel = 101.4%

**Stratégies d'optimisation :**

```c
// 1. Buffers circulaires au lieu de linéaires
#define CHIP_BUFFER_SIZE 64  // Au lieu de 256*300
static uint8_t chip_buffer[CHIP_BUFFER_SIZE];
static uint8_t buffer_head = 0;

// 2. Réutilisation buffers multi-usage
static union {
    uint8_t frame_bits[FRAME_SIZE_BYTES];    // 38 bytes
    uint8_t gps_buffer[GPS_BUFFER_SIZE];     // 32 bytes
    uint8_t debug_buffer[DEBUG_BUFFER_SIZE]; // 64 bytes
} shared_buffer;

// 3. Compression données temporaires  
typedef struct __attribute__((packed)) {
    uint16_t lat_encoded : 23;  // Au lieu de float
    uint16_t lon_encoded : 24;
    uint8_t  alt_code : 10;
} gps_compact_t;  // 7 bytes au lieu de 12

// 4. Élimination variables inutilisées
// Remplacer: static uint32_t unused_array[100];
// Par: Rien du tout si non utilisé
```

### 5. Timing haute précision

**Problème MATLAB :**
```matlab
% Timing parfait simulé
chipPeriod = 1/chipRate;  % 26.041666... μs
```

**Solution T018 DMA + Timer :**
```c
// Timer2 pour chip clock précis
void init_precision_timing(void) {
    // FCY=100MHz, Target=38400Hz
    PR2 = 2603;  // (100e6/38400) - 1, erreur -0.023%
    
    // DMA pour transfer I/Q zero-copy
    DMA0REQ = 0x21;        // SPI1 TX trigger
    DMA0STAL = (uint16_t)&dac_buffer_low;
    DMA0STAH = (uint16_t)&dac_buffer_high; 
    DMA0CNT = CHIP_BUFFER_SIZE - 1;
    DMA0CONbits.MODE = 0;  // Continuous, no ping-pong
    DMA0CONbits.SIZE = 0;  // Word transfer
    
    IEC0bits.DMA0IE = 1;   // Enable DMA interrupt
    T2CONbits.TON = 1;     // Start timer
}

// DMA interrupt: prépare prochains chips
void __attribute__((__interrupt__)) _DMA0Interrupt(void) {
    prepare_next_chip_batch();  // Prépare N chips suivants
    IFS0bits.DMA0IF = 0;
}
```

### 6. Interface GPS temps réel

**Problème MATLAB :**
```matlab  
% Coordonnées fixes simulées
lat = 45.1885; lon = 5.7245;
```

**Solution T018 NMEA parser :**
```c
typedef enum {
    NMEA_IDLE, NMEA_HEADER, NMEA_DATA, NMEA_CHECKSUM
} nmea_state_t;

// UART2 interrupt pour réception GPS
void __attribute__((__interrupt__)) _U2RXInterrupt(void) {
    static nmea_state_t state = NMEA_IDLE;
    static uint8_t nmea_buffer[NMEA_MAX_LENGTH];
    static uint8_t buffer_index = 0;
    
    char received = U2RXREG;
    
    switch(state) {
        case NMEA_IDLE:
            if(received == '$') {
                state = NMEA_HEADER;
                buffer_index = 0;
            }
            break;
            
        case NMEA_HEADER:
            if(received == ',') {
                if(strncmp(nmea_buffer, "GPGGA", 5) == 0) {
                    state = NMEA_DATA;
                    buffer_index = 0;
                } else {
                    state = NMEA_IDLE;  // Ignore other sentences
                }
            }
            break;
            
        case NMEA_DATA:
            if(received == '\n') {
                nmea_buffer[buffer_index] = 0;  // Null terminate
                parse_gga_sentence(nmea_buffer);
                state = NMEA_IDLE;
            } else if(buffer_index < NMEA_MAX_LENGTH-1) {
                nmea_buffer[buffer_index++] = received;
            }
            break;
    }
    
    IFS1bits.U2RXIF = 0;
}
```

## 🧪 Validation et debug

### 1. Outils de debug temps réel

```c
// Pin debug pour oscilloscope
#define DEBUG_PIN_TRIS  TRISDbits.TRISD11
#define DEBUG_PIN_LAT   LATDbits.LATD11

// Macro debug timing
#define DEBUG_PULSE() do { \
    DEBUG_PIN_LAT = 1; \
    __asm__ volatile ("nop"); \
    DEBUG_PIN_LAT = 0; \
} while(0)

// Usage dans code critique
void __attribute__((__interrupt__)) _T2Interrupt(void) {
    DEBUG_PULSE();  // Mesure sur scope
    oqpsk_output_next_chip();
    IFS0bits.T2IF = 0;
}
```

### 2. Validation croisée MATLAB

```c
// Export données T018 vers MATLAB
void debug_export_prn_sequence(void) {
    DEBUG_LOG_FLUSH("PRN_I = [");
    for(uint16_t i = 0; i < 256; i++) {
        uint8_t chip = lfsr_next_bit(&debug_lfsr_state);
        debug_print_dec(chip);
        if(i < 255) DEBUG_LOG_FLUSH(",");
    }
    DEBUG_LOG_FLUSH("];\\n");
}

// Comparaison dans MATLAB:
% prn_t018 = [1,0,1,1,0,1,0,0,1,1,1,0,...];  % Copié depuis T018
% corr = sum(DSSS.PRN_I(1:256) == prn_t018) / 256;
```

### 3. Tests unitaires embarqués

```c
// Framework test simple
typedef struct {
    const char* name;
    bool (*test_func)(void);
    bool passed;
} unit_test_t;

bool test_lfsr_period(void) {
    uint32_t state = 0x1;
    uint32_t period = 0;
    uint32_t initial = state;
    
    do {
        lfsr_next_bit(&state);
        period++;
    } while(state != initial && period < 0xFFFFFF);
    
    return period == 0x7FFFFF;  // 2^23-1 expected
}

bool test_bch_encode_decode(void) {
    uint8_t test_data[25] = {0x55, 0xAA, ...};  // Pattern test
    uint8_t encoded[32], decoded[25];
    
    bch_encode(test_data, encoded);
    int errors = bch_decode(encoded, decoded);
    
    return (errors == 0) && (memcmp(test_data, decoded, 25) == 0);
}

unit_test_t tests[] = {
    {"LFSR Period", test_lfsr_period, false},
    {"BCH Codec", test_bch_encode_decode, false},
    {NULL, NULL, false}
};

void run_unit_tests(void) {
    for(int i = 0; tests[i].name; i++) {
        tests[i].passed = tests[i].test_func();
        DEBUG_LOG_FLUSH(tests[i].name);
        DEBUG_LOG_FLUSH(tests[i].passed ? " PASS\\n" : " FAIL\\n");
    }
}
```

## 📊 Métriques de performance

### Cibles d'optimisation

| Métrique | Cible | Actuel | Status |
|----------|-------|--------|---------|
| RAM usage | < 2048 bytes | 2076 bytes | ❌ -28 bytes |
| CPU load | < 60% @ 38.4kHz | ~25% | ✅ OK |
| Timing jitter | < ±50 Hz | ±2 Hz | ✅ Excellent |
| Power (TX) | < 100 mA | 89 mA | ✅ OK |
| Code size | < 24KB | 13.4KB | ✅ OK |

### Profiling tools

```c
// Mesure CPU load
static uint32_t cpu_cycles_used = 0;
static uint32_t cpu_total_cycles = 0;

#define PROFILE_START() uint32_t start_cycle = ReadCoreTimer()
#define PROFILE_END(name) do { \
    uint32_t cycles = ReadCoreTimer() - start_cycle; \
    cpu_cycles_used += cycles; \
    DEBUG_PRINTF(#name ": %lu cycles\\n", cycles); \
} while(0)

// Usage
void critical_function(void) {
    PROFILE_START();
    // Code à mesurer
    PROFILE_END(critical_function);
}
```

## 🎯 Checklist d'implémentation

### Avant codage
- [ ] Analyser l'algorithme MATLAB correspondant
- [ ] Identifier contraintes temps réel vs précision
- [ ] Estimer usage RAM/ROM nécessaire
- [ ] Définir interfaces avec modules existants

### Pendant codage  
- [ ] Implémenter version simple d'abord
- [ ] Ajouter traces debug/profiling
- [ ] Tester avec données de référence MATLAB
- [ ] Optimiser uniquement si nécessaire

### Validation
- [ ] Tests unitaires embarqués
- [ ] Comparaison avec sorties MATLAB
- [ ] Tests stress (longue durée, température)
- [ ] Validation sur récepteur externe

## 📈 Prochaines étapes

### Optimisations prioritaires
1. **Réduction RAM** : Éliminer 28 bytes excédentaires
2. **Parser GPS** : NMEA complet au lieu de simulation
3. **Calibration automatique** : I/Q DC offset, gain matching
4. **Interface utilisateur** : LED status, configuration

### Extensions possibles
1. **Récepteur DSSS** : Détection/décodage signaux reçus
2. **Multiple PRN** : Support autres séquences d'étalement
3. **Modes avancés** : Test patterns, CW, modulation AM
4. **Interface PC** : Configuration/monitoring via USB

---
*Guide pratique pour adaptation MATLAB→T018 - Version 1.0*