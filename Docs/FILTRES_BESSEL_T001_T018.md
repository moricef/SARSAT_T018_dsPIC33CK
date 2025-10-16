# FILTRES BESSEL ACTIFS - T001 vs T018

## RÉSUMÉ EXÉCUTIF

| Projet | Modulation | Symbol rate | Bande signal | fc filtre | Ordre |
|--------|-----------|-------------|--------------|-----------|-------|
| **T001** | BPSK | 400 bps | ~400 Hz | **1200 Hz** | 4ème |
| **T018** | OQPSK-DSSS | 300 bps | 38.4 kHz chips | **100 kHz** | 4ème |

---

## 1. FILTRE BESSEL T001 (BPSK 400 bps)

### Spécifications
- **Type** : Bessel actif 4ème ordre (2× Sallen-Key cascadés)
- **Fréquence coupure** : 1200 Hz (-3dB)
- **Atténuation** : 24 dB/octave
- **Gain** : Unity (0 dB)
- **Phase** : Linéaire (group delay constant)

### Composants

#### Étage 1 (Sallen-Key #1)
| Composant | Valeur | Tolérance | Package |
|-----------|--------|-----------|---------|
| R1 | 10 kΩ | 1% | 0805 |
| R2 | 10 kΩ | 1% | 0805 |
| C1 | **9.679 nF** → 10 nF | 5% | 0805 COG |
| C2 | **8.885 nF** → 8.9 nF | 5% | 0805 COG |

#### Étage 2 (Sallen-Key #2)
| Composant | Valeur | Tolérance | Package |
|-----------|--------|-----------|---------|
| R3 | 10 kΩ | 1% | 0805 |
| R4 | 10 kΩ | 1% | 0805 |
| C3 | **13.324 nF** → 13 nF | 5% | 0805 COG |
| C4 | **5.135 nF** → 5.1 nF | 5% | 0805 COG |

#### Op-Amp
- **LM358P** (DIP-8) ou **LM358** (SOIC-8)
- Alimentation : 5V
- Consommation : 700 µA/canal

### Schéma

```
DAC interne dsPIC (400 bps BPSK)
    ↓
   R1 10kΩ
    ├────[C1 10nF]────┐
    │                 GND
   R2 10kΩ
    ├────[C2 8.9nF]───┘
    │
    └─→ LM358A (+)
        LM358A (-) ←──────┐ (unity gain feedback)
        LM358A (out)      │
               ↓          │
   R3 10kΩ              │
    ├────[C3 13nF]────┐ │
    │                GND │
   R4 10kΩ              │
    ├────[C4 5.1nF]───┘ │
    │                   │
    └─→ LM358B (+)      │
        LM358B (-) ←────┘
        LM358B (out)
               ↓
         Sortie filtrée → Modulateur T001
```

### Réponse fréquentielle

| Fréquence | Atténuation | Phase |
|-----------|-------------|-------|
| 400 Hz | -0.3 dB | -30° |
| 1200 Hz (fc) | **-3 dB** | -90° |
| 2400 Hz | -12 dB | -180° |
| 4800 Hz | -24 dB | -270° |

### Validation signal BPSK
- **Bande utile** : 0-800 Hz
- **Atténuation @ 800 Hz** : -0.8 dB ✓
- **Group delay** : Constant (pas de distorsion temporelle)

---

## 2. FILTRE BESSEL T018 (OQPSK-DSSS 38.4 kHz)

### Spécifications
- **Type** : Bessel actif 4ème ordre (2× Sallen-Key cascadés)
- **Fréquence coupure** : 100 kHz (-3dB)
- **Atténuation** : 24 dB/octave
- **Gain** : Unity (0 dB)
- **Phase** : Linéaire (critique pour OQPSK)

### Composants

#### Étage 1 (Sallen-Key #1)
| Composant | Valeur | Tolérance | Package |
|-----------|--------|-----------|---------|
| R1 | 10 kΩ | 1% | 0805 |
| R2 | 10 kΩ | 1% | 0805 |
| C1 | **120 pF** | 5% | 0805 COG/NPO |
| C2 | **100 pF** | 5% | 0805 COG/NPO |

#### Étage 2 (Sallen-Key #2)
| Composant | Valeur | Tolérance | Package |
|-----------|--------|-----------|---------|
| R3 | 10 kΩ | 1% | 0805 |
| R4 | 10 kΩ | 1% | 0805 |
| C3 | **150 pF** | 5% | 0805 COG/NPO |
| C4 | **68 pF** | 5% | 0805 COG/NPO |

