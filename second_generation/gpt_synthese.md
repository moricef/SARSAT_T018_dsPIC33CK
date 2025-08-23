# COSPAS-SARSAT Second Generation Beacon (SGB) – Technical Reference

## 1. Scope

This document provides the technical information required to implement binary frame generation for Second Generation Beacons (SGB) according to COSPAS-SARSAT C/S T.018 Issue 2 (October 2024).  
It is intended for bare-metal C implementations (e.g. dsPIC33CK).  

Non-technical aspects (administrative procedures, environmental resistance, packaging, certification details) are excluded.  
The objective is to give all information needed to generate valid 406 MHz SGB frames.

---

## 2. Frame structure (250 bits)

### 2.1 Overview

Each transmitted burst contains **250 bits**, numbered from 1 to 250.  
Bit numbering is **MSB-first**, i.e. bit 1 = MSB of TAC.

The frame is divided into three fields:

- **Main field**: bits 1–154 (fixed, present in every burst).  
- **Rotating field**: bits 155–202 (48 bits, varies according to beacon capabilities and rotation cycle).  
- **BCH parity**: bits 203–250 (48 bits, computed from bits 1–202).

### 2.2 Transmission rules

- **Odd/even split**:  
  - Odd-numbered bits → I channel.  
  - Even-numbered bits → Q channel.  

- **Modulation**: OQPSK with DSSS spreading.  
  - Chip rate: 38.4 kcps.  
  - Spreading factor: 256 chips per bit.  

- **Preamble**: 166.7 ms of zero bits at the beginning of each burst.  

- **Burst duration**:  
  - Preamble + 250 bits payload = 1.0 s (±1 ms).

---

## 3. Main field (bits 1–154)

The main field carries beacon identification and encoded GNSS position.  
It is present in **every burst**.

### 3.1 Bit allocation

- **Bits 1–16 (16b)**: **Type Approval Certificate (TAC)**  
  - 16-bit integer, MSB transmitted first.  
  - Range: 0…65535.  
  - Upper values reserved for system beacons.

- **Bits 17–30 (14b)**: **Serial number**  
  - Range: 0…16383.  
  - Assigned by manufacturer.

- **Bits 31–40 (10b)**: **Country code (ITU MID)**  
  - Range: 0…999.  
  - Encoded as 10-bit binary, MSB at bit 31.

- **Bit 41 (1b)**: **Homing present/active flag**  
  - 0 = no active homing function.  
  - 1 = at least one homing transmitter present and active.

- **Bit 42 (1b)**: **Return Link Service (RLS) flag**  
  - 0 = no RLS capability.  
  - 1 = RLS enabled (Type-1, Type-2 or Type-3 TWC).

- **Bit 43 (1b)**: **Test protocol flag**  
  - 0 = operational.  
  - 1 = test message (non-operational).

- **Bits 44–66 (23b)**: **Latitude**  
  - Bit 44 = sign: 0 = North, 1 = South.  
  - Bits 45–51 = degrees (7 bits, 0–90).  
  - Bits 52–66 = fractional part of degree (15 bits).  
    - Calculation: `lat_frac = round((|φ| – degrees) × 2^15)`.  
    - Resolution ≈ 3.4 m.  
  - Default pattern if GNSS encoding unavailable:  
    - Binary: `1 1111111 000001111100000`.

- **Bits 67–90 (24b)**: **Longitude**  
  - Bit 67 = sign: 0 = East, 1 = West.  
  - Bits 68–75 = degrees (8 bits, 0–180).  
  - Bits 76–90 = fractional part of degree (15 bits).  
    - Calculation: `lon_frac = round((|λ| – degrees) × 2^15)`.  
    - Resolution ≈ 3.4 m.  
  - Default pattern if GNSS encoding unavailable:  
    - Binary: `1 11111111 111110000011111`.

- **Bits 91–93 (3b)**: **Identification type**  
  - 000 = no ID (bits 94–137 = 0).  
  - 001 = MMSI.  
  - 010 = Radio callsign (modified-Baudot).  
  - 011 = Aircraft registration (modified-Baudot).  
  - 100 = 24-bit aircraft address (with optional 3LD).  
  - 101 = Operator designator (3LD) + serial number.  
  - 110 = spare.  
  - 111 = reserved for system use.

- **Bits 94–137 (44b)**: **Identification content**  
  - Depends on type selected in bits 91–93.  
  - **MMSI (type 001)**: 9 decimal digits encoded in binary. Default: `000111111`.  
  - **Radio callsign (type 010)**: 7 characters, modified-Baudot. Left-justified, pad with “space” (100100). Bits 136–137 = 00.  
  - **Aircraft registration (type 011)**: 7 characters, modified-Baudot. Right-justified, pad with spaces on the left. Bits 136–137 = 00.  
  - **Aircraft 24-bit address (type 100)**:  
    - Option A: 24-bit ICAO address + 20 spare bits = 0.  
    - Option B: 24-bit ICAO address + 3LD (15b in short-Baudot) + 5 spares = 0. If 3LD not available, use “ZGA”.  
  - **Operator designator (type 101)**: 3LD (15b short-Baudot) + 12-bit serial number + 17 spares = 1.

