# ANALYSE COMPATIBILITÉ ADL5375 / MCP4922

## RÉSUMÉ EXÉCUTIF

**Conclusion : 100% COMPATIBLES**

- **MCP4922** : Sortie TENSION (0 to VREF×Gain)
- **ADL5375** : Entrée TENSION (impédance 60-100 kΩ)

---

## 1. MCP4922 : CARACTÉRISTIQUES

### Type de sortie
**DAC à sortie TENSION** (datasheet page 1)

```
"MCP4922: Dual 12-Bit Voltage Output DAC"
```

### Spécifications électriques
| Paramètre | Valeur | Unité |
|-----------|--------|-------|
| Output Swing | 0.01 to VDD-0.04 | V |
| VREF range | 0 to VDD | V |
| Gain | 1× ou 2× (sélectionnable) | — |
| Output impedance | Basse (buffer op-amp) | — |
| Short circuit current | 15-24 | mA |
| Settling time | 4.5 | µs |

### Architecture interne
```
Code digital → String résistif → Buffer Op-Amp → VOUT
```

### Équation de sortie
```
VOUT = (Code / 4095) × VREF × Gain
```

**Exemples :**
- VREF = 2.048V, Gain = 1×, Code = 2048 → VOUT = 1.024V
- VREF = 1.0V, Gain = 1×, Code = 2048 → VOUT = 0.500V

---

## 2. ADL5375 : CARACTÉRISTIQUES

### Type d'entrée
**Entrées TENSION haute impédance** (datasheet page 19)

```
"The voltages applied to these pins drive the V-to-I stage
that converts baseband voltages into currents"
```

### Spécifications électriques
| Paramètre | Symbole | Valeur | Unité |
|-----------|---------|--------|-------|
| Differential Input Impedance | — | 60-100 | kΩ |
| Input Bias Current | IB | 41 (05) / 32 (15) | µA |
| I/Q Input Bias Level | — | 500 (05) / 1500 (15) | mV |
| Baseband amplitude | — | 1 V p-p differential | — |

### Architecture interne
```
VBBP/VBBN (tension) → V-to-I converter → Courant mixers → RFOUT
```

### Version ADL5375-05 (recommandée T018)
- **Bias DC requis** : 500 mV
- **Swing typique** : ±250 mV (500 mV p-p single-ended)
- **Plage différentielle** : 1 V p-p

---

## 3. INTERFACE MCP4922 → ADL5375

### Configuration direct (single-ended to differential)

```
         MCP4922              ADL5375-05
         VREF = 1.0V
         Gain = 1×

VOUTA (0-1V) ──────────→ IBBP (attends 0.25-0.75V typ)
             [direct]
                         IBBN ──[100kΩ]─→ GND ou VREF/2

VOUTB (0-1V) ──────────→ QBBP (attends 0.25-0.75V typ)
                         QBBN ──[100kΩ]─→ GND ou VREF/2
```

### Génération bias 500 mV
**Code DAC pour bias DC :**
```c
// VREF = 1.0V, Gain = 1×
// Midscale (code 2048) → 500 mV
#define BIAS_500MV  2048

// Génération signaux I/Q centrés sur 500mV
uint16_t code_I = BIAS_500MV + amplitude_I;
uint16_t code_Q = BIAS_500MV + amplitude_Q;

mcp4922_write(DAC_A, code_I);  // Canal I → IBBP
mcp4922_write(DAC_B, code_Q);  // Canal Q → QBBP
```

### Alternative : Dual MCP4922 (différentiel natif)

```
MCP4922 #1:
  VOUTA → IBBP (500mV + signal)
  VOUTB → QBBP (500mV + signal)

MCP4922 #2:
  VOUTA → IBBN (500mV - signal)
  VOUTB → QBBN (500mV - signal)
```

**Code différentiel :**
```c
// Génération signaux complémentaires
mcp4922_1_write(DAC_A, BIAS_500MV + amplitude_I);  // IBBP
mcp4922_2_write(DAC_A, BIAS_500MV - amplitude_I);  // IBBN

mcp4922_1_write(DAC_B, BIAS_500MV + amplitude_Q);  // QBBP
mcp4922_2_write(DAC_B, BIAS_500MV - amplitude_Q);  // QBBN
```

---

## 4. DIFFÉRENCE AVEC TxDACs (AD9122)

### AD9122 : DAC à sortie COURANT

**Architecture TxDAC** (datasheet ADL5375 page 24) :
```
AD9122 IOUT (0-20 mA) → [50Ω to GND] → Tension (I×R=V) → ADL5375
```

**Conversion I→V obligatoire :**
- Midscale = 10 mA
- V = 10 mA × 50Ω = 500 mV ✓

### Comparaison MCP4922 vs AD9122

| Caractéristique | MCP4922 | AD9122 (TxDAC) |
|-----------------|---------|----------------|
| Type sortie | **TENSION** | **COURANT** |
| Valeur typique | 0-2V | 0-20 mA |
| Interface ADL5375 | Direct | Résistances 50Ω requises |
| Génération bias 500mV | Code midscale | 10 mA × 50Ω |
| Complexité | Simple | Moyenne |

---

## 5. RÉSISTANCES 50Ω : CLARIFICATION