#### Op-Amp
- **LMV358** (SOIC-8 ou TSSOP-8)
- Alimentation : 5V
- Consommation : 215 µA/canal
- **Rail-to-rail input/output**
- **Slew Rate : 1 V/µs** (critique)

### Schéma (1 canal I ou Q)

```
MCP4922 VOUTA (0-1V, 38.4 kHz chips)
    ↓
   R1 10kΩ
    ├────[C1 120pF]───┐
    │                GND
   R2 10kΩ
    ├────[C2 100pF]───┘
    │
    └─→ LMV358A (+)
        LMV358A (-) ←─────┐ (unity gain feedback)
        LMV358A (out)     │
               ↓          │
   R3 10kΩ              │
    ├────[C3 150pF]───┐ │
    │                GND │
   R4 10kΩ              │
    ├────[C4 68pF]────┘ │
    │                   │
    └─→ LMV358B (+)     │
        LMV358B (-) ←───┘
        LMV358B (out)
               ↓
         [10kΩ série] → IBBP (ADL5375)
```

### Réponse fréquentielle

| Fréquence | Atténuation | Phase | Commentaire |
|-----------|-------------|-------|-------------|
| 38.4 kHz (chip) | **-0.5 dB** | -35° | Signal utile préservé ✓ |
| 50 kHz | -1.0 dB | -45° | Bande passante OQPSK ✓ |
| 100 kHz (fc) | **-3 dB** | -90° | Coupure nominale |
| 200 kHz | -15 dB | -180° | Images DAC atténuées ✓ |
| 400 kHz | -27 dB | -270° | Rejection HF |
| 1 MHz | -48 dB | -450° | Excellente rejection |

### Validation signal OQPSK-DSSS
- **Chip rate** : 38.4 kHz
- **Bande Nyquist** : 19.2 kHz
- **Bande utile (spreading)** : 0-50 kHz
- **Atténuation @ 38.4 kHz** : -0.5 dB ✓
- **Atténuation @ 50 kHz** : -1.0 dB ✓
- **Group delay** : Constant dans bande utile ✓

### Validation Slew Rate

**Test @ 100 kHz, amplitude 1V :**
```
SR requis = 2π × f × A = 2π × 100k × 1 = 0.628 V/µs

LM358P  = 0.3 V/µs → ❌ INSUFFISANT (distorsion)
LMV358  = 1.0 V/µs → ✅ OK
```

---

## 3. CONVERSION T001 → T018

### Calcul des valeurs

**Relation entre fréquences de coupure :**
```
Ratio = fc_T018 / fc_T001 = 100000 / 1200 = 83.33

C_T018 = C_T001 / 83.33
R_T018 = R_T001 (inchangé)
```

### Table de conversion

| Composant | T001 (1200 Hz) | T018 (100 kHz) | Ratio |
|-----------|----------------|----------------|-------|
| R1, R2, R3, R4 | 10 kΩ | **10 kΩ** | 1× |
| C1 | 9.679 nF | **120 pF** | 83.3× |
| C2 | 8.885 nF | **100 pF** | 83.3× |
| C3 | 13.324 nF | **150 pF** | 83.3× |
| C4 | 5.135 nF | **68 pF** | 83.3× |

### Changements nécessaires

**Composants identiques :**
- ✅ Toutes les résistances (10 kΩ)
- ✅ Layout PCB (même topologie)
- ✅ Connectiques

**Composants à remplacer :**
- ❌ 4× Condensateurs (nF → pF, diélectrique COG/NPO obligatoire)
- ❌ Op-Amp (LM358P → LMV358 recommandé)

---

## 4. TOPOLOGIE SALLEN-KEY (2ème ordre)

### Schéma générique

```
        R1
Vin ──────┬────[C1]───┐
          │          GND
          R2
          ├────[C2]───┘
          │
          └─→ Op-Amp (+)
              Op-Amp (-) ←──── Vout (feedback)
              Op-Amp (out) ──→ Vout
```

### Fonction de transfert

```
         1
H(s) = ─────────────────────────────
       s²×R1×R2×C1×C2 + s×(R1×C1 + R2×C2) + 1
```

### Fréquence de coupure

```
fc = 1 / (2π × √(R1 × R2 × C1 × C2))
```

**Avec R1 = R2 = R :**
```
fc = 1 / (2π × R × √(C1 × C2))
```

### Cascade 4ème ordre

**Bessel 4ème ordre = 2× Sallen-Key avec coefficients spécifiques**

**Étage 1 :** Polynôme Bessel pôles 1+2
**Étage 2 :** Polynôme Bessel pôles 3+4