- **Bits 138–140 (3b)**: **Beacon type**  
  - 000 = ELT (not ELT(DT)).  
  - 001 = EPIRB.  
  - 010 = PLB.  
  - 011 = ELT(DT).  
  - 111 = System beacon.  
  - 100, 101, 110 = spares.

- **Bits 141–154 (14b)**: **Spare bits**  
  - Normal = all “1”.  
  - Cancellation message = all “0”.

### 3.2 Implementation notes

- GNSS encoding: always **round** fractional degrees to 15-bit resolution, never truncate.  
- Default patterns must be inserted exactly as specified when GNSS is not available.  
- Modified-Baudot encoding:  
  - Callsign = left-justified, padded with space.  
  - Aircraft registration = right-justified, padded with space on the left.  
- Space character = `100100` in modified-Baudot.  
- Spare bits (141–154) must be all 1, except in cancellation where they must be 0.


## 4. Rotating field (bits 155–202)

### 4.1 General concept

The rotating field is a 48-bit block appended to the main field.  
It provides additional information that cannot be fully accommodated in the 154-bit main field.  
Unlike the main field, which is constant in every burst, the rotating field content **changes according to a predefined rotation cycle**.  
A complete set of information may require several consecutive bursts to be transmitted.

If the beacon has no Return Link Service (RLS) capability, the rotating field shall always be of **Type 0**.  
If the beacon supports RLS, the rotating field alternates between **Type 0** and an RLS-specific type (Type 2 or Type 4).  
For ELT(DT) beacons, Type 1 is included in the rotation.  

### 4.2 Rotating field types

The 48-bit rotating field can be of the following types:

- **Type 0 (default, mandatory)**  
  This type is always present and is recommended for all beacon categories.  
  It includes information about activation time, last known position time, altitude, DOP, activation mode, battery, and GNSS status.

- **Type 1**  
  Used by ELT(DT) in-flight emergency mode.  

- **Type 2**  
  RLS Type 1 and Type 2 service (Return Link acknowledgement).  

- **Type 3**  
  Reserved for national use.  

- **Type 4**  
  RLS Type 3 (Two-way communication).  

- **Type 15**  
  Cancellation message (in conjunction with main field spares = 0).

### 4.3 Type 0 rotating field structure

The 48 bits of Type 0 are allocated as follows:

- **Bits 155–160 (6 bits)**: **Time since activation**  
  - Encodes the elapsed time since beacon activation.  
  - Resolution: 30 minutes per step.  
  - Range: 0 to 186 (maximum encoded ≈ 93 hours).  
  - Value 111111 indicates “time not available”.

- **Bits 161–171 (11 bits)**: **Time since last encoded location**  
  - Encodes the elapsed time since the GNSS position contained in the main field was valid.  
  - Resolution: 4 minutes per step.  
  - Range: 0 to 2047 (maximum encoded ≈ 136 hours).  
  - Value 11111111111 indicates “time not available”.

- **Bits 172–181 (10 bits)**: **Altitude**  
  - Encoded relative to the WGS-84 ellipsoid.  
  - Step size = 16 m.  
  - Mapping:  
    - 0000000000 = altitude ≤ −400 m.  
    - 0000000001 = −384 m.  
    - 0000011001 = 0 m.  
    - 1111111110 = altitude > 15 952 m.  
    - 1111111111 = altitude not available.  
  - All other values correspond to altitude in 16 m increments, rounded to the nearest step.

- **Bits 182–189 (8 bits)**: **DOP (Dilution of Precision)**  
  - 4 bits HDOP class + 4 bits VDOP class.  
  - Each nibble encodes the quality class according to predefined thresholds.  
  - 1111 indicates “not available”.

- **Bits 190–191 (2 bits)**: **Activation mode**  
  - 00 = manual activation.  
  - 01 = automatic activation.  
  - 10 = both automatic and manual possible.  
  - 11 = reserved.

- **Bits 192–194 (3 bits)**: **Battery capacity**  
  - Encodes remaining capacity class.  
  - Mapping according to specification (class thresholds defined in annex).  
  - 111 = unknown/not available.

- **Bits 195–196 (2 bits)**: **GNSS status**  
  - 00 = no fix.  
  - 01 = 2D fix.  
  - 10 = 3D fix.  
  - 11 = reserved.

- **Bits 197–202 (6 bits)**: **Spare**  
  - All bits set to “1”.

### 4.4 Notes for implementation

- If GNSS data is not available, **time since last encoded location** and **altitude** shall be set to their “not available” codes.  
- Altitude must be rounded to the nearest 16 m before encoding.  
- DOP values shall be categorized into classes; if unavailable, code as “1111”.  
- Spare bits (197–202) must always be set to all “1”.  


