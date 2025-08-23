# Mapping MATLAB ↔ T018 Implementation

Ce document établit les correspondances entre les algorithmes MATLAB de référence et l'implémentation T018 en C pour dsPIC33CK.

## 📋 Vue d'ensemble des correspondances

| Fonction MATLAB | Fichier T018 | Fonction C | Status |
|-----------------|--------------|------------|---------|
| `dsss_transmitter.m` | `system_comms.c` | `oqpsk_*` | ✅ Implémenté |
| `generatePRNSequences()` | `system_comms.c` | `generate_prn_*` | ✅ Implémenté |
| `bchEncode()` | `error_correction.c` | `bch_encode()` | ✅ Implémenté |
| `oqpskModulate()` | `rf_interface.c` | `mcp4922_write_both()` | ✅ Implémenté |
| `preambleDetection()` | - | - | ⚠️ Récepteur non impl. |
| `dsss_despreading()` | - | - | ⚠️ Récepteur non impl. |

## 🏗️ Architecture système

### MATLAB Reference Chain
```
Message → BCH Encode → DSSS Spread → OQPSK Mod → RF Output
                            ↓
                    PRN I/Q Sequences
```

### T018 Implementation Chain  
```
protocol_data.c → error_correction.c → system_comms.c → rf_interface.c
      ↓                 ↓                    ↓               ↓
   Trame T.018      BCH(250,202)        OQPSK I/Q       MCP4922 DAC
```

## 🔀 Correspondances détaillées

### 1. Génération de séquences PRN

**MATLAB:**
```matlab
function DSSS = generatePRNSequences()
    initialState = [1 zeros(1, 22)];
    DSSS.PRN_I = generateLFSR(initialState, [23 18], 256*300);
    shiftedState = circshift(initialState, 64);
    DSSS.PRN_Q = generateLFSR(shiftedState, [23 18], 256*300);
end
```

**T018 C:**
```c
// system_comms.c:148
void generate_prn_sequences(void) {
    uint32_t lfsr_state = 0x1;  // Initial state
    for(uint32_t i = 0; i < PRN_LENGTH; i++) {
        prn_i_sequence[i] = (lfsr_state & 0x1);
        uint32_t feedback = ((lfsr_state >> 22) ^ (lfsr_state >> 17)) & 0x1;
        lfsr_state = ((lfsr_state << 1) | feedback) & 0x7FFFFF;
    }
    // Q sequence with 64-chip offset
    generate_prn_q_with_offset(64);
}
```

### 2. Construction du message

**MATLAB:**
```matlab
function message = generateTestMessage()
    hexID = [1 0 1 0 ...];        % 43 bits
    position = [0 1 0 1 ...];     % 47 bits  
    vesselID = [1 1 0 0 ...];     % 47 bits
    beaconType = [0 1 0 1 ...];   % 17 bits
    rotatingField = [0 0 0 0 ...]; % 48 bits
    message = [hexID position vesselID beaconType rotatingField];
end
```

**T018 C:**
```c  
// protocol_data.c:156
void build_complete_frame_2g(uint8_t* frame_bits) {
    uint8_t info_bits[INFO_BITS_LENGTH];
    
    // 23-HEX ID (43 bits)
    set_bit_field(info_bits, 0, 43, beacon_id_23hex);
    
    // GPS position (47 bits)
    encode_gps_position_2g(info_bits, 43, gps_latitude, gps_longitude);
    
    // Vessel ID (47 bits) 
    set_bit_field_64(info_bits, 90, 47, vessel_id);
    
    // Beacon type + rotating field
    set_beacon_type_2g(info_bits, 137);
    build_rotating_field_2g(info_bits, 154);
    
    // BCH encoding
    bch_encode(info_bits, frame_bits);
}
```

### 3. Encodage BCH

**MATLAB:**
```matlab
function encoded = bchEncode(message)
    % BCH(255,207) shortened to (250,202)
    parity = zeros(1, 48);
    for i = 1:48
        parityBit = 0;
        for j = 1:202
            if mod(i*j, 7) < 3
                parityBit = xor(parityBit, message(j));
            end
        end
        parity(i) = parityBit;
    end
    encoded = [message parity];
end
```

**T018 C:**
```c
// error_correction.c:78
void bch_encode(const uint8_t* data_bits, uint8_t* encoded_frame) {
    uint8_t parity_bits[BCH_PARITY_BITS];
    
    // Copy information bits
    memcpy(encoded_frame, data_bits, INFO_BITS_LENGTH);
    
    // Calculate BCH parity
    memset(parity_bits, 0, BCH_PARITY_BITS);
    for(uint16_t i = 0; i < INFO_BITS_LENGTH * 8; i++) {
        if(get_bit_field(data_bits, i, 1)) {
            bch_multiply_polynomial(parity_bits, i);
        }
    }
    
    // Append parity bits  
    for(uint8_t i = 0; i < BCH_PARITY_BITS; i++) {
        set_bit_field(encoded_frame, INFO_BITS_LENGTH * 8 + i, 1, 
                     get_bit_field(parity_bits, i, 1));
    }
}
```

### 4. Modulation OQPSK

**MATLAB:**
```matlab
function symbols = oqpskModulate(spreadI, spreadQ)
    bitsI = 2*spreadI - 1;  % 0->-1, 1->+1
    bitsQ = 2*spreadQ - 1;
    delayedQ = [0 bitsQ(1:end-1)];  % Q delay
    symbols = complex(bitsI, delayedQ);
end
```