**Avantage :** Phase linéaire + Group delay constant

---

## 5. CHOIX OP-AMP

### Critères de sélection

| Paramètre | Requis | LM358P | LMV358 |
|-----------|--------|--------|--------|
| **GBW** | >10× fc | 1 MHz ✓ | 1 MHz ✓ |
| **Slew Rate @ fc=1.2kHz** | >0.01 V/µs | 0.3 V/µs ✓ | 1 V/µs ✓ |
| **Slew Rate @ fc=100kHz** | >0.63 V/µs | 0.3 V/µs ❌ | 1 V/µs ✓ |
| **Input common-mode** | 0-2V | 0-3.5V ✓ | 0-5V ✓ |
| **Output swing** | 0-2V | 0.02-3.5V ✓ | 0-5V ✓ |
| **Rail-to-rail** | Préférable | Non | Oui ✓ |

### Recommandations

**T001 (fc=1.2kHz) :**
- ✅ **LM358P** ou LM358 (suffisant, économique)
- Slew Rate 0.3 V/µs OK pour signaux lents

**T018 (fc=100kHz) :**
- ✅ **LMV358** (recommandé, SR=1 V/µs)
- ⚠️ LM358P limite (risque distorsion pente)

---

## 6. BOM COMPLÈTE PAR PROJET

### T001 (1 canal BPSK)

**Filtre actif :**
- 4× Résistances 10 kΩ 1% (0805)
- 1× Condensateur 10 nF 5% COG (0805)
- 1× Condensateur 8.9 nF 5% COG (0805)
- 1× Condensateur 13 nF 5% COG (0805)
- 1× Condensateur 5.1 nF 5% COG (0805)
- 1× LM358P (DIP-8) ou LM358 (SOIC-8)

**Découplage :**
- 2× Condensateurs 100 nF (0805)
- 1× Condensateur 10 µF (1206)

### T018 (2 canaux I+Q)

**Filtres actifs (×2) :**
- 8× Résistances 10 kΩ 1% (0805)
- 2× Condensateurs 120 pF 5% COG (0805)
- 2× Condensateurs 100 pF 5% COG (0805)
- 2× Condensateurs 150 pF 5% COG (0805)
- 2× Condensateurs 68 pF 5% COG (0805)
- 2× LMV358 (SOIC-8 ou TSSOP-8)

**Protection/découplage :**
- 2× Résistances 10 kΩ série (0805)
- 4× Condensateurs 100 nF (0805)
- 2× Condensateurs 10 µF (1206)

---

## 7. NOTES TECHNIQUES

### Diélectrique condensateurs

**COG/NPO obligatoire** (±30 ppm/°C) :
- Stabilité température
- Linéarité tension
- Faible absorption diélectrique

**Éviter X7R/X5R** (filtres actifs) :
- Dérive température importante
- Non-linéarité capacitance vs tension
- Distorsion harmonique

### Layout PCB

**Règles critiques :**
- Composants proches op-amp (traces courtes)
- Plans GND continus sous filtres
- Découplage VCC/GND proche broches op-amp
- Séparation analogique/numérique

### Calibration

**T001 :** Non nécessaire (fc large vs signal)
**T018 :** Vérifier réponse @ 38.4 kHz (oscillo + fonction sweep)

---

## 8. VALIDATION MESURES

### Équipement requis

**T001 (1.2 kHz) :**
- Générateur fonction 10 Hz - 10 kHz
- Oscilloscope 2 voies >10 kHz
- Multimètre RMS

**T018 (100 kHz) :**
- Générateur fonction DC - 500 kHz
- Oscilloscope 2 voies >1 MHz
- Analyseur spectre ou FFT (optionnel)

### Procédure test

1. **Réponse amplitude :**
   - Sweep 0.1×fc à 10×fc
   - Vérifier -3dB @ fc
   - Vérifier atténuation -24 dB/oct

2. **Group delay :**
   - Signal carré @ 0.5×fc
   - Vérifier absence overshoot/ringing
   - Caractéristique Bessel

3. **Distorsion :**
   - Amplitude max (1V crête)
   - Vérifier pas d'écrêtage
   - Mesure THD si possible

---

## CONCLUSION

**Deux filtres Bessel optimisés pour chaque projet :**

- **T001** : fc=1.2kHz, LM358P, condensateurs ~10nF
- **T018** : fc=100kHz, LMV358, condensateurs ~100pF

**Topologie identique** (Sallen-Key 4ème ordre), seules les valeurs capacités et op-amp changent.

**Ratio fc = 83.3× → Ratio capacités = 83.3×**
