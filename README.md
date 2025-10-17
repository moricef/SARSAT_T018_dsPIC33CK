# COSPAS-SARSAT T.018 - Balise 2G (dsPIC33CK)

[![T.018 Compliant](https://img.shields.io/badge/T.018-100%25%20Compliant-brightgreen)](https://cospas-sarsat.int)
[![License](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-orange)](LICENSE)

> Générateur de trames COSPAS-SARSAT 2G conforme T.018 pour formation ADRASEC sur 403 MHz

## Objectif

Balise de formation 100% conforme aux spécifications COSPAS-SARSAT T.018 Rev.12 (octobre 2024) pour :
- **Formation** opérateurs ADRASEC
- **Validation** décodeurs SARSAT
- **Exercices** SAR (Search and Rescue)
- **Tests** de conformité (hors fréquence d'urgence 406 MHz)

## Caractéristiques Techniques

### Signal T.018
- **Modulation** : OQPSK-DSSS (Direct Sequence Spread Spectrum)
- **Chip rate** : 38.400 kchips/s ±0.6 chips/s
- **Data rate** : 300 bps
- **Spreading** : 256 chips/bit (128 I + 128 Q)
- **Durée trame** : 1000 ms ±1 ms
- **Structure** : 50 bits préambule + 202 bits info + 48 bits BCH

### Hardware
- **MCU** : dsPIC33CK64MC105 @ 100 MHz (Curiosity Nano)
- **== Prévisionnel ==**
- **DAC** : MCP4922 dual 12-bit pour I/Q
- **Filtre Passe-bas** : Tchebichev 5ème ou 6èmè ordre
- **RF** : ADL5375 modulateur I/Q + ADF4351 PLL 403 MHz
- **GPS** : Trimble 63530-00 (NMEA 0183)
- **Amplificateur** : RA07M4047M (7W)

### Conformité T.018 Validée

✅ **Champ rotatif dynamique** (bits 186-202)
- LFSR 8-bit générant motif pseudo-aléatoire
- Utilisation du compteur de transmissions comme seed
- Test unitaire `test_rotating_field_compliance()`

✅ **Séquences PRN officielles** (LFSR x²³ + x¹⁸ + 1)
- États initiaux : I=0x000001, Q=0x000041
- Validation contre Table 2.2 (premiers 64 chips)
- Test unitaire `test_prn_table_2_2()`

✅ **Validation NMEA GPS**
- Checksum XOR avant parsing
- Protection contre données corrompues
- Fonction `validate_nmea_checksum()`

✅ **Encodeur BCH(250,202)**
- Correction jusqu'à 6 erreurs
- Test unitaire `test_bch_encoder_2g()`

✅ **Tests automatiques au démarrage**
- Vérification PRN, champ rotatif, BCH
- Logs détaillés de conformité

## Modes de Fonctionnement

### Mode TEST (Switch RC0 = 0)
- Position fixe : Marseille offshore (43.2°N, 5.4°E)
- Intervalle : 10 secondes
- Usage : validation décodeur

### Mode EXERCISE (Switch RC0 = 1)
- Position GPS temps réel
- Séquences ELT conformes T.018 :
  - Phase 1 : 24 tx @ 5s
  - Phase 2 : 18 tx @ 10s
  - Phase 3 : continues @ 28.5s ±1.5s
- Usage : simulation crash ELT

## Structure du Projet

```
SARSAT_T018_dsPIC33CK.X/
├── main.c                 # Logique principale + tests
├── protocol_data.c/h      # Construction trames + champ rotatif
├── system_comms.c/h       # GPS + OQPSK + PRN + validation
├── error_correction.c/h   # BCH(250,202)
├── rf_interface.c/h       # DAC MCP4922 + PLL ADF4351
├── system_hal.c/h         # HAL hardware + timers
└── Docs/                  # Documentation T.018
    ├── Correction_Complete_dsPIC33CK.html
    ├── SARSAT_T018_RotatingField_Fix.html
    └── T018_2-2-3.html
```

## Compilation

```bash
# MPLAB X IDE v6.25+ et XC-DSC v3.21
make clean && make

# Résultat
# Program : 21468 bytes (32% de 64KB)
# Data    : 2884 bytes (35% de 8KB)
# Status  : 100% conforme T.018 ✅
```

## Pinout dsPIC33CK64MC105

| Pin  | Fonction              |
|------|-----------------------|
| RB1  | ADF4351 CS (SPI)      |
| RB2  | MCP4922 CS (SPI)      |
| RB11 | RF Power Control      |
| RB15 | RF Amplifier Enable   |
| RC0  | Mode Switch (pull-up) |
| RC4  | GPS TX                |
| RC5  | GPS RX                |
| RD10 | LED Status            |


## Documentation

Voir dossier `Docs/` pour :
- Guides de correction T.018
- Spécifications techniques détaillées
- Exemples de validation

## Licence

CC BY-NC-SA 4.0 - Usage non commercial - Attribution requise

---

**Projet ADRASEC09** - Formation SAR et validation décodeurs SARSAT
Conformité T.018 Rev.12 Oct 2024 vérifiée ✅