**T018 C:**
```c
// system_comms.c:267
void oqpsk_transmit_chip(uint8_t i_chip, uint8_t q_chip) {
    static uint8_t prev_q_chip = 0;  // Q channel delay
    
    // Convert chips to bipolar
    int16_t i_val = i_chip ? 1 : -1;
    int16_t q_val = prev_q_chip ? 1 : -1;  // Use previous Q
    
    // Scale to DAC range and output I/Q
    uint16_t i_dac = 2048 + (i_val * 1000);
    uint16_t q_dac = 2048 + (q_val * 1000);
    
    mcp4922_write_both(i_dac, q_dac);
    
    prev_q_chip = q_chip;  // Store for next symbol
}
```

### 5. Interface RF

**MATLAB:**
```matlab
function oversampled = oversample(symbols, sps)
    oversampled = zeros(1, length(symbols) * sps);
    for i = 1:length(symbols)
        startIdx = (i-1) * sps + 1;
        endIdx = i * sps;
        oversampled(startIdx:endIdx) = symbols(i);
    end
end
```

**T018 C:**
```c
// rf_interface.c:100
void mcp4922_write_both(uint16_t i_value, uint16_t q_value) {
    // Apply calibration if available
    if(rf_calibration.calibrated) {
        rf_apply_calibration(&i_value, &q_value);
    }
    
    mcp4922_write_dac_a(i_value);  // I channel
    mcp4922_write_dac_b(q_value);  // Q channel
}

void output_iq_chip(int8_t i_chip, int8_t q_chip) {
    uint16_t i_dac = (uint16_t)(2048 + i_chip * 1000);
    uint16_t q_dac = (uint16_t)(2048 + q_chip * 1000);
    mcp4922_write_both(i_dac, q_dac);
}
```

## ⚙️ Paramètres système

### MATLAB System Structure
```matlab
system.fc = 406.05;                    % MHz
system.chipRate = 38400;               % chips/s  
system.symbolRate = 300;               % symbols/s
system.spreadingFactor = 256;
system.preambleLength = 50;            % symbols
system.packetLength = 300;             % bits
```

### T018 Constants
```c
// system_definitions.h
#define CHIP_RATE           38400       // chips per second
#define SYMBOL_RATE         300         // symbols per second
#define SPREADING_FACTOR    256         // chips per symbol
#define PREAMBLE_LENGTH     50          // symbols  
#define PACKET_LENGTH       300         // bits
#define FCY                 100000000UL // 100MHz
```

## 🔄 Adaptations nécessaires

### 1. Timing précis
**Problème :** MATLAB utilise des échantillons flottants, T018 doit générer timing précis

**Solution T018 :**
```c
// Timer pour 38.4 kchips/s précis
void timer_init_chip_clock(void) {
    PR2 = (FCY / CHIP_RATE) - 1;  // 2604 pour 38.4kHz
    T2CONbits.TON = 1;
}

void __attribute__((__interrupt__)) _T2Interrupt(void) {
    oqpsk_output_next_chip();  // Output I/Q chip
    IFS0bits.T2IF = 0;
}
```

### 2. Ressources mémoire
**Problème :** MATLAB utilise arrays dynamiques, dsPIC33CK a 2KB RAM

**Solution T018 :**
```c
// Buffers optimisés
static uint8_t prn_i_sequence[PRN_BUFFER_SIZE];  // 256 chips max
static uint8_t current_frame[FRAME_SIZE_BYTES];   // 38 bytes pour 300 bits
static uint8_t chip_buffer[CHIP_BUFFER_SIZE];     // Buffer circulaire
```

### 3. Calculs temps réel
**Problème :** MATLAB traite offline, T018 doit streamer en continu

**Solution T018 :**
```c
// Pipeline optimisé
typedef enum {
    TX_IDLE, TX_PREAMBLE, TX_DATA, TX_BCH
} tx_state_t;

void oqpsk_state_machine(void) {
    switch(tx_state) {
        case TX_PREAMBLE:
            output_preamble_chip();
            break;
        case TX_DATA:  
            output_spread_data_chip();
            break;
        case TX_BCH:
            output_bch_parity_chip();
            break;
    }
}
```

## 📊 Validation croisée

### Tests de correspondance
1. **PRN sequences** : Comparer `prn_i_sequence[0:255]` avec `DSSS.PRN_I(1:256)`
2. **BCH encoding** : Vérifier parité identique entre MATLAB et T018
3. **OQPSK output** : Analyser sortie MCP4922 vs symboles MATLAB
4. **Timing** : Mesurer chip rate réel vs 38.4kHz théorique

### Métriques de validation
- **Erreur PRN** : < 0.1% différence
- **Erreur BCH** : Parité exacte  
- **Erreur fréquence** : ±10 Hz sur 38.4kHz
- **Erreur phase I/Q** : < 5° décalage

## 🎯 Prochaines étapes

### Implémentation manquante  
- [ ] Récepteur DSSS (détection préambule, sync, désétalement)
- [ ] Interface GPS temps réel
- [ ] Calibration RF automatique
- [ ] Tests conformité T.018 complets

### Optimisations
- [ ] DMA pour génération I/Q haute vitesse  
- [ ] Buffer circulaire optimisé
- [ ] Réduction consommation modes sleep
- [ ] Interface SPI accélérée

---
*Mapping généré à partir de l'analyse comparative MATLAB/T018*