### Rôle dans interface TxDAC (AD9122)
**Conversion courant → tension** (shunt to GND) :
```
IOUT (courant) → [50Ω] → GND
                   ↓
              V = I×R (tension)
```

### Rôle dans T018 actuel (pont diviseur 500mV)
**Isolation + protection** (série) :
```
Pont diviseur 500mV → [50Ω série] → IBBN/QBBN (ADL5375)
```

**Fonctions :**
- Isolation référence DC
- Protection court-circuit
- Impédance source stable

### Pas besoin avec MCP4922
**Sortie déjà en tension** → connexion directe possible

**Résistances série optionnelles (10kΩ recommandé) :**
- Protection ESD
- Filtrage HF additionnel
- Découplage charge capacitive

---

## 6. CONFIGURATION FINALE T018

### Architecture complète

```
dsPIC33CK ──[SPI]─→ MCP4922 (VREF=1V, Gain=1×)
                       ↓
                  VOUTA    VOUTB
                    ↓        ↓
                [Filtre] [Filtre]
                 Bessel   Bessel
                 100kHz   100kHz
                    ↓        ↓
                 [LMV358] [LMV358]
                    ↓        ↓
                 [10kΩ]   [10kΩ]  (série protection)
                    ↓        ↓
                  IBBP     QBBP ────→ ADL5375-05

Pont diviseur 500mV REF:
                    ↓
                 [50Ω]→ IBBN (référence différentielle)
                 [50Ω]→ QBBN
```

### BOM interface

**Composants essentiels :**
- 1× MCP4922-E/P (PDIP-14) ou MCP4922-E/SL (SOIC-14)
- 2× Filtres Bessel 100kHz (voir doc séparée)
- 2× LMV358 (dual op-amp rail-to-rail)
- 2× Résistances 10kΩ série (0805)
- 2× Condensateurs 100nF découplage (0805)
- 3× Résistances 50Ω existantes (conserver)

**Composants optionnels :**
- 1× MCP4922 additionnel (si différentiel natif souhaité)
- Condensateurs découplage VREF (10µF + 100nF)

---

## 7. VALIDATION COMPATIBILITÉ

### Impédances
| Interface | Source | Charge | Compatible |
|-----------|--------|--------|------------|
| MCP4922 → ADL5375 | Basse (buffer) | Haute (100kΩ) | ✅ OUI |
| AD9122 → 50Ω → ADL5375 | Moyenne (50Ω) | Haute (100kΩ) | ✅ OUI |

### Niveaux tension
| Signal | MCP4922 | ADL5375-05 requis | Compatible |
|--------|---------|-------------------|------------|
| Bias DC | 500mV (code 2048) | 500mV | ✅ OUI |
| Swing AC | ±500mV max | ±250mV typ | ✅ OUI (marge) |
| Plage totale | 0-1V | 0.25-0.75V typ | ✅ OUI |

### Bande passante
| Paramètre | MCP4922 | ADL5375 | Compatible |
|-----------|---------|---------|------------|
| Settling time | 4.5 µs | — | ✅ OUI |
| Bandwidth (-3dB) | — | >400 MHz | ✅ OUI |
| Slew rate (buffer) | — | 0.55 V/µs | ✅ OUI |

---

## 8. RÉFÉRENCES DATASHEET

### MCP4922
- **Page 1** : Type "Voltage Output DAC"
- **Page 2** : Block diagram (String DAC + Buffer)
- **Page 4** : Output Amplifier specs

### ADL5375
- **Page 6** : Baseband Inputs (100kΩ impedance)
- **Page 19** : V-to-I Converter description
- **Page 20** : Baseband drive requirements
- **Page 24** : DAC interface (TxDAC example)

---

## CONCLUSION

**MCP4922 et ADL5375 sont 100% compatibles.**

**Interface directe possible** avec filtrage approprié (Bessel 100kHz + LMV358).

**Pas de résistances 50Ω de conversion** nécessaires (contrairement aux TxDACs).

**Configuration recommandée :**
- VREF MCP4922 = 1.0V
- Gain = 1×
- Midscale = 500 mV (bias ADL5375-05)
- Filtrage actif avant ADL5375



Ces signaux servent à piloter les mélangeurs. Les signaux d'entrée en bande de base I/Q sont convertis en courants par les étages V vers I,
qui pilotent ensuite les deux mélangeurs. Les sorties de ces mélangeurs se combinent pour alimenter le symétriseur de sortie, qui fournit
une sortie asymétrique. La cellule de polarisation génère des courants de référence pour l'étage V vers I.

Convertisseur V/I
Les entrées différentielles en bande de base (QBBP, QBBN, IBBN et IBBP) présentent une impédance élevée. Les tensions appliquées à ces broches
pilotent l'étage V/I qui convertit les tensions en bande de base en courants. Les courants de sortie différentiels des étages V/I alimentent
chacun de leurs mélangeurs respectifs. La tension continue de mode commun aux entrées en bande de base détermine les courants dans les deux cœurs
du mélangeur. La variation de la tension de mode commun en bande de base influence le courant dans le mélangeur et affecte les performances globales
du modulateur. La tension continue recommandée pour la tension de mode commun en bande de base est de 500 mV pour l'ADL5375-05 et de 1 500 mV pour
l'ADL5375-15.
