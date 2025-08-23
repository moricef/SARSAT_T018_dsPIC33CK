#ifndef INCLUDES_H
#define INCLUDES_H

// MUST DEFINE FAMILY BEFORE INCLUDING <xc.h>
#define __dsPIC33CK__

// Define frequencies
#define FOSC 200000000UL
#define FCY (FOSC / 2)
#define _XTAL_FREQ FOSC

#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libpic30.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>

// Clock Configuration for T018 (100MHz) - dsPIC33CK specific
#pragma config FNOSC = FRC          // Fast RC Oscillator
#pragma config FCKSM = CSECMD       // Clock switching enabled
#pragma config IESO = OFF           // Two-Speed Start-up disabled
#pragma config FWDTEN = 0         // Watchdog timer disabled
#pragma config POSCMD = NONE       // Primary oscillator disabled
#pragma config OSCIOFNC = ON        // OSC2 as digital I/O
#pragma config ICS = PGD3           // ICD Communication Channel Select
#pragma config JTAGEN = OFF         // JTAG Enable bit

#endif /* INCLUDES_H */